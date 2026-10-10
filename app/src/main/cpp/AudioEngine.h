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
#include "PlaybackSnapshot.h"

namespace aiora {

class AudioEngine final : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    static AudioEngine& instance();

    bool start();
    void stop();
    int noteOn(int midi, float velocity) noexcept;
    int noteOnPad(int padIndex, int midi, float velocity) noexcept;
    void noteOff(int voiceId) noexcept;
    void panic() noexcept;
    void setFactoryPreset(int index) noexcept;
    bool syncProject();
    bool playTransport(int startStep = 0);
    void stopTransport() noexcept;
    void collectRetiredSnapshots() noexcept;
    [[nodiscard]] bool transportPlaying() const noexcept { return transportPlaying_.load(std::memory_order_relaxed); }
    [[nodiscard]] int playheadStep() const noexcept { return playheadStep_.load(std::memory_order_relaxed); }
    [[nodiscard]] float playheadPosition() const noexcept { return playheadPosition_.load(std::memory_order_relaxed); }
    int factoryPreset() const noexcept { return selectedPreset_.load(std::memory_order_relaxed); }
    int sampleRate() const noexcept { return sampleRate_.load(std::memory_order_relaxed); }

    // Keep enough history for a D4-calibrated display window at common
    // Android sample rates, including 96 kHz.
    static constexpr size_t kScopeSamples = 2048;
    static constexpr size_t kScopeReadSamples = 1024;
    void copyScope(std::array<float, kScopeReadSamples>& out) const noexcept;

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) override;
    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override;

private:
    AudioEngine() = default;
    ~AudioEngine() override { stop(); }
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    enum class EventType : uint8_t { NoteOn, PadOn, NoteOff, Panic, Preset, Snapshot, Play, StopTransport };
    struct Event {
        EventType type{EventType::Panic};
        int32_t id{-1};
        int32_t midi{62};
        int32_t source{0};
        float value{0.8f};
        uintptr_t pointer{0};
        DspPatch patch{};
        float gainLeft{1.0f};
        float gainRight{1.0f};
    };
    struct VoiceSlot {
        SpectrachordVoice voice{};
        float gainLeft{1.0f};
        float gainRight{1.0f};
        int16_t fxBus{-1};
        bool transport{false};
    };
    struct PlaybackFxBus {
        FxProcessor processor{};
    };

    void applyEvent(const Event&) noexcept;
    std::array<float,2> renderFrame() noexcept;
    VoiceSlot& allocateVoice() noexcept;
    void triggerStep(int step,double phase) noexcept;
    void advanceTransport() noexcept;
    void configureFx(const Fx& fx) noexcept;
    void configureFxForPreset(int preset) noexcept;

    std::shared_ptr<oboe::AudioStream> stream_;
    std::mutex streamMutex_;
    SpscQueue<Event, 512> events_;
    SpscQueue<PlaybackSnapshot*, 256> retiredSnapshots_;
    std::array<VoiceSlot, 48> voices_{};
    FxProcessor previewFx_{};
    FxProcessor masterFx_{};
    std::array<PlaybackFxBus, kMaxPlaybackFxBuses> playbackFx_{};
    int32_t playbackFxCount_{0};
    PlaybackSnapshot* playback_{nullptr};
    std::atomic<int32_t> nextVoiceId_{1};
    std::atomic<int32_t> sampleRate_{48000};
    std::atomic<int32_t> selectedPreset_{0};
    std::atomic<bool> transportPlaying_{false};
    std::atomic<int32_t> playheadStep_{0};
    std::atomic<float> playheadPosition_{0.0f};
    int32_t transportStep_{0};
    size_t stepEventCursor_{0};
    double samplesIntoStep_{0.0};
    double transportSamplesPerStep_{1.0};
    uint64_t ageCounter_{0};

    std::array<std::atomic<int32_t>, kScopeSamples> scope_{};
    std::atomic<uint32_t> scopeWrite_{0};
};

} // namespace aiora
