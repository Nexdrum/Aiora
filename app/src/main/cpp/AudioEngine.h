#pragma once
#include <oboe/Oboe.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include "SpscQueue.h"
#include "SpectrachordVoice.h"
#include "FxProcessor.h"

namespace aiora {

class AudioEngine final : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    static AudioEngine& instance();

    bool start();
    void stop();
    int noteOn(int midi, float velocity) noexcept;
    void noteOff(int voiceId) noexcept;
    void panic() noexcept;
    void setFactoryPreset(int index) noexcept;
    int factoryPreset() const noexcept { return selectedPreset_.load(std::memory_order_relaxed); }
    int sampleRate() const noexcept { return sampleRate_.load(std::memory_order_relaxed); }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override;

private:
    AudioEngine() = default;
    ~AudioEngine() override { stop(); }
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    enum class EventType : uint8_t { NoteOn, NoteOff, Panic, Preset };
    struct Event {
        EventType type{EventType::Panic};
        int32_t id{-1};
        int32_t midi{62};
        int32_t preset{0};
        float value{0.8f};
    };

    void applyEvent(const Event&) noexcept;
    std::array<float,2> renderFrame() noexcept;
    SpectrachordVoice& allocateVoice() noexcept;
    void configureFxForPreset(int preset) noexcept;

    std::shared_ptr<oboe::AudioStream> stream_;
    std::mutex streamMutex_;
    SpscQueue<Event, 512> events_;
    std::array<SpectrachordVoice, 48> voices_{};
    FxProcessor previewFx_{};
    std::atomic<int32_t> nextVoiceId_{1};
    std::atomic<int32_t> sampleRate_{48000};
    std::atomic<int32_t> selectedPreset_{0};
    uint64_t ageCounter_{0};
};

} // namespace aiora
