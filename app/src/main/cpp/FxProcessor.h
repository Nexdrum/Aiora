#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "AioraTypes.h"

namespace aiora {

// Fixed-order, allocation-free audio processing. Buffers are prepared ahead
// of playback; identical module types may appear in multiple chain slots.
class FxProcessor {
public:
    void prepare(float sampleRate,float maxDelaySeconds=1.2f,bool realtimeChanges=false);
    void reset() noexcept;
    void set(const Fx& fx);
    std::array<float,2> process(float mono) noexcept {return processStereo(mono,mono);}
    std::array<float,2> processStereo(float left,float right) noexcept;

private:
    struct Comb {
        std::vector<float> buf;
        size_t pos{0},length{0};
        float feedback{0.72f},damp{0.22f},store{0};
        float process(float x) noexcept;
        void reset() noexcept;
    };
    struct Allpass {
        std::vector<float> buf;
        size_t pos{0},length{0};
        float feedback{0.5f};
        float process(float x) noexcept;
        void reset() noexcept;
    };
    struct Biquad {
        float b0{1},b1{0},b2{0},a1{0},a2{0},z1{0},z2{0};
        float process(float x) noexcept {
            const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
            return y;
        }
        void peak(float freq,float q,float gainDb,float sr) noexcept;
        void clear() noexcept {z1=z2=0;}
    };
    struct Stage {
        FxModule module{};
        std::vector<float> delay;
        size_t delayWrite{0};
        std::array<Comb,4> combL{},combR{};
        std::array<Allpass,2> allL{},allR{};
        std::array<std::array<Biquad,kPrismaBands>,2> eq{};
        float crushLevels{65535.0f};
        bool crushEnabled{false};
        // 2x-oversampling state for Ampere's anti-aliasing FIR.
        std::array<std::array<float,33>,2> coreHistory{};
        std::array<float,33> coreFir{};
        std::array<size_t,2> corePosition{};
        std::array<float,2> corePrevious{};
        // Ampere: all tracking comes from the incoming audio, never the
        // synth's normal/morph envelopes. Separate state for each channel.
        std::array<float,2> ampEnvelope{},ampTightLow{},ampMidLow{},
            ampMidHigh{},ampPickLow{},ampInter{},ampGrowlLow{},
            ampGrowlHigh{},ampDc{};
        float ampTightPole{0},ampMidLowPole{0},ampMidHighPole{0},
            ampPickPole{0},ampInterPole{0},ampGrowlLowPole{0},
            ampGrowlHighPole{0},ampDcPole{0},ampAttackPole{0},
            ampReleasePole{0};
        // Cabeus: 64-tap procedural, causal FIR cabinet responses.
        // These are synthetic kernels, NOT sampled commercial speaker IRs.
        std::array<float,64> cabImpulse{};
        std::array<std::array<float,64>,2> cabHistory{};
        std::array<size_t,2> cabPosition{};
        std::array<float,2> cabLow{},cabHigh{};
        float cabLowPole{0},cabHighPole{0};
        void prepare(float sr,float maxDelay,FxType type,bool allTypes);
        void configure(float sr);
        void reset() noexcept;
        std::array<float,2> process(float l,float r,float sr) noexcept;
    };
    float sampleRate_{48000.0f},maxDelaySeconds_{1.2f};
    bool realtimeChanges_{false};
    Fx fx_{};
    std::array<Stage,kMaxFxStages> stages_{};
};
} // namespace aiora
