#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "PlaybackSnapshot.h"
#include "SongExport.h"
#include "FxProcessor.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace aiora;

int main(){
    // New general string synthesis never relies on a named preset flag.
    const auto nebular=makeFactoryPatch(FactoryPreset::Nebular);
    const auto spectrello=makeFactoryPatch(FactoryPreset::Spectrello);
    const auto subula=makeFactoryPatch(FactoryPreset::Subula);
    const auto owns=[](const Patch& p,LinkSource source,ModTarget target){
        const auto& slots=source==LinkSource::Velocity?p.velocitySlots:p.modSlots;
        for(const auto& slot:slots)if(slot.target==target)return true;
        return false;
    };
    for(const Patch* preset:{&nebular,&spectrello,&subula}){
        assert(preset->cymbalModel==0&&!preset->timpaniMembrane);
        assert(preset->velocitySlotCount>=3&&preset->modSlotCount>=2);
        assert(owns(*preset,LinkSource::Velocity,ModTarget::StringBrightness));
        for(const auto& v:preset->velocitySlots)
            for(const auto& m:preset->modSlots)assert(v.target!=m.target);
    }
    assert(nebular.stringDecay>0&&nebular.stringHarmLoss>0);
    assert(subula.stringDecay>0&&subula.stringHarmLoss>0);
    assert(subula.ops[0].wave==Wave::Saw&&subula.ops[1].wave==Wave::Saw);
    assert(owns(nebular,LinkSource::Mod,ModTarget::StringDamping));
    assert(owns(subula,LinkSource::Mod,ModTarget::StringDamping));
    assert(!owns(spectrello,LinkSource::Mod,ModTarget::StringDamping));
    assert(owns(spectrello,LinkSource::Mod,ModTarget::Morph1));
    assert(owns(spectrello,LinkSource::Velocity,ModTarget::AmpAttack));
    assert(spectrello.stringDecay==0.0f); // The bow sustains without pluck decay.

    // Factory patches serialize the same values and V/M links that can be
    // entered manually in Spectrachord's synth controls.
    for(const Patch* preset:{&nebular,&spectrello,&subula}){
        Patch loaded;std::string patchError;
        assert(deserializePatchJson(serializePatchJson(*preset),loaded,&patchError));
        assert(std::fabs(loaded.stringDecay-preset->stringDecay)<0.0001f);
        assert(std::fabs(loaded.stringHarmLoss-preset->stringHarmLoss)<0.0001f);
        assert(std::fabs(loaded.stringBrightness-preset->stringBrightness)<0.0001f);
        assert(loaded.velocitySlotCount==preset->velocitySlotCount);
        assert(loaded.modSlotCount==preset->modSlotCount);
    }
    Patch normal=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    assert(normal.stringDecay==0&&normal.stringHarmLoss==0);
    assert(normal.stringDamping==0&&normal.stringBrightness==1);

    // At fixed pitch and velocity, mid-note M damping should choke a
    // freely ringing saw even after M is released back to zero.
    Patch model=normal;
    model.ops[0].wave=Wave::Saw;
    model.ops[0].env={.001f,.08f,1.0f,.5f};
    model.amp={.001f,.08f,1.0f,.5f};
    model.filter.cutoff=12000.0f;
    model.stringDecay=6.0f;model.stringHarmLoss=.35f;
    model.stringBrightness=.9f;model.stringDamping=0;
    model.modSlots={{ModTarget::StringDamping,0.0f,1.0f}};
    model.modSlotCount=1;
    SpectrachordVoice openString,mutedString;
    openString.prepare(12000.0f);mutedString.prepare(12000.0f);
    openString.start(751,toDspPatch(model),54,.85f,18000u,12000.0f);
    mutedString.start(751,toDspPatch(model),54,.85f,18000u,12000.0f);
    openString.setModValue(0.0f,true);
    mutedString.setModValue(0.0f,true);
    float openLate=0.0f,mutedLate=0.0f;
    for(int n=0;n<11000;++n){
        if(n==1500)mutedString.setModValue(1.0f,true);
        if(n==3000)mutedString.setModValue(0.0f,true);
        const float a=openString.render(),b=mutedString.render();
        assert(std::isfinite(a)&&std::isfinite(b));
        if(n>=8000&&n<10500){openLate+=a*a;mutedLate+=b*b;}
    }
    assert(openLate>.00001f);
    assert(mutedLate<openLate*.12f);
    // The same patch can sustain indefinitely with stringDecay zero,
    // letting bow-driven ADSR and expression supply the energy.
    model.stringDecay=0.0f;model.stringHarmLoss=0.0f;
    model.stringBrightness=1.0f;
    model.modSlots.clear();model.modSlotCount=0;
    SpectrachordVoice sustaining;
    sustaining.prepare(12000.0f);
    sustaining.start(752,toDspPatch(model),54,.85f,18000u,12000.0f);
    float sustainedPower=0.0f;
    for(int n=0;n<11000;++n){const float v=sustaining.render();if(n>9000)sustainedPower+=v*v;}
    assert(sustainedPower>.001f);
    // FX sliders must affect actual DSP, not only their UI labels.
    Fx distort{};distort.delay=0.0f;distort.reverb=0.0f;
    distort.distortion=3.0f;distort.distIn=1.0f;distort.distOut=0.25f;
    FxProcessor distortionBus;distortionBus.prepare(12000.0f);distortionBus.set(distort);
    const float reduced=distortionBus.process(0.1f)[0];
    assert(std::fabs(reduced-0.25f*std::tanh(2.5f))<0.00001f);
    distort.distOut=1.0f;distortionBus.set(distort);
    assert(distortionBus.process(0.1f)[0]>reduced*3.9f);
    distort.distIn=0.0f;distortionBus.set(distort);
    assert(std::fabs(distortionBus.process(0.1f)[0])<0.00001f);
    distort.distIn=1.0f;distort.distortion=1.0f;distortionBus.set(distort);
    const float oldDrive=distortionBus.process(0.001f)[0];
    distort.distortion=10.0f;distortionBus.set(distort);
    assert(distortionBus.process(0.001f)[0]>oldDrive*8.0f);
    distort.distortion=0.0f;distort.distIn=0.0f;distort.distOut=0.0f;
    distortionBus.set(distort);
    assert(std::fabs(distortionBus.process(0.1f)[0]-0.1f)<0.00001f);

    // Larger virtual rooms have later reflections. Longer decay values
    // retain more energy; both parameters belong to the shared processor.
    const auto firstReflection=[](float room){
        Fx fx{};fx.distortion=0;fx.delay=0;fx.reverb=1;
        fx.reverbDecay=0.8f;fx.reverbRoom=room;
        FxProcessor reverb;reverb.prepare(12000.0f);reverb.set(fx);
        int first=-1;
        for(int i=0;i<2400;++i){
            const float dry=i==0?1.0f:0.0f;
            const float wet=reverb.process(dry)[0]-dry;
            if(first<0&&std::fabs(wet)>0.00001f)first=i;
        }
        return first;
    };
    assert(firstReflection(0.5f)>0);
    assert(firstReflection(2.0f)>firstReflection(0.5f));
    const auto lateReverbEnergy=[](float decay){
        Fx fx{};fx.distortion=0;fx.delay=0;fx.reverb=1;
        fx.reverbDecay=decay;fx.reverbRoom=1.0f;
        FxProcessor reverb;reverb.prepare(12000.0f);reverb.set(fx);
        float energy=0.0f;
        for(int i=0;i<18000;++i){
            const float sample=reverb.process(i==0?1.0f:0.0f)[0];
            assert(std::isfinite(sample));
            if(i>=9000)energy+=sample*sample;
        }
        return energy;
    };
    assert(lateReverbEnergy(5.0f)>lateReverbEnergy(0.2f)*2.0f);
    Project fxProject;fxProject.masterReverb=0.65f;
    fxProject.masterReverbDecay=5.5f;fxProject.masterReverbRoom=2.1f;
    Track fxTrack;fxTrack.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    fxTrack.patch.fx.distortion=8.5f;
    fxTrack.patch.fx.distIn=2.2f;fxTrack.patch.fx.distOut=0.35f;
    fxTrack.patch.fx.reverbDecay=6.7f;fxTrack.patch.fx.reverbRoom=2.3f;
    fxProject.tracks.push_back(fxTrack);
    Project fxRestored;std::string fxError;
    assert(deserializeProjectJson(serializeProjectJson(fxProject),fxRestored,&fxError));
    assert(std::fabs(fxRestored.masterReverbDecay-5.5f)<.001f);
    assert(std::fabs(fxRestored.masterReverbRoom-2.1f)<.001f);
    assert(std::fabs(fxRestored.tracks[0].patch.fx.distortion-8.5f)<.001f);
    assert(std::fabs(fxRestored.tracks[0].patch.fx.distIn-2.2f)<.001f);
    assert(std::fabs(fxRestored.tracks[0].patch.fx.distOut-.35f)<.001f);
    assert(std::fabs(fxRestored.tracks[0].patch.fx.reverbDecay-6.7f)<.001f);
    assert(std::fabs(fxRestored.tracks[0].patch.fx.reverbRoom-2.3f)<.001f);

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
    assert(kit[0].patch.ops[0].wave==Wave::Noise&&kit[0].patch.ops[1].wave==Wave::Hiss);
    assert(kit[1].patch.ops[0].wave==Wave::Noise&&kit[1].patch.ops[3].wave==Wave::Hiss);
    assert(kit[0].patch.velocitySlotCount>=6);
    bool hasWashDecay0=false,hasWashDecay1=false;
    for(const auto& s:kit[0].patch.velocitySlots){
        hasWashDecay0|=s.target==ModTarget::Op1Decay;
        hasWashDecay1|=s.target==ModTarget::Op2Decay;
    }
    assert(hasWashDecay0&&hasWashDecay1);
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
    assert(kitRestored.tracks[0].pads[0].patch.ops[1].wave==Wave::Hiss);
    assert(kitRestored.tracks[0].pads[1].patch.ops[3].wave==Wave::Hiss);
    bool hasRestoredWashDecay=false;
    for(const auto& s:kitRestored.tracks[0].pads[0].patch.velocitySlots)
        hasRestoredWashDecay|=s.target==ModTarget::Op2Decay&&s.max>.4f;
    assert(hasRestoredWashDecay);
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
    // Hiss must be pitch responsive on ANY patch, with no cymbal flag.
    // Existing Noise must stay pitch independent and identical to legacy.
    Patch colored=kit[0].patch;
    for(size_t i=0;i<colored.ops.size();++i)colored.ops[i].enabled=(i==1);
    colored.cymbalModel=0;
    colored.timpaniMembrane=false;
    colored.velocitySlots.clear();colored.velocitySlotCount=0;
    colored.modSlots.clear();colored.modSlotCount=0;
    colored.velocityAmp=0.0f;
    colored.amp={.001f,.45f,.8f,.15f};
    colored.ops[1].env={.001f,.35f,.2f,.15f};
    auto aPatch=colored,bPatch=colored;
    bPatch.ops[1].semitoneOffset+=5.0f;
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
    Patch plainNoise=colored;
    plainNoise.ops[1].wave=Wave::Noise;
    auto detunedNoise=plainNoise;
    detunedNoise.ops[1].semitoneOffset+=5.0f;
    SpectrachordVoice noiseA,noiseB;
    noiseA.prepare(48000.0f);noiseB.prepare(48000.0f);
    noiseA.start(521,toDspPatch(plainNoise),80,.85f,9600u,48000.0f);
    noiseB.start(521,toDspPatch(detunedNoise),80,.85f,9600u,48000.0f);
    for(int f=0;f<2000;++f)
        assert(noiseA.render()==noiseB.render());

    // A standard V link now controls an individual operator decay;
    // no hidden patch-specific envelope is required for the long wash.
    Patch wash=colored;
    wash.ops[1].env={.001f,.25f,0.0f,.18f};
    wash.amp={.001f,.10f,1.0f,.20f};
    wash.velocitySlots={{ModTarget::Op2Decay,.06f,.50f}};
    wash.velocitySlotCount=1;
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
