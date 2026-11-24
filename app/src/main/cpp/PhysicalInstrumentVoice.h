#pragma once

// Experimental, self-contained physical instrument mechanism.
// No dependency on Spectrachord, its presets, or the production audio path.
// Fixed-capacity state; after prepare/start, render() makes no allocations.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "PhysicalFlowGraph.h"

namespace aiora::physical {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kMaxStringCells = 2048;
constexpr int kBodyModes = 6;
constexpr int kAllBodyModes=kBodyModes*kMaxFlowBodies;

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
        resolveFlowGraph();
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
        bodyModeCount_=0;

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

        if (excitationConnected_ && blueprint_.excitation == Excitation::Pluck) {
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

        if (excitationConnected_ && blueprint_.excitation == Excitation::Hammer &&
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
        if(blueprint_.graph.nodeCount>0) {
            for(int j=0;j<damperCount_;++j){
                const auto& d=dampers_[static_cast<size_t>(j)];
                const int cell=std::clamp(static_cast<int>(
                    std::clamp(d.position+.09f*(1.0f-e2Smoothed_),.01f,.48f)*
                    (cells_-1)),1,cells_-2);
                const float softness=materialProperties(d.material).contactSoftness;
                const float loss=std::clamp(d.coupling*pressure*
                    (.06f+.34f*softness),0.0f,.80f);
                if(loss>.0001f){
                    contact(cell,loss);
                    if(cell+1<cells_-1)contact(cell+1,.23f*loss);
                }
            }
        }else{
            const float softness=materialProperties(blueprint_.damperMaterial).contactSoftness;
            const float loss=std::clamp(blueprint_.damperStrength*pressure*
                (.07f+.36f*softness),0.0f,.80f);
            if(loss>.0001f){
                contact(muteCell,loss);
                if(muteCell+1<cells_-1)contact(muteCell+1,.23f*loss);
            }
        }
        if (released_) contact(std::max(1, cells_ / 11), 0.25f);

        // Bridge force drives the body's approximate structural modes.
        const float bridgeForce = bridgeConnected_ ? incomingBridge - readRight(0) : 0.0f;
        float radiated = 0.0f;
        for (int i=0;i<bodyModeCount_;++i) {
            auto& mode=modes_[static_cast<size_t>(i)];
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
        const float output = radiated + (directConnected_ ? 0.09f*bridgeForce : 0.0f);
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
    // Translate the connected physical nodes into a single-note mechanism.
    // Unconnected nodes have no acoustic influence. Connection types are
    // deliberately restricted to paths implemented by this v2 solver.
    void resolveFlowGraph() noexcept {
        excitationConnected_=bridgeConnected_=directConnected_=true;
        bodyCount_=damperCount_=0;
        const auto& graph=blueprint_.graph;
        if(graph.nodeCount==0)return; // legacy standalone blueprint
        const int ex=graph.first(FlowKind::Exciter);
        const int st=graph.first(FlowKind::String);
        const int br=graph.first(FlowKind::Bridge);
        const int out=graph.first(FlowKind::Output);
        excitationConnected_=graph.linked(ex,st);
        bridgeConnected_=graph.linked(st,br);
        directConnected_=bridgeConnected_&&graph.linked(br,out);
        if(st>=0){
            const auto& node=graph.nodes[static_cast<size_t>(st)];
            blueprint_.stringMaterial=node.material;
            blueprint_.stringDiameterMm=node.diameterMm;
            blueprint_.tensionNewtons=node.tensionNewtons;
        }
        if(ex>=0){
            const auto& node=graph.nodes[static_cast<size_t>(ex)];
            blueprint_.exciterMaterial=node.material;
            blueprint_.excitation=node.excitation;
            blueprint_.excitationPosition=node.position;
            blueprint_.hammerHardness=node.hardness;
        }
        if(br>=0){
            const auto& node=graph.nodes[static_cast<size_t>(br)];
            // A stiff bridge more efficiently transfers mechanical force.
            const auto properties=materialProperties(node.material);
            const float stiffness=std::clamp(std::sqrt(
                properties.youngModulusPa/(11.e9f)),.25f,1.75f);
            blueprint_.bridgeCoupling=std::clamp(node.coupling*stiffness,0.0f,1.0f);
        }
        if(bridgeConnected_){
            for(int i=0;i<graph.nodeCount;++i) {
                if(graph.nodes[static_cast<size_t>(i)].kind!=FlowKind::Body||
                   !graph.linked(br,i)||!graph.linked(i,out))continue;
                if(bodyCount_<kMaxFlowBodies)
                    bodies_[static_cast<size_t>(bodyCount_++)]=graph.nodes[static_cast<size_t>(i)];
            }
        }
        for(int i=0;i<graph.nodeCount;++i){
            if(graph.nodes[static_cast<size_t>(i)].kind==FlowKind::Damper&&
               graph.linked(i,st)&&damperCount_<kMaxFlowDampers)
                dampers_[static_cast<size_t>(damperCount_++)]=graph.nodes[static_cast<size_t>(i)];
        }
    }
    void appendBodyModes(Material materialType,Geometry geometry,
        float size,float thicknessMm,float dampingScale,float coupling) noexcept {
        constexpr std::array<float,kBodyModes> box{93,178,312,490,830,1370};
        constexpr std::array<float,kBodyModes> cylinder{112,251,390,633,1020,1610};
        constexpr std::array<float,kBodyModes> plate{143,306,565,970,1530,2360};
        constexpr std::array<float,kBodyModes> bell{205,412,521,757,1125,1542};
        constexpr std::array<float,kBodyModes> bowl{176,369,597,803,1230,1830};
        constexpr std::array<float,kBodyModes> sphere{128,255,426,716,1080,1640};
        constexpr std::array<float,kBodyModes> tube{93,187,279,372,465,558};
        const auto& centers=geometry==Geometry::HollowBox?box:
            geometry==Geometry::HollowCylinder?cylinder:
            geometry==Geometry::Plate?plate:
            geometry==Geometry::Bell?bell:
            geometry==Geometry::Bowl?bowl:
            geometry==Geometry::Sphere?sphere:tube;
        const auto material=materialProperties(materialType);
        const float speedRatio=std::sqrt((material.youngModulusPa/material.densityKgM3)/
            (11.e9f/650.f));
        size=std::clamp(size,.25f,4.f);
        thicknessMm=std::clamp(thicknessMm,.3f,35.f);
        dampingScale=std::clamp(dampingScale,.15f,8.f);
        coupling=std::clamp(coupling,0.f,1.f);
        const float thicknessRatio=thicknessMm/3.f;
        const float thicknessPitch=std::pow(thicknessRatio,
            geometry==Geometry::Plate?.85f:.28f);
        // Softer materials and added damping increase decay. Q affects
        // bandwidth and duration but NOT unbounded peak gain.
        const float q=std::clamp(1.2f/
            std::max(.002f,material.lossFactor*dampingScale),2.0f,140.0f);
        const float radiation=std::clamp(std::pow(thicknessRatio,-.22f),.42f,1.6f);
        for(int i=0;i<kBodyModes&&bodyModeCount_<kAllBodyModes;++i){
            const float hz=std::clamp(centers[i]*speedRatio*thicknessPitch/size,
                                     35.f,sampleRate_*.45f);
            const float omega=2.f*kPi*hz/sampleRate_;
            const float r=std::exp(-kPi*hz/(q*sampleRate_));
            const float sinOmega=std::sin(omega);
            // Magnitude of the resonator denominator evaluated at its own
            // central frequency: |(1-r)*(1-r*exp(-j*2*omega))|.
            // Normalizing by this value prevents high-Q materials from
            // acquiring runaway output gain at matched notes.
            const float resonanceDen=(1.f-r)*std::sqrt(
                (1.f-r)*(1.f-r)+4.f*r*sinOmega*sinOmega);
            auto& mode=modes_[static_cast<size_t>(bodyModeCount_++)];
            mode.a=2.f*r*std::cos(omega);
            mode.b=r*r;
            const float peakTarget=(.32f/(1.f+.65f*i))*coupling*radiation;
            mode.gain=peakTarget*resonanceDen;
        }
    }
    void buildBodyModes() noexcept {
        bodyModeCount_=0;
        if(!bridgeConnected_)return;
        if(blueprint_.graph.nodeCount==0){
            appendBodyModes(blueprint_.bodyMaterial,blueprint_.bodyGeometry,
                blueprint_.bodySize,blueprint_.bodyThicknessMm,
                blueprint_.bodyDamping,blueprint_.bridgeCoupling);
            return;
        }
        const float share=1.f/std::sqrt(static_cast<float>(std::max(1,bodyCount_)));
        for(int i=0;i<bodyCount_;++i){
            const auto& body=bodies_[static_cast<size_t>(i)];
            appendBodyModes(body.material,body.geometry,
                body.size,body.thicknessMm,body.damping,
                blueprint_.bridgeCoupling*body.coupling*share);
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
    std::array<Mode,kAllBodyModes> modes_{};
    std::array<FlowNode,kMaxFlowBodies> bodies_{};
    std::array<FlowNode,kMaxFlowDampers> dampers_{};
    int bodyModeCount_{0},bodyCount_{0},damperCount_{0};
    bool excitationConnected_{true},bridgeConnected_{true},directConnected_{true};
    int rHead_{0}, lHead_{0}, cells_{100}, strikeIndex_{20}, hammerSamples_{50};
    uint64_t samplesRendered_{0}, releasedAt_{0};
    bool active_{false}, released_{false};
};

} // namespace aiora::physical
