#include "SongExport.h"

#include "FxProcessor.h"
#include "PlaybackSnapshot.h"
#include "ProjectCore.h"
#include "SpectrachordVoice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace aiora {
namespace {

void setError(std::string* error,const std::string& value){
    if(error)*error=value;
}

void putLe16(std::ostream& out,uint16_t v){
    const char b[2]{
        static_cast<char>(v&0xffu),
        static_cast<char>((v>>8)&0xffu)};
    out.write(b,2);
}

void putLe32(std::ostream& out,uint32_t v){
    const char b[4]{
        static_cast<char>(v&0xffu),
        static_cast<char>((v>>8)&0xffu),
        static_cast<char>((v>>16)&0xffu),
        static_cast<char>((v>>24)&0xffu)};
    out.write(b,4);
}

void putBe16(std::vector<uint8_t>& out,uint16_t v){
    out.push_back(static_cast<uint8_t>((v>>8)&0xffu));
    out.push_back(static_cast<uint8_t>(v&0xffu));
}

void putBe32(std::vector<uint8_t>& out,uint32_t v){
    out.push_back(static_cast<uint8_t>((v>>24)&0xffu));
    out.push_back(static_cast<uint8_t>((v>>16)&0xffu));
    out.push_back(static_cast<uint8_t>((v>>8)&0xffu));
    out.push_back(static_cast<uint8_t>(v&0xffu));
}

void putVarLen(std::vector<uint8_t>& out,uint32_t value){
    uint8_t buffer[5]{};
    int count=0;
    buffer[count++]=static_cast<uint8_t>(value&0x7fu);
    while((value>>=7u)!=0u){
        buffer[count++]=static_cast<uint8_t>((value&0x7fu)|0x80u);
    }
    while(count>0)out.push_back(buffer[--count]);
}

struct RenderVoice {
    SpectrachordVoice voice{};
    float gainLeft{1.0f};
    float gainRight{1.0f};
    int16_t fxBus{-1};
};

RenderVoice& allocateVoice(
    std::array<RenderVoice,48>& voices) noexcept {
    for(auto& slot:voices)if(!slot.voice.active())return slot;
    return *std::min_element(
        voices.begin(),voices.end(),
        [](const RenderVoice& a,const RenderVoice& b){
            return a.voice.age()<b.voice.age();
        });
}

float projectEndSteps(const Project& project){
    float end=0.0f;
    for(const auto& track:project.tracks){
        for(const auto& note:track.notes){
            end=std::max(end,note.startStep+std::max(1.0f,note.lengthSteps));
        }
    }
    return end;
}

struct MidiEvent {
    uint32_t tick{};
    uint8_t priority{};
    std::vector<uint8_t> bytes;
};

void addMetaText(
    std::vector<MidiEvent>& events,uint32_t tick,uint8_t type,
    const std::string& text){
    std::vector<uint8_t> bytes{0xffu,type};
    putVarLen(bytes,static_cast<uint32_t>(text.size()));
    bytes.insert(bytes.end(),text.begin(),text.end());
    events.push_back({tick,2u,std::move(bytes)});
}

std::vector<uint8_t> makeMidiTrack(
    std::vector<MidiEvent> events){
    std::sort(
        events.begin(),events.end(),
        [](const MidiEvent& a,const MidiEvent& b){
            if(a.tick!=b.tick)return a.tick<b.tick;
            return a.priority<b.priority;
        });

    std::vector<uint8_t> data;
    uint32_t lastTick=0;
    for(const auto& event:events){
        putVarLen(data,event.tick-lastTick);
        data.insert(data.end(),event.bytes.begin(),event.bytes.end());
        lastTick=event.tick;
    }

    putVarLen(data,0);
    data.insert(data.end(),{0xffu,0x2fu,0x00u});

    std::vector<uint8_t> chunk{
        static_cast<uint8_t>('M'),static_cast<uint8_t>('T'),
        static_cast<uint8_t>('r'),static_cast<uint8_t>('k')};
    putBe32(chunk,static_cast<uint32_t>(data.size()));
    chunk.insert(chunk.end(),data.begin(),data.end());
    return chunk;
}

} // namespace

bool exportProjectWav(
    const std::string& path,
    const Project& project,
    int sampleRate,
    std::string* error){

    if(path.empty()){
        setError(error,"Empty WAV export path");
        return false;
    }
    sampleRate=std::clamp(sampleRate,22050,192000);

    auto snapshot=ProjectCore::instance().makePlaybackSnapshot();
    if(!snapshot){
        setError(error,"Could not build playback snapshot");
        return false;
    }

    const double samplesPerStep=
        static_cast<double>(sampleRate)*60.0/
        (std::max(12.0f,snapshot->bpm)*std::max(1,snapshot->divisions));

    const float endSteps=std::max(
        static_cast<float>(snapshot->lengthSteps),
        projectEndSteps(project));
    constexpr double tailSeconds=4.0;
    const uint64_t totalFrames64=
        static_cast<uint64_t>(std::ceil(endSteps*samplesPerStep))+
        static_cast<uint64_t>(std::ceil(tailSeconds*sampleRate));

    const uint64_t dataBytes64=totalFrames64*4ull;
    if(dataBytes64>std::numeric_limits<uint32_t>::max()-44u){
        setError(error,"Song is too long for a standard WAV file");
        return false;
    }
    const uint32_t dataBytes=static_cast<uint32_t>(dataBytes64);

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out){
        setError(error,"Could not create WAV file");
        return false;
    }

    out.write("RIFF",4);
    putLe32(out,36u+dataBytes);
    out.write("WAVE",4);
    out.write("fmt ",4);
    putLe32(out,16u);
    putLe16(out,1u);
    putLe16(out,2u);
    putLe32(out,static_cast<uint32_t>(sampleRate));
    putLe32(out,static_cast<uint32_t>(sampleRate*4));
    putLe16(out,4u);
    putLe16(out,16u);
    out.write("data",4);
    putLe32(out,dataBytes);

    std::array<RenderVoice,48> voices{};
    for(auto& slot:voices)slot.voice.prepare(static_cast<float>(sampleRate));

    std::array<FxProcessor,kMaxPlaybackFxBuses> buses{};
    const int busCount=std::min<int>(
        static_cast<int>(snapshot->fxBuses.size()),
        static_cast<int>(buses.size()));
    for(int i=0;i<busCount;++i){
        buses[static_cast<size_t>(i)].prepare(static_cast<float>(sampleRate));
        buses[static_cast<size_t>(i)].reset();
        buses[static_cast<size_t>(i)].set(snapshot->fxBuses[static_cast<size_t>(i)]);
    }

    FxProcessor masterFx;
    masterFx.prepare(static_cast<float>(sampleRate));
    masterFx.reset();
    Fx masterCfg;
    masterCfg.distortion=0.0f;
    masterCfg.delay=0.0f;
    masterCfg.delayFeedback=0.0f;
    masterCfg.reverb=std::clamp(snapshot->masterReverb,0.0f,1.0f);
    masterFx.set(masterCfg);

    uint64_t age=0;
    int32_t nextId=1;
    int nextStep=0;

    const auto triggerStep=[&](int step){
        if(step<0||step>=static_cast<int>(snapshot->steps.size()))return;
        for(const auto& event:snapshot->steps[static_cast<size_t>(step)].events){
            auto& slot=allocateVoice(voices);
            const float pressure=event.automation.velocity.count
                ?std::clamp(event.automation.velocity.points[0].value,0.0f,1.0f)
                :0.8f;
            const uint32_t gate=static_cast<uint32_t>(std::max<double>(
                sampleRate*0.09,
                std::max(1.0f,event.lengthSteps)*samplesPerStep));
            slot.voice.start(
                nextId++,event.patch,event.midi,pressure,gate,
                static_cast<float>(samplesPerStep),event.automation);
            slot.voice.setAge(++age);
            slot.gainLeft=event.gainLeft;
            slot.gainRight=event.gainRight;
            slot.fxBus=event.fxBus;
        }
    };

    if(!snapshot->steps.empty()){
        triggerStep(0);
        nextStep=1;
    }

    std::array<int16_t,4096> pcm{};
    size_t pcmCount=0;

    for(uint64_t frame=0;frame<totalFrames64;++frame){
        while(nextStep<static_cast<int>(snapshot->steps.size()) &&
              static_cast<double>(frame)>=
                  std::llround(static_cast<double>(nextStep)*samplesPerStep)){
            triggerStep(nextStep++);
        }

        std::array<float,kMaxPlaybackFxBuses> busLeft{};
        std::array<float,kMaxPlaybackFxBuses> busRight{};
        float left=0.0f,right=0.0f;

        for(auto& slot:voices){
            if(!slot.voice.active())continue;
            const float sample=slot.voice.render();
            const float l=sample*slot.gainLeft;
            const float r=sample*slot.gainRight;
            if(slot.fxBus>=0&&slot.fxBus<busCount){
                busLeft[static_cast<size_t>(slot.fxBus)]+=l;
                busRight[static_cast<size_t>(slot.fxBus)]+=r;
            }else{
                left+=l;
                right+=r;
            }
        }

        for(int i=0;i<busCount;++i){
            const auto wet=buses[static_cast<size_t>(i)].processStereo(
                busLeft[static_cast<size_t>(i)],
                busRight[static_cast<size_t>(i)]);
            left+=wet[0];
            right+=wet[1];
        }

        const auto mastered=masterFx.processStereo(left,right);
        const float gain=snapshot->masterVolume;
        left=std::tanh(mastered[0]*gain);
        right=std::tanh(mastered[1]*gain);

        const auto toPcm=[](float v){
            const float clamped=std::clamp(v,-1.0f,1.0f);
            return static_cast<int16_t>(std::lround(clamped*32767.0f));
        };

        pcm[pcmCount++]=toPcm(left);
        pcm[pcmCount++]=toPcm(right);

        if(pcmCount==pcm.size()){
            out.write(
                reinterpret_cast<const char*>(pcm.data()),
                static_cast<std::streamsize>(pcm.size()*sizeof(int16_t)));
            pcmCount=0;
            if(!out){
                setError(error,"Could not write WAV audio data");
                return false;
            }
        }
    }

    if(pcmCount>0){
        out.write(
            reinterpret_cast<const char*>(pcm.data()),
            static_cast<std::streamsize>(pcmCount*sizeof(int16_t)));
    }
    out.flush();
    if(!out){
        setError(error,"Could not finish WAV file");
        return false;
    }
    return true;
}

bool exportProjectMidi(
    const std::string& path,
    const Project& project,
    std::string* error){

    if(path.empty()){
        setError(error,"Empty MIDI export path");
        return false;
    }

    constexpr uint16_t ppqn=480u;
    const int divisions=std::max(1,project.divisions);
    const double ticksPerStep=
        static_cast<double>(ppqn)/static_cast<double>(divisions);

    std::vector<std::vector<uint8_t>> tracks;

    {
        std::vector<MidiEvent> meta;
        const uint32_t micros=static_cast<uint32_t>(std::clamp(
            std::lround(60000000.0/std::max(12.0f,project.bpm)),
            1l,16777215l));
        meta.push_back({
            0u,1u,
            {0xffu,0x51u,0x03u,
             static_cast<uint8_t>((micros>>16)&0xffu),
             static_cast<uint8_t>((micros>>8)&0xffu),
             static_cast<uint8_t>(micros&0xffu)}});
        addMetaText(meta,0u,0x03u,"AIORA tempo");
        tracks.push_back(makeMidiTrack(std::move(meta)));
    }

    int melodicChannel=0;
    for(const auto& track:project.tracks){
        int channel=9;
        if(!track.drums){
            if(melodicChannel==9)++melodicChannel;
            channel=melodicChannel%16;
            if(channel==9)channel=(channel+1)%16;
            ++melodicChannel;
        }

        std::vector<MidiEvent> events;
        addMetaText(events,0u,0x03u,track.name.empty()?"AIORA Track":track.name);

        for(const auto& note:track.notes){
            const uint32_t start=static_cast<uint32_t>(std::max<long long>(
                0ll,std::llround(note.startStep*ticksPerStep)));
            const uint32_t length=static_cast<uint32_t>(std::max<long long>(
                1ll,std::llround(std::max(1.0f,note.lengthSteps)*ticksPerStep)));
            const uint32_t end=start+length;

            float velocity=0.8f;
            if(!note.velocity.empty())
                velocity=std::clamp(note.velocity.front().value,0.0f,1.0f);
            const uint8_t vel=static_cast<uint8_t>(std::clamp(
                static_cast<int>(std::lround(velocity*127.0f)),1,127));
            const uint8_t midi=static_cast<uint8_t>(std::clamp(note.midi,0,127));
            const uint8_t on=static_cast<uint8_t>(0x90u|(channel&0x0f));
            const uint8_t off=static_cast<uint8_t>(0x80u|(channel&0x0f));

            events.push_back({end,0u,{off,midi,0u}});
            events.push_back({start,1u,{on,midi,vel}});
        }

        tracks.push_back(makeMidiTrack(std::move(events)));
    }

    if(tracks.size()>std::numeric_limits<uint16_t>::max()){
        setError(error,"Too many tracks for MIDI export");
        return false;
    }

    std::vector<uint8_t> file{
        static_cast<uint8_t>('M'),static_cast<uint8_t>('T'),
        static_cast<uint8_t>('h'),static_cast<uint8_t>('d')};
    putBe32(file,6u);
    putBe16(file,1u);
    putBe16(file,static_cast<uint16_t>(tracks.size()));
    putBe16(file,ppqn);
    for(const auto& track:tracks)
        file.insert(file.end(),track.begin(),track.end());

    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out){
        setError(error,"Could not create MIDI file");
        return false;
    }
    out.write(
        reinterpret_cast<const char*>(file.data()),
        static_cast<std::streamsize>(file.size()));
    out.flush();
    if(!out){
        setError(error,"Could not finish MIDI file");
        return false;
    }
    return true;
}

} // namespace aiora
