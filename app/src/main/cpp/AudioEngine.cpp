#include "AudioEngine.h"
#include "FactoryPresets.h"
#include <algorithm>
#include <android/log.h>

namespace aiora {
namespace {
constexpr char kTag[] = "AIORA";
constexpr int kPresetCount = static_cast<int>(FactoryPreset::Count);
int clampPreset(int index) noexcept { return std::clamp(index, 0, kPresetCount - 1); }
}

AudioEngine& AudioEngine::instance() {
    static AudioEngine engine;
    return engine;
}

bool AudioEngine::start() {
    std::scoped_lock lock(streamMutex_);
    if (stream_ && stream_->getState() != oboe::StreamState::Closed) return true;

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setDataCallback(this)
        ->setErrorCallback(this);

    auto result = builder.openStream(stream_);
    if (result != oboe::Result::OK || !stream_) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "openStream failed: %s", oboe::convertToText(result));
        stream_.reset();
        return false;
    }

    const int sr = stream_->getSampleRate();
    sampleRate_.store(sr, std::memory_order_relaxed);
    for (auto& voice : voices_) voice.prepare(static_cast<float>(sr));
    previewFx_.prepare(static_cast<float>(sr));
    configureFxForPreset(selectedPreset_.load(std::memory_order_relaxed));

    const auto burst = stream_->getFramesPerBurst();
    if (burst > 0) stream_->setBufferSizeInFrames(burst * 3);

    result = stream_->requestStart();
    if (result != oboe::Result::OK) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "requestStart failed: %s", oboe::convertToText(result));
        stream_->close();
        stream_.reset();
        return false;
    }
    return true;
}

void AudioEngine::stop() {
    std::shared_ptr<oboe::AudioStream> old;
    {
        std::scoped_lock lock(streamMutex_);
        old = std::move(stream_);
    }
    if (old) {
        old->requestStop();
        old->close();
    }
    for (auto& voice : voices_) voice.kill();
    previewFx_.reset();
}

int AudioEngine::noteOn(int midi, float velocity) noexcept {
    const int id = nextVoiceId_.fetch_add(1, std::memory_order_relaxed);
    const int preset = selectedPreset_.load(std::memory_order_relaxed);
    events_.push({EventType::NoteOn, id, std::clamp(midi, 0, 127), preset, std::clamp(velocity, 0.0f, 1.0f)});
    return id;
}

void AudioEngine::noteOff(int voiceId) noexcept {
    events_.push({EventType::NoteOff, voiceId, 0, 0, 0.0f});
}

void AudioEngine::panic() noexcept {
    events_.push({EventType::Panic, -1, 0, 0, 0.0f});
}

void AudioEngine::setFactoryPreset(int index) noexcept {
    const int p = clampPreset(index);
    selectedPreset_.store(p, std::memory_order_relaxed);
    events_.push({EventType::Preset, -1, 0, p, 0.0f});
}

SpectrachordVoice& AudioEngine::allocateVoice() noexcept {
    for (auto& v : voices_) if (!v.active()) return v;
    return *std::min_element(voices_.begin(), voices_.end(), [](const SpectrachordVoice& a, const SpectrachordVoice& b) {
        return a.age() < b.age();
    });
}

void AudioEngine::configureFxForPreset(int preset) noexcept {
    previewFx_.set(factoryBank()[static_cast<size_t>(clampPreset(preset))].fx);
}

void AudioEngine::applyEvent(const Event& e) noexcept {
    switch (e.type) {
        case EventType::Panic:
            for (auto& v : voices_) v.kill();
            previewFx_.reset();
            return;
        case EventType::Preset:
            configureFxForPreset(e.preset);
            return;
        case EventType::NoteOff:
            for (auto& v : voices_) if (v.active() && v.id() == e.id) { v.release(); return; }
            return;
        case EventType::NoteOn: {
            auto& v = allocateVoice();
            v.start(e.id, factoryBank()[static_cast<size_t>(clampPreset(e.preset))], e.midi, e.value);
            v.setAge(++ageCounter_);
            configureFxForPreset(e.preset);
            return;
        }
    }
}

std::array<float,2> AudioEngine::renderFrame() noexcept {
    float dry = 0.0f;
    for (auto& v : voices_) if (v.active()) dry += v.render();
    return previewFx_.process(dry * 0.33f);
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) {
    Event e;
    while (events_.pop(e)) applyEvent(e);

    auto* out = static_cast<float*>(audioData);
    for (int32_t i = 0; i < numFrames; ++i) {
        const auto s = renderFrame();
        out[i * 2] = s[0];
        out[i * 2 + 1] = s[1];
    }
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream*, oboe::Result error) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "Audio stream closed after error: %s", oboe::convertToText(error));
    {
        std::scoped_lock lock(streamMutex_);
        stream_.reset();
    }
    start();
}

} // namespace aiora
