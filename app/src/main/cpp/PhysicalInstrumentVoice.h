#pragma once

// Experimental, self-contained physical instrument mechanism.
// No dependency on Spectrachord, its presets, or the production audio path.
// Fixed-capacity state; after prepare/start, render() makes no allocations.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace aiora::physical {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kMaxStringCells = 2048;
constexpr int kBodyModes = 6;

enum class Material : uint8_t { Steel, Nylon, Bronze, Wood, Glass, Ceramic, Rubber, Felt };
enum class Geometry : uint8_t { HollowBox, HollowCylinder, Plate, Bell };
enum class Excitation : uint8_t { Pluck, Hammer };

struct MaterialProperties {
    float densityKgM3;
    float youngModulusPa;
    float lossFactor;
    float contactSoftness;
};

inline constexpr MaterialProperties materialProperties(Material material) noexcept {
    switch (material) {
    case Material::Steel:   return {7850.0f, 200.0e9f, 0.0010f, 0.05f};
    case Material::Nylon:   return {1140.0f,   2.5e9f, 0.0080f, 0.30f};
    case Material::Bronze:  return {8800.0f, 105.0e9f, 0.0020f, 0.08f};
    case Material::Wood:    return { 650.0f,  11.0e9f, 0.0250f, 0.28f};
    case Material::Glass:   return {2500.0f,  70.0e9f, 0.0025f, 0.02f};
    case Material::Ceramic: return {3000.0f, 100.0e9f, 0.0020f, 0.02f};
    case Material::Rubber:  return {1100.0f,   0.01e9f, 0.1800f, 0.85f};
    case Material::Felt:    return { 350.0f,   0.003e9f,0.3000f, 0.95f};
    }
    return {7850.0f, 200.0e9f, 0.0010f, 0.05f};
}

struct Blueprint {
    Material stringMaterial{Material::Steel};
    float stringDiameterMm{0.50f};
    float referenceLengthMeters{0.65f};
    float tensionNewtons{80.0f};
    Excitation excitation{Excitation::Pluck};
    Material exciterMaterial{Material::Nylon};
    float excitationPosition{0.19f}; // 0 = bridge, 1 = nut
    float hammerHardness{0.65f}; // 0 = soft, 1 = hard
    Geometry bodyGeometry{Geometry::HollowBox};
    Material bodyMaterial{Material::Wood};
    float bodySize{1.0f}; // larger bodies resonate lower
    float bridgeCoupling{0.65f};
    Material damperMaterial{Material::Felt};
    float damperBasePosition{0.07f};
    float damperTravel{0.09f};
    float damperStrength{1.0f};
};

// Each note has its own state; length is retuned for its target MIDI pitch.
// E1 is attack/strike intensity. E2 is a continuously varying damping action:
// increasing E2 moves a soft contact toward the bridge and presses harder.
// The prototype intentionally excludes full 3D contact and radiative acoustics.
class PhysicalInstrumentVoice {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = std::clamp(sampleRate, 8000.0f, 192000.0f);
        release();
    }

    void start(const Blueprint& blueprint, int midi, float e1, float e2 = 0.0f) noexcept {
        blueprint_ = blueprint;
        e1_ = clamp01(e1);
        e2Target_ = clamp01(e2);
        e2Smoothed_ = e2Target_;
        released_ = false;
        active_ = true;
        samplesRendered_ = 0;
        releasedAt_ = 0;
        bridgeFilter_ = 0.0f;
        rHead_ = 0;
        lHead_ = 0;
        for (auto& x : right_) x = 0.0f;
        for (auto& x : left_) x = 0.0f;
        for (auto& x : modes_) x = {};

        const auto string = materialProperties(blueprint.stringMaterial);
        const float diameter = std::clamp(blueprint.stringDiameterMm, 0.08f, 5.0f) * 0.001f;
        const float linearDensity = string.densityKgM3 * 0.25f * kPi * diameter * diameter;
        const float tension = std::clamp(blueprint.tensionNewtons, 0.5f, 1500.0f);
        waveSpeedMetersPerSecond_ = std::sqrt(tension / std::max(1.0e-8f, linearDensity));
        const float hz = std::clamp(440.0f * std::pow(2.0f, (midi - 69) / 12.0f),
                                    25.0f, sampleRate_ * 0.21f);
        targetFrequencyHz_ = hz;
        // Simple pitch-by-length adaptation. Integer cells imply small tuning
        // deviations at high notes; fractional delays belong in the next stage.
        cells_ = std::clamp(static_cast<int>(std::lround(sampleRate_ / (2.0f * hz))), 8, kMaxStringCells);
        effectiveLengthMeters_ = waveSpeedMetersPerSecond_ / (2.0f * hz);
        const float decayPerSecond = 0.16f + 65.0f * string.lossFactor;
        const float roundTripRetention = std::exp(-decayPerSecond / hz);
        const float bridge = std::clamp(blueprint.bridgeCoupling, 0.0f, 1.0f);
        bridgeReflect_ = std::sqrt(roundTripRetention) * (1.0f - 0.0020f * bridge);
        nutReflect_ = std::sqrt(roundTripRetention);
        bridgeHighDamping_ = std::clamp(0.65f - 40.0f * string.lossFactor, 0.07f, 0.65f);
        bridgeLowpassMix_ = std::clamp(0.15f + 60.0f * string.lossFactor, 0.15f, 0.92f);
        // Body-to-string feedback needs a passive impedance junction; disabled in v0.1.
        const float exciterSoftness = materialProperties(blueprint.exciterMaterial).contactSoftness;
        hammerSamples_ = std::max(2, static_cast<int>(sampleRate_ * (0.00020f +
            0.0040f * (1.0f - clamp01(blueprint.hammerHardness)) * (0.5f + exciterSoftness))));
        strikeIndex_ = std::clamp(static_cast<int>(std::lround(
            std::clamp(blueprint.excitationPosition, 0.03f, 0.97f) * (cells_ - 1))), 1, cells_ - 2);
        dampingSmoothing_ = 1.0f - std::exp(-1.0f / (sampleRate_ * 0.008f));
        buildBodyModes();

        if (blueprint.excitation == Excitation::Pluck) {
            const float apex = static_cast<float>(strikeIndex_) / static_cast<float>(cells_ - 1);
            for (int i = 0; i < cells_; ++i) {
                const float x = static_cast<float>(i) / static_cast<float>(cells_ - 1);
                const float shape = x < apex ? x / apex : (1.0f - x) / (1.0f - apex);
                // Stationary, triangular initial string displacement.
                const float displacement = e1_ * 0.44f * shape;
                right_[i] = left_[i] = displacement * 0.5f;
            }
        }
    }

    void setExpression(float e1, float e2) noexcept {
        // E1 is a note-on action in this blueprint; changing it does not
        // re-strike the note. E2 remains a continuous contact control.
        e1_ = clamp01(e1);
        e2Target_ = clamp01(e2);
    }

    void release() noexcept {
        if(!released_){released_ = true;releasedAt_=samplesRendered_;}
    }
    void stop() noexcept { active_ = false; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] float targetFrequency() const noexcept { return targetFrequencyHz_; }
    [[nodiscard]] float effectiveLengthMeters() const noexcept { return effectiveLengthMeters_; }
    [[nodiscard]] float waveSpeedMetersPerSecond() const noexcept { return waveSpeedMetersPerSecond_; }
    [[nodiscard]] int stringCells() const noexcept { return cells_; }

    float render() noexcept {
        if (!active_) return 0.0f;
        const float incomingBridge = readLeft(0);
        const float incomingNut = readRight(cells_ - 1);
        // Bidirectional traveling waves propagate one cell/sample; endpoint
        // reflections invert displacement and transfer some energy to the body.
        rHead_ = (rHead_ + cells_ - 1) % cells_;
        lHead_ = (lHead_ + 1) % cells_;
        bridgeFilter_ += bridgeHighDamping_ * (incomingBridge - bridgeFilter_);
        const float smoothReflection =
            (1.0f - bridgeLowpassMix_) * incomingBridge + bridgeLowpassMix_ * bridgeFilter_;
        writeRight(0, -bridgeReflect_ * smoothReflection);
        writeLeft(cells_ - 1, -nutReflect_ * incomingNut);

        if (blueprint_.excitation == Excitation::Hammer &&
            samplesRendered_ < static_cast<uint64_t>(hammerSamples_)) {
            const float phase = (static_cast<float>(samplesRendered_) + 0.5f) /
                static_cast<float>(hammerSamples_);
            const float pulse = 4.4f * e1_ * (0.5f * kPi / hammerSamples_) *
                std::sin(kPi * phase);
            inject(strikeIndex_, pulse);
            if (strikeIndex_ > 1) inject(strikeIndex_ - 1, pulse * 0.18f);
            if (strikeIndex_ + 1 < cells_ - 1) inject(strikeIndex_ + 1, pulse * 0.18f);
        }

        e2Smoothed_ += dampingSmoothing_ * (e2Target_ - e2Smoothed_);
        const float pressure = e2Smoothed_ * e2Smoothed_;
        const float mutePoint = std::clamp(
            blueprint_.damperBasePosition + blueprint_.damperTravel * (1.0f - e2Smoothed_),
            0.01f, 0.48f);
        const int muteCell = std::clamp(static_cast<int>(mutePoint * (cells_ - 1)), 1, cells_ - 2);
        const float softness = materialProperties(blueprint_.damperMaterial).contactSoftness;
        const float damp = std::clamp(blueprint_.damperStrength * pressure *
            (0.07f + 0.36f * softness), 0.0f, 0.80f);
        if (damp > 0.0001f) {
            contact(muteCell, damp);
            if (muteCell + 1 < cells_ - 1) contact(muteCell + 1, 0.23f * damp);
        }
        if (released_) contact(std::max(1, cells_ / 11), 0.25f);

        // Bridge force drives the body's approximate structural modes.
        const float bridgeForce = incomingBridge - readRight(0);
        float radiated = 0.0f;
        for (auto& mode : modes_) {
            const float next = mode.a * mode.y1 - mode.b * mode.y2 + mode.gain * bridgeForce;
            mode.y2 = mode.y1;
            mode.y1 = next;
            radiated += next;
        }
        ++samplesRendered_;
        // Reclaim released voices after their damped acoustic tail. Without
        // this, long sessions would accumulate permanently active voices.
        if(released_ && samplesRendered_-releasedAt_ >
            static_cast<uint64_t>(sampleRate_*2.75f))stop();
        const float output = 0.8f * radiated + 0.028f * bridgeForce;
        if (!std::isfinite(output)) { stop(); return 0.0f; }
        // Safety limiter protects downstream buffers if model is over-driven.
        return std::clamp(output, -3.0f, 3.0f);
    }

private:
    struct Mode { float a{0}, b{0}, gain{0}, y1{0}, y2{0}; };
    static float clamp01(float v) noexcept { return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f; }
    float readRight(int i) const noexcept { return right_[(rHead_ + i) % cells_]; }
    float readLeft(int i) const noexcept { return left_[(lHead_ + i) % cells_]; }
    void writeRight(int i, float v) noexcept { right_[(rHead_ + i) % cells_] = v; }
    void writeLeft(int i, float v) noexcept { left_[(lHead_ + i) % cells_] = v; }
    void inject(int i, float wave) noexcept {
        writeRight(i, readRight(i) + wave);
        writeLeft(i, readLeft(i) + wave);
    }
    void contact(int i, float damping) noexcept {
        const float survive = 1.0f - damping;
        writeRight(i, readRight(i) * survive);
        writeLeft(i, readLeft(i) * survive);
    }
    void buildBodyModes() noexcept {
        constexpr std::array<float, kBodyModes> box{93, 178, 312, 490, 830, 1370};
        constexpr std::array<float, kBodyModes> cylinder{112, 251, 390, 633, 1020, 1610};
        constexpr std::array<float, kBodyModes> plate{143, 306, 565, 970, 1530, 2360};
        constexpr std::array<float, kBodyModes> bell{205, 412, 521, 757, 1125, 1542};
        const auto& centers = blueprint_.bodyGeometry == Geometry::HollowBox ? box :
            blueprint_.bodyGeometry == Geometry::HollowCylinder ? cylinder :
            blueprint_.bodyGeometry == Geometry::Plate ? plate : bell;
        const auto material = materialProperties(blueprint_.bodyMaterial);
        // Frequency changes with approximate sqrt(E/rho) and dimensions.
        const float speedRatio = std::sqrt((material.youngModulusPa / material.densityKgM3) /
            (11.0e9f / 650.0f));
        const float size = std::clamp(blueprint_.bodySize, 0.25f, 4.0f);
        const float coupling = std::clamp(blueprint_.bridgeCoupling, 0.0f, 1.0f);
        const float Q = std::clamp(1.2f / std::max(0.002f, material.lossFactor), 4.0f, 100.0f);
        for (int i = 0; i < kBodyModes; ++i) {
            const float hz = std::clamp(centers[i] * speedRatio / size,
                                        35.0f, sampleRate_ * 0.45f);
            const float omega = 2.0f * kPi * hz / sampleRate_;
            const float r = std::exp(-kPi * hz / (Q * sampleRate_));
            auto& m = modes_[i];
            m.a = 2.0f * r * std::cos(omega);
            m.b = r * r;
            m.gain = coupling * 0.0020f / (1.0f + 0.60f * i);
        }
    }

    float sampleRate_{48000.0f}, e1_{0.8f}, e2Target_{0.0f}, e2Smoothed_{0.0f};
    float dampingSmoothing_{0.002f};
    float targetFrequencyHz_{220.0f}, effectiveLengthMeters_{0.65f};
    float waveSpeedMetersPerSecond_{180.0f};
    float bridgeReflect_{0.999f}, nutReflect_{0.999f};
    float bridgeHighDamping_{0.65f}, bridgeLowpassMix_{0.15f}, bridgeFilter_{0.0f};
    Blueprint blueprint_{};
    std::array<float, kMaxStringCells> right_{}, left_{};
    std::array<Mode, kBodyModes> modes_{};
    int rHead_{0}, lHead_{0}, cells_{100}, strikeIndex_{20}, hammerSamples_{50};
    uint64_t samplesRendered_{0}, releasedAt_{0};
    bool active_{false}, released_{false};
};

} // namespace aiora::physical
