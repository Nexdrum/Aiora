#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "PlaybackSnapshot.h"
#include "SongExport.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace aiora;

int main(){
    auto& core=ProjectCore::instance();
    core.reset();

    for(int i=0;i<64;++i)assert(core.addTrack(false)==i);
    assert(core.trackCount()==64);
    assert(core.selectTrack(0));
    const std::string longName="A long orchestral section name that must never be truncated by save or load";
    core.setTrackName(0,longName);

    Patch marimba=makeFactoryPatch(FactoryPreset::Marimba);
    assert(marimba.ops[2].ratio>8.0f);
    marimba.velocitySlots={{ModTarget::Op1,.08f,.5f}};
    marimba.velocitySlotCount=marimba.velocitySlots.size();
    marimba.modSlots.assign(70,{ModTarget::Cutoff,1000.0f,5000.0f});
    marimba.modSlotCount=marimba.modSlots.size();
    assert(core.replaceSelectedPatch(marimba));
    const int index=core.addNote(0,2,0.0f,5000.0f);
    assert(index>=0);
    for(int j=0;j<160;++j){
        assert(core.addCurvePoint(0,index,0,static_cast<float>(j),-24.0f+j*.25f,true)>=0);
        assert(core.addCurvePoint(0,index,1,static_cast<float>(j),j%2?0.3f:0.9f,true)>=0);
        assert(core.addCurvePoint(0,index,2,static_cast<float>(j),j%2?0.8f:0.1f,true)>=0);
    }
    assert(core.addNote(0,122,5120.0f,1.0f)>=0);
    const std::string json=serializeProjectJson(core.projectCopy());
    Project restored;
    std::string error;
    assert(deserializeProjectJson(json,restored,&error));
    assert(restored.tracks.size()==64);
    assert(restored.tracks.front().name==longName);
    assert(restored.tracks.front().notes.size()==2);
    const auto& note=restored.tracks.front().notes.front();
    assert(note.lengthSteps==5000.0f);
    assert(note.bend.size()==160 && note.velocity.size()==160 && note.mod.size()==160);
    assert(note.bend.front().value < -12.0f);
    assert(restored.tracks.front().patch.modSlotCount==70);
    assert(restored.tracks.front().patch.modSlots.size()==70);
    assert(restored.tracks.front().patch.velocitySlotCount==1);
    assert(restored.tracks.front().patch.velocitySlots.front().target==ModTarget::Op1);
    assert(restored.tracks.front().patch.ops[2].ratio>8.0f);
    assert(core.replaceProject(std::move(restored)));
    assert(core.trackCount()==64);
    // A target belongs to V or M, not both. Legacy M duplicates still load.
    assert(!core.setSelectedModSlot(0,ModTarget::Op1,0.0f,1.0f,LinkSource::Mod));
    assert(core.setSelectedModSlot(0,ModTarget::Cutoff,1300.0f,4700.0f,LinkSource::Mod));
    assert(core.addSelectedModSlot(ModTarget::Op1,LinkSource::Mod));
    assert(core.selectedModSlot(core.selectedModSlotCount()-1).target!=ModTarget::Op1);
    const auto snap=core.makePlaybackSnapshot();
    assert(snap && snap->fxBuses.size()==64);
    assert(snap->steps[0].events[0].automation.bend.count>=160);
    assert(snap->steps[0].events[0].automation.velocity.count>=160);

    Project pads;
    Track drums;
    drums.drums=true;
    for(int i=0;i<49;++i){
        DrumPad p;
        p.lowMidi=p.centerMidi=p.highMidi=38+i;
        p.name="Pad with a descriptive long name "+std::to_string(i);
        p.patch=makeFactoryPatch(FactoryPreset::Triangle);
        drums.pads.push_back(std::move(p));
    }
    Note drumNote;
    drumNote.midi=38;
    drumNote.lengthSteps=1.0f;
    drumNote.bend.push_back({0.0f,24.0f,true});
    drums.notes.push_back(std::move(drumNote));
    pads.tracks.push_back(std::move(drums));
    Project padsLoaded;
    assert(deserializeProjectJson(serializeProjectJson(pads),padsLoaded,&error));
    assert(padsLoaded.tracks[0].pads.size()==49);
    assert(padsLoaded.tracks[0].pads.back().centerMidi==86);
    assert(padsLoaded.tracks[0].pads.back().name==pads.tracks[0].pads.back().name);
    assert(core.replaceProject(padsLoaded));
    const auto padSnapshot=core.makePlaybackSnapshot();
    assert(padSnapshot->steps[0].events[0].automation.bend.count>=2);
    assert(std::fabs(padSnapshot->steps[0].events[0].automation.bend.points[1].value-24.0f)<0.001f);

    core.reset();
    const int track=core.addTrack(false);
    assert(track==0);
    Patch soft=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    soft.fx={0.0f,0.0f,0.32f,0.0f,0.0f};
    soft.amp.release=0.04f;
    assert(core.replaceSelectedPatch(std::move(soft)));
    assert(core.addNote(0,62,0.0f,1.0f)>=0);
    const std::string path="/tmp/aiora-limit-regression.wav";
    assert(exportProjectWav(path,core.projectCopy(),22050,&error,3.0f));
    std::ifstream wave(path,std::ios::binary);
    assert(wave);
    char hdr[12]{};
    wave.read(hdr,sizeof(hdr));
    assert(std::string(hdr,4)=="RIFF");
    assert(std::string(hdr+8,4)=="WAVE");
    // Nexdrum factory instruments must survive JSON and play finite signals.
    const auto& kit=nexdrumKit();
    assert(kit.size()==7);
    const auto& kick=kit[6].patch;
    assert(kick.ops[0].enabled && kick.ops[0].ratio==1.0f);
    assert(kick.ops[0].detuneCents==0.0f && kick.unison==0.0f);
    assert(kick.amp.attack<0.005f && kick.drumPitchDrop>0.0f);
    assert(kick.drumPitchDropSeconds>0.0f);
    assert(kick.fx.reverb<0.05f);
    assert(kit[0].patch.cymbalModel==1);
    assert(kit[1].patch.cymbalModel==2);
    assert(kit[0].patch.filter.cutoff<2500.0f);
    assert(kit[0].patch.timpaniMembrane);
    assert(kit[1].patch.amp.release>kit[0].patch.amp.release*1.5f);
    assert(kit[0].patch.amp.release<.8f);
    assert(kit[1].patch.amp.release<1.3f);
    assert(kit[0].patch.volume<.45f&&kit[1].patch.volume<.45f);
    assert(kit[1].patch.filter.cutoff<kit[0].patch.filter.cutoff);
    assert(kit[1].patch.velocitySlotCount>=3);
    assert(kit[0].patch.ops[0].wave==Wave::Noise&&kit[0].patch.ops[1].wave==Wave::Noise);
    assert(std::fabs(kit[0].patch.ops[1].semitoneOffset-kit[0].patch.ops[0].semitoneOffset)>=3.0f);
    assert(kit[1].patch.ops[0].wave==Wave::Noise&&kit[1].patch.ops[3].wave==Wave::Noise);
    assert(std::fabs(kit[1].patch.ops[3].semitoneOffset-kit[1].patch.ops[0].semitoneOffset)>=3.0f);
    // Natural wash responds to V; expressive M remains free for choking.
    const auto hasTarget=[](const auto& slots,ModTarget t){
        for(const auto& slot:slots)if(slot.target==t)return true;
        return false;
    };
    assert(hasTarget(kit[0].patch.velocitySlots,ModTarget::Op1));
    assert(hasTarget(kit[1].patch.velocitySlots,ModTarget::Op1));
    assert(!hasTarget(kit[0].patch.modSlots,ModTarget::Op1));
    assert(!hasTarget(kit[1].patch.modSlots,ModTarget::Op1));
    Project kitProject;
    Track kitTrack;kitTrack.name="Nexdrum";kitTrack.drums=true;
    kitTrack.pads.assign(kit.begin(),kit.end());
    kitProject.tracks.push_back(std::move(kitTrack));
    Project kitRestored;
    assert(deserializeProjectJson(serializeProjectJson(kitProject),kitRestored,&error));
    assert(kitRestored.tracks[0].pads[6].patch.drumPitchDrop==kick.drumPitchDrop);
    assert(kitRestored.tracks[0].pads[0].patch.cymbalModel==1);
    assert(kitRestored.tracks[0].pads[1].patch.cymbalModel==2);
    assert(std::fabs(kitRestored.tracks[0].pads[1].patch.amp.release-.90f)<.001f);
    assert(kitRestored.tracks[0].pads[1].patch.modSlots.back().max<1.0f);
    assert(kitRestored.tracks[0].pads[3].patch.velocitySlotCount>=2);
    assert(kitRestored.tracks[0].pads[6].patch.velocitySlotCount>=2);
    for(const auto& pad:kit){
        for(const auto& vel:pad.patch.velocitySlots)
            for(const auto& mod:pad.patch.modSlots)assert(vel.target!=mod.target);
    }
    // A high-V strike produces stronger cymbal excitation than a light tap.
    for(size_t padIndex : {size_t{0},size_t{1}}){
        const auto& pad=kit[padIndex];
        float energies[2]{};
        for(int vi=0;vi<2;++vi){
            SpectrachordVoice voice;voice.prepare(12000.0f);
            voice.start(340+vi,toDspPatch(pad.patch),pad.centerMidi,
                        vi?1.0f:0.3f,6000u,12000.0f);
            for(int sample=0;sample<2400;++sample){
                const float v=voice.render();
                assert(std::isfinite(v));
                energies[vi]+=v*v;
            }
        }
        assert(energies[0]>0.0f&&energies[1]>energies[0]*1.5f);
    }
    // The full-velocity crash should be punchy and both cymbals should
    // fade into a typical metal mix, not continue ringing for many seconds.
    SpectrachordVoice crashTail,rideTail;
    crashTail.prepare(12000.0f);rideTail.prepare(12000.0f);
    crashTail.start(380,toDspPatch(kit[0].patch),kit[0].centerMidi,.9f,1200u,12000.0f);
    rideTail.start(381,toDspPatch(kit[1].patch),kit[1].centerMidi,.9f,1200u,12000.0f);
    float crashEarly=0.0f,rideEarly=0.0f,crashLate=0.0f,rideLate=0.0f;
    for(int frame=0;frame<24000;++frame){
        const float crash=crashTail.render();
        const float ride=rideTail.render();
        assert(std::isfinite(crash)&&std::isfinite(ride));
        if(frame<2400){crashEarly+=crash*crash;rideEarly+=ride*ride;}
        if(frame>13200){crashLate+=crash*crash;rideLate+=ride*ride;}
    }
    assert(crashEarly>0.0001f&&rideEarly>0.0001f);
    assert(crashLate<crashEarly*.0001f);
    assert(rideLate<rideEarly*.0001f);
    assert(!crashTail.active()&&!rideTail.active());
    // The cymbal noise pitch/detune must change its spectrum; the old
    // independent white-noise generator ignored pitch completely.
    Patch colored=kit[0].patch;
    for(size_t i=1;i<colored.ops.size();++i)colored.ops[i].enabled=false;
    colored.velocitySlots.clear();colored.velocitySlotCount=0;
    colored.modSlots.clear();colored.modSlotCount=0;
    colored.velocityAmp=0.0f;
    auto aPatch=colored,bPatch=colored;
    bPatch.ops[0].semitoneOffset+=5.0f;
    SpectrachordVoice colorA,colorB;
    colorA.prepare(48000.0f);colorB.prepare(48000.0f);
    colorA.start(515,toDspPatch(aPatch),80,.85f,9600u,48000.0f);
    colorB.start(515,toDspPatch(bPatch),80,.85f,9600u,48000.0f);
    float coloredDifference=0.0f;
    for(int f=0;f<5000;++f){
        const float a=colorA.render(),b=colorB.render();
        assert(std::isfinite(a)&&std::isfinite(b));
        coloredDifference+=std::fabs(a-b);
    }
    assert(coloredDifference>.01f);
    // Hard crash noise should hold energy longer than a soft tap.
    // Compare late/early power normalized for inherent loudness changes.
    Patch wash=colored;
    wash.amp={.001f,.15f,1.0f,.25f};
    wash.ops[0].env={.001f,.25f,0.0f,.18f};
    float ratios[2]{};
    for(int vi=0;vi<2;++vi){
        SpectrachordVoice voice;voice.prepare(24000.0f);
        voice.start(630,toDspPatch(wash),80,vi?1.0f:.2f,16000u,24000.0f);
        float early=0.0f,late=0.0f;
        for(int f=0;f<8500;++f){
            const float y=voice.render();assert(std::isfinite(y));
            if(f>=1000&&f<2200)early+=y*y;
            if(f>=5300&&f<7000)late+=y*y;
        }
        ratios[vi]=late/std::max(early,1.0e-10f);
    }
    assert(ratios[1]>ratios[0]*1.1f);
    // Animated velocity must influence sound beyond the original V volume.
    Patch linked=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    for(auto& op:linked.ops){op.enabled=false;op.level=0.0f;}
    linked.ops[0].enabled=true;linked.ops[0].level=.5f;
    linked.ops[0].wave=Wave::Sine;linked.ops[0].env={.001f,.4f,.5f,.1f};
    linked.ops[1].enabled=true;linked.ops[1].level=0.0f;
    linked.ops[1].wave=Wave::Sine;linked.ops[1].ratio=2.0f;
    linked.ops[1].env={.001f,.4f,.5f,.1f};
    linked.amp={.001f,.4f,.5f,.1f};linked.fx={};
    linked.velocitySlots={{ModTarget::Op2,0.0f,.8f}};
    linked.velocitySlotCount=1;
    VoiceAutomation animation;
    animation.velocity.storage=std::make_shared<std::vector<DspCurvePoint>>();
    animation.velocity.storage->push_back({0.0f,0.0f,.5f,0.0f,false});
    animation.velocity.storage->push_back({1.0f,1.0f,.5f,0.0f,false});
    animation.velocity.points=animation.velocity.storage->data();animation.velocity.count=2;
    SpectrachordVoice animated,reference;
    animated.prepare(48000.0f);reference.prepare(48000.0f);
    animated.start(700,toDspPatch(linked),62,.8f,0,4800.0f,animation);
    linked.velocitySlots.clear();linked.velocitySlotCount=0;
    reference.start(700,toDspPatch(linked),62,.8f,0,4800.0f,animation);
    float expressionDifference=0.0f;
    for(int frame=0;frame<6000;++frame){
        const float a=animated.render(),b=reference.render();
        assert(std::isfinite(a)&&std::isfinite(b));
        expressionDifference+=std::fabs(a-b);
    }
    assert(expressionDifference>1.0f);
    for(size_t padIndex : {size_t{0},size_t{1},size_t{3},size_t{6}}) {
        SpectrachordVoice voice;
        voice.prepare(48000.0f);
        const auto& pad=kit[padIndex];
        voice.start(101+static_cast<int>(padIndex),
                    toDspPatch(pad.patch),pad.centerMidi,.96f,
                    9600u,9600.0f);
        float peak=0.0f, energy=0.0f;
        for(int frame=0;frame<12000;++frame) {
            const float v=voice.render();
            assert(std::isfinite(v));
            peak=std::max(peak,std::fabs(v));
            energy+=v*v;
        }
        assert(peak>0.005f && energy>0.005f);
        assert(peak<8.0f);
    }
    return 0;
}
