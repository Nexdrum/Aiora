#pragma once
#include <oboe/Oboe.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include "SpscQueue.h"

namespace aiora {

class AudioEngine final : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    static AudioEngine& instance();

    bool start();
    void stop();
    int noteOn(int midi, float velocity) noexcept;
    void noteOff(int voiceId) noexcept;
    void panic() noexcept;
    int sampleRate() const noexcept { return sampleRate_.load(std::memory_order_relaxed); }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override;

private:
    AudioEngine() = default;
    ~AudioEngine() override { stop(); }
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    enum class EventType : uint8_t { NoteOn, NoteOff, Panic };
    struct Event { EventType type{EventType::Panic}; int32_t id{-1}; int32_t midi{62}; float value{0.8f}; };
    enum class EnvStage : uint8_t { Off, Attack, Sustain, Release };
    struct Voice {
        bool active{false};
        int32_t id{-1};
        float phase{0.0f}, phaseInc{0.0f};
        float velocity{0.0f}, env{0.0f}, releaseStep{0.0f};
        EnvStage stage{EnvStage::Off};
        uint64_t age{0};
    };

    void applyEvent(const Event&) noexcept;
    float renderFrame() noexcept;
    Voice& allocateVoice() noexcept;

    std::shared_ptr<oboe::AudioStream> stream_;
    std::mutex streamMutex_;
    SpscQueue<Event, 512> events_;
    std::array<Voice, 48> voices_{};
    std::atomic<int32_t> nextVoiceId_{1};
    std::atomic<int32_t> sampleRate_{48000};
    uint64_t ageCounter_{0};
};

} // namespace aiora
