#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "AioraTypes.h"

namespace aiora {

class FxProcessor {
public:
    void prepare(float sampleRate, float maxDelaySeconds = 1.2f);
    void reset() noexcept;
    void set(const Fx& fx) noexcept { fx_ = fx; }
    std::array<float,2> process(float input) noexcept;

private:
    struct Comb {
        std::vector<float> buf;
        size_t pos{0};
        float feedback{0.72f};
        float damp{0.22f};
        float store{0};
        float process(float x) noexcept;
        void reset() noexcept;
    };
    struct Allpass {
        std::vector<float> buf;
        size_t pos{0};
        float feedback{0.5f};
        float process(float x) noexcept;
        void reset() noexcept;
    };

    float sampleRate_{48000.0f};
    Fx fx_{};
    std::vector<float> delay_;
    size_t delayWrite_{0};
    std::array<Comb,4> combL_{};
    std::array<Comb,4> combR_{};
    std::array<Allpass,2> allL_{};
    std::array<Allpass,2> allR_{};
};

} // namespace aiora
