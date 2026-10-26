#pragma once
#include <array>
#include <cstdint>
#include "AioraTypes.h"

namespace aiora {

// Prepared wavetable types are shared with DspPatch in AioraTypes.h.

class SpectrachordVoice {
public:
    void prepare(float sampleRate) noexcept;
    void start(
        int32_t id,
        const DspPatch& patch,
        int midi,
        float pressure,
        uint64_t gateSamples = 0,
        float samplesPerStep = 1.0f,
        const VoiceAutomation& automation = VoiceAutomation{}
    ) noexcept;
    void release() noexcept;
    void kill() noexcept;
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] int32_t id() const noexcept { return id_; }
    [[nodiscard]] uint64_t age() const noexcept { return age_; }
    void setAge(uint64_t v) noexcept { age_ = v; }
    void setModValue(float value, bool active = true) noexcept { modValue_ = clamp01(value); modActive_ = active; }
    void setLiveExpression(float v,float m) noexcept {
        velocity_=clamp01(v);modValue_=clamp01(m);
        modActive_=true;liveGain_=velocity_*velocity_;
    }
    float render() noexcept;

private:
    struct PointEnvState {
        OperatorCurve curve{};
        float age{0},releaseAge{0},releaseStart{0},value{0};
        bool releasing{false},done{false},holdLast{false};
        void reset(const OperatorCurve& c,bool retainFinal=false) noexcept;
        void noteOff() noexcept;
        float next(float sampleRate) noexcept;
    };
    struct Biquad {
        float b0{1},b1{0},b2{0},a1{0},a2{0};
        float z1{0},z2{0};
        void reset() noexcept { z1=z2=0; }
        void configureLowpass(float freq,float q,float sr) noexcept;
        void configurePeak(float freq,float q,float gainDb,float sr) noexcept;
        void configureEvolving(FilterShape shape,float freq,float qNorm,float gainDb,float sr) noexcept;
        float process(float x) noexcept;
    };

    static float midiHz(float midi) noexcept;
    static float clamp01(float v) noexcept;
    static float curveValue(const DspCurve& curve, float step, float fallback) noexcept;
    static float curveMean(const DspCurve& curve, float fallback) noexcept;
    float noise() noexcept;
    float hissSample(size_t index, int unisonVoice, float pitchHz) noexcept;
    float waveSample(Wave wave,const SpectralWaveTables& tables,
        float phase,float frequency) noexcept;
    bool active_{false};
    int32_t id_{-1};
    uint64_t age_{0};
    float sampleRate_{48000.0f};
    int midi_{62};
    float keyMapValue_{0}; // note-on key position; pitch bend does not change K
    float velocity_{0.8f};
    DspPatch patch_{};
    VoiceAutomation automation_{};
    std::array<PointEnvState,6> opVolume_{},opPitch_{},opMorphVolume_{},opMorphPitch_{};
    std::array<PointEnvState,6> filterTimeline_{};
    PointEnvState unisonTimeline_{},masterAmpTimeline_{};
    float unisonSmoothed_{0};
    std::array<Biquad,3> evolvingFilterBiquads_{};
    std::array<std::array<Biquad,2>,8> slopeLowpasses_{};
    std::array<float,9> slopeGainSmoothed_{},slopeGainTarget_{};
    uint32_t slopeUpdateCounter_{0};
    float filterFrequencySmoothed_{1000.0f},filterGainSmoothed_{0.0f},filterQSmoothed_{.5f};
    std::array<float,3> filterShapeSmoothed_{{0,1,0}};
    std::array<std::array<float,2>,6> phase_{};
    std::array<float,6> lastOp_{};
    // Immutable band-limited cycles are referenced through patch_.
    // Voice start performs no FFT and copies no waveform samples.
    std::array<std::array<float,2>,6> hissState_{};
    Biquad peak_{};
    float peakCenterSmoothed_{220.0f},peakQSmoothed_{1.0f},peakGainSmoothed_{0.0f};
    bool peakReady_{false};
    uint32_t rng_{0x91e10da5u};
    float lfoPhase_{0.0f};
    float lfoAge_{0.0f};
    float modValue_{0.0f};
    float liveGain_{1.0f};
    float samplesPerStep_{1.0f};
    float fixedUnison_{0.0f};
    uint64_t gateSamples_{0};
    uint64_t elapsedSamples_{0};
    bool modActive_{false};
    bool velocityCurveActive_{false};
    bool autoRelease_{false};
};

} // namespace aiora
