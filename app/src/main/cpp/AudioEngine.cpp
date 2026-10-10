#include "AudioEngine.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "ProjectCore.h"
#include <algorithm>
#include <cmath>
#include <android/log.h>

namespace aiora {
namespace {
constexpr char kTag[] = "AIORA";
constexpr int kPresetCount = static_cast<int>(FactoryPreset::Count);
int clampPreset(int index) noexcept { return std::clamp(index,0,kPresetCount-1); }
int clampPad(int index) noexcept { return std::clamp(index,0,static_cast<int>(nexdrumKit().size())-1); }
std::pair<float,float> panGain(float gain,float pan) noexcept {
    constexpr float kPi=3.14159265358979323846f;
    const float p=std::clamp(pan,-1.0f,1.0f);
    const float angle=(p+1.0f)*kPi*0.25f;
    return {gain*std::cos(angle),gain*std::sin(angle)};
}
}

AudioEngine& AudioEngine::instance(){static AudioEngine engine;return engine;}

bool AudioEngine::start(){
    std::scoped_lock lock(streamMutex_);
    if(stream_&&stream_->getState()!=oboe::StreamState::Closed)return true;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)->setPerformanceMode(oboe::PerformanceMode::LowLatency)->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)->setChannelCount(oboe::ChannelCount::Stereo)->setDataCallback(this)->setErrorCallback(this);
    auto result=builder.openStream(stream_);
    if(result!=oboe::Result::OK||!stream_){__android_log_print(ANDROID_LOG_ERROR,kTag,"openStream failed: %s",oboe::convertToText(result));stream_.reset();return false;}
    const int sr=stream_->getSampleRate();sampleRate_.store(sr,std::memory_order_relaxed);
    for(auto&slot:voices_)slot.voice.prepare(static_cast<float>(sr));
    previewFx_.prepare(static_cast<float>(sr));
    masterFx_.prepare(static_cast<float>(sr));
    for(auto& bus:playbackFx_)bus.processor.prepare(static_cast<float>(sr));
    configureFxForPreset(selectedPreset_.load(std::memory_order_relaxed));
    const auto burst=stream_->getFramesPerBurst();if(burst>0)stream_->setBufferSizeInFrames(burst*3);
    result=stream_->requestStart();
    if(result!=oboe::Result::OK){__android_log_print(ANDROID_LOG_ERROR,kTag,"requestStart failed: %s",oboe::convertToText(result));stream_->close();stream_.reset();return false;}
    return true;
}

void AudioEngine::stop(){
    std::shared_ptr<oboe::AudioStream> old;{std::scoped_lock lock(streamMutex_);old=std::move(stream_);}if(old){old->requestStop();old->close();}
    transportPlaying_.store(false,std::memory_order_relaxed);for(auto&slot:voices_)slot.voice.kill();
    previewFx_.reset();masterFx_.reset();for(auto&bus:playbackFx_)bus.processor.reset();playbackFxCount_=0;
    Event pending;while(events_.pop(pending))if(pending.type==EventType::Snapshot&&pending.pointer)delete reinterpret_cast<PlaybackSnapshot*>(pending.pointer);
    if(playback_){delete playback_;playback_=nullptr;}collectRetiredSnapshots();
}

int AudioEngine::noteOn(int midi,float velocity) noexcept {
    const int id=nextVoiceId_.fetch_add(1,std::memory_order_relaxed);
    Event e;
    e.type=EventType::NoteOn;
    e.id=id;
    e.midi=std::clamp(midi,0,127);
    e.value=std::clamp(velocity,0.0f,1.0f);
    auto& project=ProjectCore::instance();
    e.patch=project.selectedDspPatch();
    const int track=project.selectedTrack();
    if(track>=0){
        const auto [l,r]=panGain(project.trackVolume(track),project.trackPan(track));
        e.gainLeft=l;e.gainRight=r;
    }
    events_.push(e);
    return id;
}
int AudioEngine::noteOnPad(int padIndex,int midi,float velocity) noexcept {
    const int id=nextVoiceId_.fetch_add(1,std::memory_order_relaxed);
    const int p=clampPad(padIndex);
    auto& project=ProjectCore::instance();
    const int track=project.selectedTrack();

    Event e;
    e.type=EventType::PadOn;
    e.id=id;
    e.source=p;
    e.value=std::clamp(velocity,0.0f,1.0f);

    if(track>=0&&project.trackIsDrums(track)&&p<project.padCount(track)){
        const int lo=std::min(project.padLow(track,p),project.padHigh(track,p));
        const int hi=std::max(project.padLow(track,p),project.padHigh(track,p));
        e.midi=std::clamp(midi,lo,hi);
        e.patch=project.padDspPatch(track,p);
        const float gain=project.trackVolume(track)*project.padVolume(track,p);
        const float pan=std::clamp(project.trackPan(track)+project.padPan(track,p),-1.0f,1.0f);
        const auto [l,r]=panGain(gain,pan);e.gainLeft=l;e.gainRight=r;
    }else{
        const auto& pad=nexdrumKit()[static_cast<size_t>(p)];
        e.midi=std::clamp(midi,std::min(pad.lowMidi,pad.highMidi),std::max(pad.lowMidi,pad.highMidi));
        e.patch=toDspPatch(pad.patch);
    }
    events_.push(e);
    return id;
}
void AudioEngine::noteOff(int voiceId) noexcept {events_.push({EventType::NoteOff,voiceId,0,0,0,0});}
void AudioEngine::panic() noexcept {events_.push({EventType::Panic,-1,0,0,0,0});}
void AudioEngine::setFactoryPreset(int index) noexcept {const int p=clampPreset(index);selectedPreset_.store(p,std::memory_order_relaxed);events_.push({EventType::Preset,-1,0,p,0,0});}

bool AudioEngine::syncProject(){
    collectRetiredSnapshots();
    auto snapshot=ProjectCore::instance().makePlaybackSnapshot();if(!snapshot)return false;
    PlaybackSnapshot* raw=snapshot.release();
    if(!events_.push({EventType::Snapshot,-1,0,0,0,reinterpret_cast<uintptr_t>(raw)})){delete raw;return false;}
    return true;
}
bool AudioEngine::playTransport(int startStep){
    if(!start())return false;if(!syncProject())return false;
    if(!events_.push({EventType::Play,-1,0,startStep,0,0}))return false;return true;
}
void AudioEngine::stopTransport() noexcept {events_.push({EventType::StopTransport,-1,0,0,0,0});}
void AudioEngine::collectRetiredSnapshots() noexcept {PlaybackSnapshot*p=nullptr;while(retiredSnapshots_.pop(p))delete p;}

void AudioEngine::copyScope(std::array<float,kScopeReadSamples>& out) const noexcept {
    const uint32_t published=scopeWrite_.load(std::memory_order_acquire);
    const uint32_t begin=published-static_cast<uint32_t>(kScopeReadSamples);
    for(size_t i=0;i<out.size();++i){
        const uint32_t index=(begin+static_cast<uint32_t>(i))%static_cast<uint32_t>(kScopeSamples);
        out[i]=static_cast<float>(scope_[index].load(std::memory_order_relaxed))/32767.0f;
    }
}

AudioEngine::VoiceSlot& AudioEngine::allocateVoice() noexcept {
    for(auto&slot:voices_)if(!slot.voice.active())return slot;
    return *std::min_element(voices_.begin(),voices_.end(),[](const VoiceSlot&a,const VoiceSlot&b){return a.voice.age()<b.voice.age();});
}
void AudioEngine::configureFx(const Fx&fx) noexcept {previewFx_.set(fx);}
void AudioEngine::configureFxForPreset(int preset) noexcept {configureFx(factoryBank()[static_cast<size_t>(clampPreset(preset))].fx);}

void AudioEngine::triggerStep(int step,double phase) noexcept {
    if(!playback_||step<0||step>=static_cast<int>(playback_->steps.size()))return;
    const auto& events=playback_->steps[static_cast<size_t>(step)].events;
    // Per-step events are sorted by fractional note offset.
    while(stepEventCursor_<events.size()&&
          static_cast<double>(events[stepEventCursor_].offsetSteps)<=phase+1.0e-9){
        const auto& e=events[stepEventCursor_++];
        auto&slot=allocateVoice();const float pressure=e.automation.velocity.count?std::clamp(e.automation.velocity.points[0].value,0.0f,1.0f):0.8f;
        const uint32_t gate=static_cast<uint32_t>(std::max<double>(sampleRate_.load(std::memory_order_relaxed)*0.09,e.lengthSteps*transportSamplesPerStep_));
        const int id=nextVoiceId_.fetch_add(1,std::memory_order_relaxed);
        slot.voice.start(id,e.patch,e.midi,pressure,gate,static_cast<float>(transportSamplesPerStep_),e.automation);slot.voice.setAge(++ageCounter_);
        slot.gainLeft=e.gainLeft;slot.gainRight=e.gainRight;slot.fxBus=e.fxBus;slot.transport=true;
    }
}

void AudioEngine::advanceTransport() noexcept {
    if(!transportPlaying_.load(std::memory_order_relaxed)||!playback_)return;
    samplesIntoStep_+=1.0;
    while(samplesIntoStep_>=transportSamplesPerStep_){
        // Flush sub-sample-late events at the step boundary before advancing.
        triggerStep(transportStep_,1.0);
        samplesIntoStep_-=transportSamplesPerStep_;
        transportStep_=(transportStep_+1)%std::max(1,playback_->lengthSteps);
        playheadStep_.store(transportStep_,std::memory_order_relaxed);
        stepEventCursor_=0;
        triggerStep(transportStep_,0.0);
    }
    // Called after rendering: a fractional attack begins on the next sample.
    triggerStep(transportStep_,samplesIntoStep_/transportSamplesPerStep_);
}

void AudioEngine::applyEvent(const Event&e) noexcept {
    switch(e.type){
        case EventType::Panic:
            for(auto&slot:voices_)slot.voice.kill();
            previewFx_.reset();masterFx_.reset();for(auto&bus:playbackFx_)bus.processor.reset();
            return;
        case EventType::Preset:configureFxForPreset(e.source);return;
        case EventType::NoteOff:
            for(auto&slot:voices_)if(slot.voice.active()&&slot.voice.id()==e.id){slot.voice.release();return;}return;
        case EventType::NoteOn:{
            auto&slot=allocateVoice();slot.voice.start(e.id,e.patch,e.midi,e.value);slot.voice.setAge(++ageCounter_);
            slot.gainLeft=e.gainLeft;slot.gainRight=e.gainRight;slot.fxBus=-1;slot.transport=false;configureFx(e.patch.fx);return;
        }
        case EventType::PadOn:{
            auto&slot=allocateVoice();slot.voice.start(e.id,e.patch,e.midi,e.value);slot.voice.setAge(++ageCounter_);
            slot.gainLeft=e.gainLeft;slot.gainRight=e.gainRight;slot.fxBus=-1;slot.transport=false;configureFx(e.patch.fx);return;
        }
        case EventType::Snapshot:{
            auto*incoming=reinterpret_cast<PlaybackSnapshot*>(e.pointer);if(!incoming)return;
            const bool wasPlaying=transportPlaying_.load(std::memory_order_relaxed);
            const int32_t previousStep=transportStep_;
            const double previousStepSamples=transportSamplesPerStep_;
            const double previousPhase=previousStepSamples>0.0
                ?std::clamp(samplesIntoStep_/previousStepSamples,0.0,1.0)
                :0.0;
            auto*old=playback_;playback_=incoming;
            playbackFxCount_=std::min<int32_t>(static_cast<int32_t>(playback_->fxBuses.size()),static_cast<int32_t>(playbackFx_.size()));
            for(int i=0;i<playbackFxCount_;++i){playbackFx_[static_cast<size_t>(i)].processor.reset();playbackFx_[static_cast<size_t>(i)].processor.set(playback_->fxBuses[static_cast<size_t>(i)]);}
            for(int i=playbackFxCount_;i<static_cast<int>(playbackFx_.size());++i)playbackFx_[static_cast<size_t>(i)].processor.reset();
            Fx masterFxCfg;masterFxCfg.distortion=0.0f;masterFxCfg.delay=0.0f;masterFxCfg.delayFeedback=0.0f;masterFxCfg.reverb=std::clamp(playback_->masterReverb,0.0f,1.0f);
            masterFx_.reset();masterFx_.set(masterFxCfg);
            transportSamplesPerStep_=static_cast<double>(std::max(1,sampleRate_.load(std::memory_order_relaxed)))*60.0/(std::max(12.0f,playback_->bpm)*std::max(1,playback_->divisions));
            stepEventCursor_=0;
            if(wasPlaying){
                transportStep_=std::clamp(
                    previousStep,0,std::max(0,playback_->lengthSteps-1));
                samplesIntoStep_=previousPhase*transportSamplesPerStep_;
                // Do not replay past events when a new snapshot is applied mid-step.
                const auto& current=playback_->steps[static_cast<size_t>(transportStep_)].events;
                while(stepEventCursor_<current.size()&&
                      current[stepEventCursor_].offsetSteps<=previousPhase)
                    ++stepEventCursor_;
                playheadStep_.store(transportStep_,std::memory_order_relaxed);
                playheadPosition_.store(
                    static_cast<float>(transportStep_)+static_cast<float>(previousPhase),
                    std::memory_order_relaxed);
            }else{
                transportStep_=0;
                samplesIntoStep_=0;
                playheadStep_.store(0,std::memory_order_relaxed);
                playheadPosition_.store(0.0f,std::memory_order_relaxed);
            }
            if(old&&!retiredSnapshots_.push(old)){/* rare UI-side collection starvation: keep old allocated rather than deleting on RT */}
            return;
        }
        case EventType::Play:
            if(!playback_)return;
            for(auto&slot:voices_)if(slot.transport)slot.voice.kill();
            for(int i=0;i<playbackFxCount_;++i)playbackFx_[static_cast<size_t>(i)].processor.reset();
            masterFx_.reset();
            {Fx masterFxCfg;masterFxCfg.distortion=0.0f;masterFxCfg.delay=0.0f;masterFxCfg.delayFeedback=0.0f;masterFxCfg.reverb=std::clamp(playback_->masterReverb,0.0f,1.0f);masterFx_.set(masterFxCfg);}
            transportStep_=std::clamp(e.source,0,std::max(0,playback_->lengthSteps-1));
            samplesIntoStep_=0;
            stepEventCursor_=0;
            playheadStep_.store(transportStep_,std::memory_order_relaxed);
            playheadPosition_.store(
                static_cast<float>(transportStep_),
                std::memory_order_relaxed);
            transportPlaying_.store(true,std::memory_order_relaxed);
            triggerStep(transportStep_,0.0);
            return;
        case EventType::StopTransport:
            transportPlaying_.store(false,std::memory_order_relaxed);for(auto&slot:voices_)if(slot.transport)slot.voice.kill();
            for(int i=0;i<playbackFxCount_;++i)playbackFx_[static_cast<size_t>(i)].processor.reset();masterFx_.reset();
            transportStep_=0;samplesIntoStep_=0;stepEventCursor_=0;
            playheadStep_.store(0,std::memory_order_relaxed);
            playheadPosition_.store(0.0f,std::memory_order_relaxed);
            return;
    }
}

std::array<float,2> AudioEngine::renderFrame() noexcept {
    std::array<float,kMaxPlaybackFxBuses> busLeft{};
    std::array<float,kMaxPlaybackFxBuses> busRight{};
    float transportLeft=0.0f,transportRight=0.0f;
    float previewLeft=0.0f,previewRight=0.0f;

    for(auto&slot:voices_){
        if(!slot.voice.active())continue;
        const float sample=slot.voice.render();
        const float l=sample*slot.gainLeft;
        const float r=sample*slot.gainRight;
        if(slot.transport&&transportPlaying_.load(std::memory_order_relaxed)){
            if(slot.fxBus>=0&&slot.fxBus<playbackFxCount_){
                busLeft[static_cast<size_t>(slot.fxBus)]+=l;
                busRight[static_cast<size_t>(slot.fxBus)]+=r;
            }else{
                transportLeft+=l;transportRight+=r;
            }
        }else{
            previewLeft+=l;previewRight+=r;
        }
    }

    if(transportPlaying_.load(std::memory_order_relaxed)&&playback_){
        for(int i=0;i<playbackFxCount_;++i){
            const auto wet=playbackFx_[static_cast<size_t>(i)].processor.processStereo(
                busLeft[static_cast<size_t>(i)],busRight[static_cast<size_t>(i)]);
            transportLeft+=wet[0];transportRight+=wet[1];
        }
        const auto masterWet=masterFx_.processStereo(transportLeft,transportRight);
        const float master=playback_->masterVolume;
        transportLeft=masterWet[0]*master;transportRight=masterWet[1]*master;
    }

    const auto preview=previewFx_.processStereo(previewLeft*0.165f,previewRight*0.165f);
    return {std::tanh(transportLeft+preview[0]),std::tanh(transportRight+preview[1])};
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream*,void*audioData,int32_t numFrames){
    Event e;while(events_.pop(e))applyEvent(e);auto*out=static_cast<float*>(audioData);
    uint32_t scopeWrite=scopeWrite_.load(std::memory_order_relaxed);
    for(int32_t i=0;i<numFrames;++i){
        const auto s=renderFrame();
        out[i*2]=s[0];out[i*2+1]=s[1];
        const float mono=std::clamp((s[0]+s[1])*0.5f,-1.0f,1.0f);
        scope_[scopeWrite%static_cast<uint32_t>(kScopeSamples)].store(
            static_cast<int32_t>(std::lround(mono*32767.0f)),
            std::memory_order_relaxed);
        ++scopeWrite;
        advanceTransport();
    }
    scopeWrite_.store(scopeWrite,std::memory_order_release);
    if(transportPlaying_.load(std::memory_order_relaxed)&&playback_){
        const double phase=transportSamplesPerStep_>0.0
            ?std::clamp(samplesIntoStep_/transportSamplesPerStep_,0.0,1.0)
            :0.0;
        playheadPosition_.store(
            static_cast<float>(transportStep_)+static_cast<float>(phase),
            std::memory_order_relaxed);
    }
    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream*,oboe::Result error){__android_log_print(ANDROID_LOG_WARN,kTag,"Audio stream closed after error: %s",oboe::convertToText(error));{std::scoped_lock lock(streamMutex_);stream_.reset();}start();}

} // namespace aiora
