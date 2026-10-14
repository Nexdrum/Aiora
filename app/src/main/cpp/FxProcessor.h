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
        std::array<std::array<Biquad,6>,2> eq{};
        std::array<float,2> low{},inter{},high{};
        float cutoffA{0},cutoffB{0};
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
