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
    // Every synth waveform, including Hiss, must appear in the editor's
    // shared dropdown vocabulary and survive patch serialization.
    assert(kWaveformLabels.size()==7);
    assert(std::string(kWaveformLabels[static_cast<size_t>(Wave::Hiss)])=="hiss");
    {
        auto& synth=ProjectCore::instance();
        Project waveProject;
        Track waveTrack;
        waveTrack.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        waveProject.tracks.push_back(std::move(waveTrack));
        assert(synth.replaceProject(std::move(waveProject),0));
        for(int op=0;op<6;++op){
            assert(synth.setSelectedOperatorWave(op,Wave::Hiss));
            assert(synth.selectedPatch().ops[static_cast<size_t>(op)].wave==Wave::Hiss);
        }
        Patch loaded;
        std::string parseError;
        assert(deserializePatchJson(serializePatchJson(synth.selectedPatch()),loaded,&parseError));
        for(const auto& op:loaded.ops)assert(op.wave==Wave::Hiss);
        synth.reset();
    }
    // Rack effects are ordinary, repeatable, ordered modules, not fixed slots.
    Fx rack{};rack.rackConfigured=true;rack.count=2;
    rack.chain[0]=makeFxModule(FxType::MetalAmp);
    rack.chain[1]=makeFxModule(FxType::Cabinet);
    assert(fxParamCount(FxType::MetalAmp)==8);
    assert(fxParamCount(FxType::Cabinet)==5);
    FxProcessor metalFirst,cabinetFirst,bypassed;
    metalFirst.prepare(12000);cabinetFirst.prepare(12000);bypassed.prepare(12000);
    metalFirst.set(rack);
    std::swap(rack.chain[0],rack.chain[1]);
    cabinetFirst.set(rack);
    rack.chain[0].enabled=false;bypassed.set(rack);
    double orderDifference=0,bypassDifference=0;
    for(int n=0;n<1200;++n){
        const float x=.15f*std::sin(n*.073f);
        const auto a=metalFirst.process(x),b=cabinetFirst.process(x),c=bypassed.process(x);
        assert(std::isfinite(a[0])&&std::isfinite(b[0])&&std::isfinite(c[0]));
        orderDifference+=std::fabs(a[0]-b[0]);
        bypassDifference+=std::fabs(c[0]-b[0]);
    }
    assert(orderDifference>.05&&bypassDifference>.05);
    Fx duplicates{};duplicates.rackConfigured=true;duplicates.count=2;
    duplicates.chain[0]=makeFxModule(FxType::BandEq);
    duplicates.chain[1]=makeFxModule(FxType::BandEq);
    duplicates.chain[1].params[2]=-4.5f;
    Patch testPatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    testPatch.fx=duplicates;
    Patch roundTrip;std::string rackError;
    assert(deserializePatchJson(serializePatchJson(testPatch),roundTrip,&rackError));
    assert(roundTrip.fx.rackConfigured&&roundTrip.fx.count==2);
    assert(roundTrip.fx.chain[0].type==FxType::BandEq);
    assert(roundTrip.fx.chain[1].params[2]==-4.5f);
    Project rackProject;Track rackTrack;rackTrack.patch=testPatch;
    rackTrack.name="Test";rackProject.tracks.push_back(rackTrack);
    rackProject.masterFx.rackConfigured=true;rackProject.masterFx.count=2;
    rackProject.masterFx.chain[0]=makeFxModule(FxType::Delay);
    rackProject.masterFx.chain[1]=makeFxModule(FxType::Reverb);
    Project fxRecovered;
    assert(deserializeProjectJson(serializeProjectJson(rackProject),fxRecovered,&rackError));
    assert(fxRecovered.masterFx.count==2);
    assert(fxRecovered.tracks[0].patch.fx.count==2);
    const auto oldMaster=legacyMasterFx(.45f,2.3f,1.3f);
    assert(oldMaster.count==1&&oldMaster.chain[0].type==FxType::Reverb);
    assert(std::fabs(oldMaster.chain[0].params[0]-.45f)<.0001f);
    auto& fxCore=ProjectCore::instance();
    fxCore.replaceProject(rackProject,0);
    assert(fxCore.fxRack(true).count==2);
    assert(fxCore.moveFxStage(true,0,1));
    assert(fxCore.fxRack(true).chain[0].type==FxType::Reverb);
    assert(fxCore.addFxStage(true,FxType::MetalAmp));
    assert(fxCore.setFxStageParameter(true,2,1,8.5f));
    assert(fxCore.setFxStageEnabled(true,2,false));
    assert(!fxCore.fxRack(true).chain[2].enabled);
    assert(fxCore.removeFxStage(true,2));
    assert(fxCore.addFxStage(false,FxType::Cabinet));
    assert(fxCore.fxRack(false).count==3);
    const auto snapshot=fxCore.makePlaybackSnapshot();
    assert(snapshot&&snapshot->masterEffects.count==2);
    fxCore.reset();

    // Native presets are ordinary editable Spectrachord patches.
    const auto nebular=makeFactoryPatch(FactoryPreset::Nebular);
    const auto spectrello=makeFactoryPatch(FactoryPreset::Spectrello);
    const auto subula=makeFactoryPatch(FactoryPreset::Subula);
    const auto hasInstrumentTarget=[](const auto& slots,ModTarget target){
        for(const auto& slot:slots)if(slot.target==target)return true;
        return false;
    };
    for(const Patch* patch:{&nebular,&spectrello,&subula}){
        assert(!patch->cymbalModel&&!patch->timpaniMembrane);
        assert(patch->velocitySlotCount>=3&&patch->modSlotCount>=3);
        for(const auto& v:patch->velocitySlots)
            for(const auto& m:patch->modSlots)assert(v.target!=m.target);
        const auto json=serializePatchJson(*patch);
        assert(json.find("stringDecay")==std::string::npos);
        assert(json.find("stringDamping")==std::string::npos);
        Patch restored;std::string patchError;
        assert(deserializePatchJson(json,restored,&patchError));
        assert(restored.modSlotCount==patch->modSlotCount);
        assert(restored.velocitySlotCount==patch->velocitySlotCount);
    }
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::AmpDecay));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::AmpSustain));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::AmpRelease));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::Morph1));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::Cutoff));
    assert(nebular.peak.enabled&&nebular.peak.followNote);
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::PeakRatio));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::PeakQ));
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::PeakGain));
    // A palm mute boosts the NOTE fundamental only during the short attack;
    // it must not use a sustained sub-octave or steal OP4 from V routing.
    assert(nebular.ops[3].wave==Wave::Triangle);
    assert(std::fabs(nebular.ops[3].ratio-1.0f)<.00001f);
    assert(nebular.ops[3].env.sustain==0.0f);
    assert(nebular.ops[3].env.decay>=.04f&&nebular.ops[3].env.decay<=.07f);
    assert(hasInstrumentTarget(nebular.modSlots,ModTarget::Op4));
    assert(!hasInstrumentTarget(nebular.velocitySlots,ModTarget::Op4));
    assert(hasInstrumentTarget(nebular.velocitySlots,ModTarget::Op1));
    const auto nebularLink=[&](ModTarget target){
        for(const auto& link:nebular.modSlots)
            if(link.target==target)return link;
        return ModSlot{};
    };
    const auto bodyLink=nebularLink(ModTarget::Op4);
    assert(bodyLink.max>bodyLink.min*10.0f);
    assert(nebularLink(ModTarget::AmpDecay).max>=.11f);
    assert(nebularLink(ModTarget::Cutoff).max>=1200.0f);
    {
        Patch restored;std::string err;
        assert(deserializePatchJson(serializePatchJson(nebular),restored,&err));
        assert(restored.ops[3].ratio==nebular.ops[3].ratio);
        assert(restored.peak.enabled&&restored.peak.followNote);
        assert(std::fabs(restored.peak.gainDb-nebular.peak.gainDb)<.0001f);
        assert(restored.modSlotCount==nebular.modSlotCount);
        for(size_t i=0;i<nebular.modSlotCount;++i){
            assert(restored.modSlots[i].target==nebular.modSlots[i].target);
            assert(std::fabs(restored.modSlots[i].max-nebular.modSlots[i].max)<.0001f);
        }
    }
    assert(hasInstrumentTarget(nebular.velocitySlots,ModTarget::Op3));
    assert(hasInstrumentTarget(nebular.velocitySlots,ModTarget::Op2));
    assert(hasInstrumentTarget(subula.modSlots,ModTarget::AmpDecay));
    assert(hasInstrumentTarget(subula.modSlots,ModTarget::AmpSustain));
    assert(hasInstrumentTarget(subula.modSlots,ModTarget::AmpRelease));
    assert(hasInstrumentTarget(subula.modSlots,ModTarget::Cutoff));
    assert(subula.ops[0].wave==Wave::Saw);
    assert(subula.ops[1].wave==Wave::Saw);
    assert(hasInstrumentTarget(spectrello.modSlots,ModTarget::Morph1));
    assert(hasInstrumentTarget(spectrello.modSlots,ModTarget::Op5));
    assert(hasInstrumentTarget(spectrello.modSlots,ModTarget::Op6));
    assert(hasInstrumentTarget(spectrello.velocitySlots,ModTarget::AmpAttack));

    // Existing patches without a "peak" object remain unchanged/disabled.
    {
        Patch old=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        assert(!old.peak.enabled);
        std::string oldJson=serializePatchJson(old);
        const auto begin=oldJson.find(",\"peak\":{");
        assert(begin!=std::string::npos);
        const auto end=oldJson.find('}',begin);
        assert(end!=std::string::npos);
        oldJson.erase(begin,end-begin+1);
        Patch restored;std::string error;
        assert(deserializePatchJson(oldJson,restored,&error));
        assert(!restored.peak.enabled);
    }
    // Per-voice key tracking: a peaking bell centered on each new note
    // amplifies it, whereas a fixed 220 Hz peak doesn't follow A4 (440 Hz).
    {
        Patch sine=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        for(auto& op:sine.ops){op.enabled=false;op.level=0.0f;}
        sine.ops[0].enabled=true;
        sine.ops[0].wave=Wave::Sine;
        sine.ops[0].ratio=1.0f;
        sine.ops[0].level=.1f;
        sine.ops[0].env={.002f,.01f,1.0f,.1f};
        sine.amp={.002f,.01f,1.0f,.1f};
        sine.filter={FilterType::Lowpass,8500,.707f,0,{.005f,.1f,0,.1f}};
        sine.velocityAmp=0;sine.velocityFilter=0;
        sine.modSlots.clear();sine.modSlotCount=0;
        sine.velocitySlots.clear();sine.velocitySlotCount=0;
        sine.peak={true,true,1.0f,220.0f,6.0f,12.0f};
        const auto energy=[&](Patch p,int midi){
            SpectrachordVoice voice;
            voice.prepare(24000.0f);
            voice.start(723,toDspPatch(p),midi,1.0f,24000,24000.0f);
            double sum=0.0;
            for(int sample=0;sample<4000;++sample){
                const float y=voice.render();
                assert(std::isfinite(y));
                if(sample>=1000)sum+=static_cast<double>(y)*y;
            }
            return sum;
        };
        const auto tracked=energy(sine,69);
        sine.peak.followNote=false;
        const auto offCenter=energy(sine,69);
        sine.peak.enabled=false;
        const auto bypass=energy(sine,69);
        assert(tracked>bypass*7.0);
        assert(tracked>offCenter*3.0);
        sine.peak.enabled=true;sine.peak.followNote=false;
        const auto fixedMatched=energy(sine,57);
        sine.peak.enabled=false;
        const auto fixedBypass=energy(sine,57);
        assert(fixedMatched>fixedBypass*7.0);
        // DSP copies keep the full settings, including fixed/follow mode.
        sine.peak.enabled=true;
        const auto dsp=toDspPatch(sine);
        assert(dsp.peak.enabled&&!dsp.peak.followNote&&dsp.peak.gainDb==12.0f);
    }
    // M selects a palm-muted *note articulation* at its onset.
    // Short amp decay and zero sustain must make the note percussive
    // rather than choking a previously ringing open note.
    const auto noteEnergy=[](const Patch& patch,float mod,float velocity,
                             int first,int last){
        SpectrachordVoice voice;
        voice.prepare(12000.0f);
        VoiceAutomation automation;
        automation.mod.storage=std::make_shared<std::vector<DspCurvePoint>>();
        automation.mod.storage->push_back({0.0f,mod,0.5f,mod,false});
        automation.mod.points=automation.mod.storage->data();
        automation.mod.count=1;
        voice.start(601,toDspPatch(patch),48,velocity,12000,12000.0f,automation);
        float energy=0.0f;
        for(int i=0;i<last;++i){
            const float sample=voice.render();
            assert(std::isfinite(sample));
            if(i>=first)energy+=sample*sample;
        }
        return energy;
    };
    for(const Patch* patch:{&nebular,&subula}){
        const float mutedOnset=noteEnergy(*patch,1.0f,.95f,20,320);
        const float openOnset=noteEnergy(*patch,0.0f,.95f,20,320);
        const float mutedLate=noteEnergy(*patch,1.0f,.95f,2800,4100);
        const float openLate=noteEnergy(*patch,0.0f,.95f,2800,4100);
        assert(openOnset>0.00001f&&mutedOnset>0.00001f);
        assert(openLate>0.000001f);
        assert(mutedLate<openLate*0.1f);
        const float soft=noteEnergy(*patch,0.0f,.25f,20,400);
        const float hard=noteEnergy(*patch,0.0f,1.0f,20,400);
        assert(hard>soft*1.5f);
    }
    // A bowed cello holds sustained tone; M changes bow pressure/timbre.
    const float celloSoft=noteEnergy(spectrello,0.0f,.85f,2500,3300);
    const float celloHard=noteEnergy(spectrello,1.0f,.85f,2500,3300);
    assert(celloSoft>0.000001f&&celloHard>0.000001f);
    assert(std::fabs(celloHard-celloSoft)>.000001f);
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
