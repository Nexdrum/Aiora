#pragma once
#include <array>
#include <cstdint>
#include "AioraTypes.h"

namespace aiora {

class SpectrachordVoice {
public:
    void prepare(float sampleRate) noexcept;
    void start(int32_t id, const Patch& patch, int midi, float velocity) noexcept;
    void release() noexcept;
    void kill() noexcept;
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] int32_t id() const noexcept { return id_; }
    [[nodiscard]] uint64_t age() const noexcept { return age_; }
    void setAge(uint64_t v) noexcept { age_ = v; }
    void setModValue(float value, bool active = true) noexcept { modValue_ = clamp01(value); modActive_ = active; }
    float render() noexcept;

private:
    enum class EnvStage : uint8_t { Off, Attack, Decay, Sustain, Release };
    struct EnvState {
        EnvStage stage{EnvStage::Off};
        float value{0.0f};
        float releaseStart{0.0f};
        uint32_t stageSamples{0};
        uint32_t pos{0};
        Envelope shape{};
        void reset(const Envelope& e, float sr) noexcept;
        void noteOff(float sr) noexcept;
        float next(float sr) noexcept;
        bool done() const noexcept { return stage == EnvStage::Off; }
    };
    struct Biquad {
        float b0{1},b1{0},b2{0},a1{0},a2{0};
        float z1{0},z2{0};
        void reset() noexcept { z1=z2=0; }
        void configure(FilterType type, float freq, float q, float sr) noexcept;
        float process(float x) noexcept;
    };

    static float midiHz(float midi) noexcept;
    static float clamp01(float v) noexcept;
    float noise() noexcept;
    float waveSample(const Operator& op, float phase, float morph) noexcept;
    float slotValue(ModTarget target, float m, float fallback) const noexcept;
    float opLevel(size_t index, float m) const noexcept;
    float morphValue(size_t index, float m) const noexcept;

    bool active_{false};
    int32_t id_{-1};
    uint64_t age_{0};
    float sampleRate_{48000.0f};
    int midi_{62};
    float velocity_{0.8f};
    const Patch* patch_{nullptr};
    std::array<EnvState,6> opEnv_{};
    EnvState ampEnv_{};
    EnvState filterEnv_{};
    std::array<std::array<float,2>,6> phase_{};
    std::array<float,6> lastOp_{};
    Biquad filter_{};
    uint32_t rng_{0x91e10da5u};
    float lfoPhase_{0.0f};
    float lfoAge_{0.0f};
    float modValue_{0.0f};
    bool modActive_{false};
};

} // namespace aiora
