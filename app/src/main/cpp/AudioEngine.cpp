#include "AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <android/log.h>

namespace aiora {
namespace {
constexpr float kTwoPi = 6.2831853071795864769f;
constexpr char kTag[] = "AIORA";
float midiHz(int midi) noexcept { return 440.0f * std::pow(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f); }
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

    sampleRate_.store(stream_->getSampleRate(), std::memory_order_relaxed);
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
    panic();
}

int AudioEngine::noteOn(int midi, float velocity) noexcept {
    const int id = nextVoiceId_.fetch_add(1, std::memory_order_relaxed);
    events_.push({EventType::NoteOn, id, std::clamp(midi, 0, 127), std::clamp(velocity, 0.0f, 1.0f)});
    return id;
}

void AudioEngine::noteOff(int voiceId) noexcept { events_.push({EventType::NoteOff, voiceId, 0, 0.0f}); }
void AudioEngine::panic() noexcept { events_.push({EventType::Panic, -1, 0, 0.0f}); }

AudioEngine::Voice& AudioEngine::allocateVoice() noexcept {
    for (auto& v : voices_) if (!v.active) return v;
    return *std::min_element(voices_.begin(), voices_.end(), [](const Voice& a, const Voice& b){ return a.age < b.age; });
}

void AudioEngine::applyEvent(const Event& e) noexcept {
    if (e.type == EventType::Panic) {
        for (auto& v : voices_) v = {};
        return;
    }
    if (e.type == EventType::NoteOff) {
        for (auto& v : voices_) if (v.active && v.id == e.id) {
            v.stage = EnvStage::Release;
            v.releaseStep = std::max(v.env / std::max(1.0f, sampleRate_.load() * 0.12f), 1.0e-7f);
            return;
        }
        return;
    }

    auto& v = allocateVoice();
    v = {};
    v.active = true;
    v.id = e.id;
    v.velocity = e.value;
    v.phaseInc = kTwoPi * midiHz(e.midi) / static_cast<float>(std::max(1, sampleRate_.load()));
    v.stage = EnvStage::Attack;
    v.age = ++ageCounter_;
}

float AudioEngine::renderFrame() noexcept {
    const float sr = static_cast<float>(std::max(1, sampleRate_.load(std::memory_order_relaxed)));
    float mix = 0.0f;
    for (auto& v : voices_) {
        if (!v.active) continue;
        if (v.stage == EnvStage::Attack) {
            v.env += 1.0f / (sr * 0.006f);
            if (v.env >= 1.0f) { v.env = 1.0f; v.stage = EnvStage::Sustain; }
        } else if (v.stage == EnvStage::Release) {
            v.env -= v.releaseStep;
            if (v.env <= 0.0001f) { v = {}; continue; }
        }
        mix += std::sin(v.phase) * v.env * v.velocity * 0.18f;
        v.phase += v.phaseInc;
        if (v.phase >= kTwoPi) v.phase -= kTwoPi;
    }
    return std::tanh(mix);
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) {
    Event e;
    while (events_.pop(e)) applyEvent(e);

    auto* out = static_cast<float*>(audioData);
    for (int32_t i = 0; i < numFrames; ++i) {
        const float s = renderFrame();
        out[i * 2] = s;
        out[i * 2 + 1] = s;
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
