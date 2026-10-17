#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "PlaybackSnapshot.h"
#include "SongExport.h"
#include "FxProcessor.h"
#include "RollDrawControllers.h"
#include "RollTouchRouting.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace aiora;

int main(){
    // Paused Roll recordings must not steal the first scroll/tap contact.
    using aiora::PausedRollTouch;
    using aiora::routePausedRollTouch;
    assert(routePausedRollTouch(true,true,true,true,true)
        ==PausedRollTouch::Resume);
    assert(routePausedRollTouch(true,true,true,true,false)
        ==PausedRollTouch::CompleteAndRoute);
    assert(routePausedRollTouch(false,true,true,true,true)
        ==PausedRollTouch::CompleteAndRoute);
    assert(routePausedRollTouch(false,true,true,true,false)
        ==PausedRollTouch::CompleteAndRoute);
    assert(routePausedRollTouch(true,true,false,true,true)
        ==PausedRollTouch::Ignore);
    assert(routePausedRollTouch(true,false,true,true,true)
        ==PausedRollTouch::Ignore);
    assert(routePausedRollTouch(true,true,true,false,true)
        ==PausedRollTouch::Ignore);

    // V and M are equal-travel controllers anchored at an equal radius from
    // the piano-roll corner. Zoom changes only the transverse dimensions.
    for(const auto& viewport:std::array<std::array<float,2>,3>{{
        {{820.0f,1400.0f}},{{1680.0f,540.0f}},{{1680.0f,503.0f}}
    }}){
        float firstTravel=0.0f;
        float defaultVThickness=0.0f,defaultMThickness=0.0f;
        for(int zoom=0;zoom<3;++zoom){
            const float scale=rollDrawZoomThicknessScale(zoom);
            const float gutter=90.0f*scale;
            const float header=80.0f*scale;
            const auto pair=rollControllerPair(
                14.0f,320.0f,viewport[0],viewport[1],gutter,header,
                90.0f*1.2f,80.0f*1.2f);
            const auto& v=pair.velocity;
            const auto& m=pair.modulation;
            assert(std::fabs(v.w-m.h)<.0001f);
            // Landscape may shorten the shared travel to keep both fingers
            // on screen; that length MUST remain identical at every zoom.
            assert(v.w>=275.0f&&v.w<=320.0f);
            assert(v.x+v.w<=14.0f+viewport[0]);
            assert(m.y+m.h<=320.0f+viewport[1]);
            const float cx=14.0f+gutter*.5f;
            const float cy=320.0f+header*.5f;
            assert(std::fabs((v.x-cx)-(m.y-cy))<.0001f);
            assert(v.x-cx>=120.0f);
            assert(m.y-cy>=120.0f);
            // Starting fingertips are now over 165 px apart diagonally.
            assert(std::hypot(v.x-cx,m.y-cy)>165.0f);
            assert(std::fabs((v.y+v.h*.5f)-cy)<.0001f);
            assert(std::fabs((m.x+m.w*.5f)-cx)<.0001f);
            assert(std::fabs(v.h-(header-10.0f))<.0001f);
            assert(std::fabs(m.w-(gutter-10.0f))<.0001f);
            if(zoom==0){
                firstTravel=v.w;
                defaultVThickness=v.h;
                defaultMThickness=m.w;
            }else{
                assert(v.w==firstTravel);
                assert(zoom==1
                    ?(v.h>defaultVThickness&&m.w>defaultMThickness)
                    :(v.h<defaultVThickness&&m.w<defaultMThickness));
            }
        }
    }
    // Song title belongs to the project JSON, not just the export filename.
    {
        auto& core=ProjectCore::instance();
        core.reset();
        assert(core.songName()=="Untitled Song");
        core.setSongName("  Industrial Experiment  ");
        assert(core.songName()=="Industrial Experiment");
        Project saved=core.projectCopy();
        assert(saved.name=="Industrial Experiment");
        std::string error;
        Project loaded;
        const std::string json=serializeProjectJson(saved);
        assert(json.find("\"name\":\"Industrial Experiment\"")!=std::string::npos);
        assert(deserializeProjectJson(json,loaded,&error));
        assert(loaded.name=="Industrial Experiment");
        assert(core.replaceProject(std::move(loaded),0));
        assert(core.songName()=="Industrial Experiment");
        core.reset();
        assert(core.songName()=="Untitled Song");
        // Old song JSON did not have a name key.
        std::string legacy=json;
        const auto key=legacy.find("\"name\":\"Industrial Experiment\",");
        assert(key!=std::string::npos);
        legacy.erase(key,std::string("\"name\":\"Industrial Experiment\",").size());
        Project recovered;
        assert(deserializeProjectJson(legacy,recovered,&error));
        assert(recovered.name=="Untitled Song");
    }

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
    // Typed min/max edits must preserve fractional precision and allow
    // deliberately reversed expression directions without touching V.
    assert(core.setSelectedModSlot(0,ModTarget::Cutoff,10598.5f,8739.17f,LinkSource::Mod));
    const auto editedM=core.selectedModSlot(0,LinkSource::Mod);
    assert(std::fabs(editedM.min-10598.5f)<.02f);
    assert(std::fabs(editedM.max-8739.17f)<.02f);
    const auto oldV=core.selectedModSlot(0,LinkSource::Velocity);
    assert(core.setSelectedModSlot(0,oldV.target,.234567f,.876543f,LinkSource::Velocity));
    const auto editedV=core.selectedModSlot(0,LinkSource::Velocity);
    assert(std::fabs(editedV.min-.234567f)<.000001f);
    assert(std::fabs(editedV.max-.876543f)<.000001f);
    assert(std::fabs(core.selectedModSlot(0,LinkSource::Mod).min-10598.5f)<.02f);
    const std::string editedLinks=serializePatchJson(core.selectedPatch());
    Patch reloadedLinks;
    std::string linkError;
    assert(deserializePatchJson(editedLinks,reloadedLinks,&linkError));
    assert(std::fabs(reloadedLinks.modSlots[0].max-8739.17f)<.02f);
    assert(std::fabs(reloadedLinks.velocitySlots[0].min-.234567f)<.000001f);
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
    // Custom operator envelopes remain sample-accurate and survive a patch
    // round-trip without modifying legacy ADSR settings.
    Patch pointPatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    for(auto& op:pointPatch.ops){op.enabled=false;op.level=0.0f;}
    auto& pointOp=pointPatch.ops[0];
    pointOp.enabled=true;pointOp.level=.8f;
    pointOp.wave=Wave::Sine;pointOp.env={.001f,.2f,.7f,.1f};
    pointOp.volumeCurve=legacyOperatorVolume(pointOp.env);
    pointOp.volumeCurve.sustain=false;
    pointOp.volumeCurve.releaseIndex=2;
    pointOp.volumeCurve.points[0]={0.0f,0.0f};
    pointOp.volumeCurve.points[1]={.002f,1.0f};
    pointOp.volumeCurve.points[2]={.045f,.4f};
    pointOp.volumeCurve.points[3]={.095f,0.0f};
    pointOp.pitchCurve=initialOperatorPitch();
    pointOp.pitchCurve.points[0].value=2.0f;
    pointOp.pitchCurve.points[1].value=0.0f;
    pointOp.expressionCurve=initialOperatorResponse();
    pointOp.expressionCurve.points[1].handleActive=true;
    pointOp.expressionCurve.points[1].handleValue=.2f;
    const auto pointJson=serializePatchJson(pointPatch);
    Patch decoded;assert(deserializePatchJson(pointJson,decoded));
    assert(decoded.ops[0].volumeCurve.enabled);
    assert(decoded.ops[0].volumeCurve.count==4);
    assert(decoded.ops[0].volumeCurve.sustain==false);
    assert(decoded.ops[0].pitchCurve.points[0].value==2);
    assert(decoded.ops[0].expressionCurve.points[1].handleActive);
    assert(decoded.ops[0].volumeCurve.decayIndex==1);
    assert(decoded.ops[0].volumeCurve.releaseIndex==2);
    assert(initialOperatorPitch().count==4);
    assert(initialOperatorPitch().decayIndex==1);
    assert(initialOperatorPitch().releaseIndex==2);
    assert(operatorCurveValue(decoded.ops[0].pitchCurve.points.data(),
        decoded.ops[0].pitchCurve.count,0.0f)==2.0f);
    SpectrachordVoice shortVoice;shortVoice.prepare(48000.0f);
    shortVoice.start(801,toDspPatch(pointPatch),62,.85f,48000u,48000.0f);
    float earlyEnergy=0,lateEnergy=0;
    for(int k=0;k<12000;++k){
        const float sample=shortVoice.render();assert(std::isfinite(sample));
        if(k>150&&k<1000)earlyEnergy+=sample*sample;
        if(k>9500)lateEnergy+=sample*sample;
    }
    assert(earlyEnergy>.0001f);
    assert(lateEnergy<earlyEnergy*.0001f);
    // A held sustain must remain audible after the same free-running envelope
    // would have completed, then follow its release at note-off.
    pointPatch.ops[0].volumeCurve.sustain=true;
    SpectrachordVoice held;held.prepare(48000.0f);
    held.start(802,toDspPatch(pointPatch),62,.85f,14000u,48000.0f);
    float sustainEnergy=0;
    for(int k=0;k<11000;++k){
        const float sample=held.render();assert(std::isfinite(sample));
        if(k>9000)sustainEnergy+=sample*sample;
    }
    assert(sustainEnergy>.0001f);
    // Dynamic sustain must enter its authored release when it reaches R,
    // while an early note-off must release immediately in both modes.
    Patch dynamicPatch=pointPatch;
    dynamicPatch.ops[0].volumeCurve=legacyOperatorVolume(
        {.01f,.02f,.65f,.05f});
    dynamicPatch.ops[0].volumeCurve.points[1].time=.01f;
    dynamicPatch.ops[0].volumeCurve.points[2].time=.04f;
    dynamicPatch.ops[0].volumeCurve.points[3].time=.10f;
    dynamicPatch.ops[0].volumeCurve.sustain=false;
    dynamicPatch.ops[0].modChannelsConfigured=false;
    dynamicPatch.ops[0].expressionCurve={};
    dynamicPatch.ops[0].pitchCurve={};
    SpectrachordVoice timed,earlyDynamic,earlyInfinite;
    timed.prepare(48000.0f);earlyDynamic.prepare(48000.0f);
    earlyInfinite.prepare(48000.0f);
    timed.start(911,toDspPatch(dynamicPatch),62,1,0,4800.0f);
    earlyDynamic.start(911,toDspPatch(dynamicPatch),62,1,0,4800.0f);
    dynamicPatch.ops[0].volumeCurve.sustain=true;
    earlyInfinite.start(911,toDspPatch(dynamicPatch),62,1,0,4800.0f);
    float earlyDifference=0,afterReleaseEnergy=0;
    for(int sample=0;sample<6500;++sample){
        if(sample==240){
            earlyDynamic.release();
            earlyInfinite.release();
        }
        const float t=timed.render(),d=earlyDynamic.render(),s=earlyInfinite.render();
        assert(std::isfinite(t)&&std::isfinite(d)&&std::isfinite(s));
        if(sample>=240&&sample<1700)earlyDifference+=std::fabs(d-s);
        if(sample>=5600)afterReleaseEnergy+=t*t;
        if(sample>5400)assert(std::fabs(d)<1e-5f&&std::fabs(s)<1e-5f);
    }
    assert(earlyDifference<.0001f);
    assert(afterReleaseEnergy<1e-6f);
    // Dynamic sustain retains its programmed timeline before R; it
    // must not launch the release tail early during a held note.
    dynamicPatch.ops[0].volumeCurve.sustain=false;
    SpectrachordVoice heldDynamic,heldInfinite;
    heldDynamic.prepare(48000.0f);heldInfinite.prepare(48000.0f);
    heldDynamic.start(912,toDspPatch(dynamicPatch),62,1,0,4800.0f);
    dynamicPatch.ops[0].volumeCurve.sustain=true;
    heldInfinite.start(912,toDspPatch(dynamicPatch),62,1,0,4800.0f);
    for(int sample=0;sample<1700;++sample){
        const float t=heldDynamic.render(),h=heldInfinite.render();
        assert(std::fabs(t-h)<1e-6f);
    }
    // Midpoint and handle-based shapes follow authored response values.
    auto r=initialOperatorResponse();
    r.points[1].handleActive=true;r.points[1].handleValue=0;
    assert(std::fabs(operatorCurveValue(r.points.data(),r.count,.5f))<.00001f);
    r.points[1].handleValue=.28f;
    r.points[1].midpoint=.35f;
    assert(std::fabs(operatorCurveValue(r.points.data(),r.count,.35f)-.28f)<.00001f);
    for(int i=0;i<=100;++i){
        const float t=static_cast<float>(i)/100;
        const float y=operatorCurveValue(r.points.data(),r.count,t);
        assert(y>=-.00001f&&y<=1.00001f);
    }
    // Four independently saved operator modulation channels, with M=0
    // explicitly equivalent to an unedited M automation line.
    Patch morphPatch=pointPatch;
    auto& morphOp=morphPatch.ops[0];
    morphOp.volumeCurve=legacyOperatorVolume({.002f,.20f,.7f,.1f});
    morphOp.volumeCurve.sustain=true;
    morphOp.volumeMorphCurve=legacyOperatorVolume({.18f,.20f,.7f,.1f});
    morphOp.pitchCurve=initialOperatorPitch();
    morphOp.pitchCurve.points[0].value=0.0f;
    morphOp.pitchMorphCurve=initialOperatorPitch();
    for(int j=0;j<morphOp.pitchMorphCurve.count;++j)
        morphOp.pitchMorphCurve.points[j].value=12.0f;
    morphOp.modChannelsConfigured=true;
    for(auto& c:morphOp.modChannels)c=initialOperatorModChannel();
    morphOp.modChannels[0].enabled=true;
    morphOp.modChannels[0].destination=OperatorModDestination::Morph;
    morphOp.modChannels[0].response.source=1;
    morphOp.modChannels[1].enabled=true;
    morphOp.modChannels[1].destination=OperatorModDestination::PitchMorph;
    morphOp.modChannels[1].response.source=1;
    morphOp.modChannels[2].enabled=true;
    morphOp.modChannels[2].destination=OperatorModDestination::Pitch;
    morphOp.modChannels[2].response.source=1;
    morphOp.modChannels[3].enabled=true;
    morphOp.modChannels[3].destination=OperatorModDestination::Vol;
    morphOp.modChannels[3].response.source=0;
    // Keep level at 100% in this test to isolate shape changes.
    morphOp.modChannels[3].response.points[0].value=1.0f;
    morphOp.modChannels[3].response.points[1].value=1.0f;
    const auto savedMorph=serializePatchJson(morphPatch);
    Patch decodedMorph;
    assert(deserializePatchJson(savedMorph,decodedMorph));
    assert(decodedMorph.ops[0].volumeMorphCurve.enabled);
    assert(decodedMorph.ops[0].pitchMorphCurve.enabled);
    assert(decodedMorph.ops[0].modChannelsConfigured);
    assert(decodedMorph.ops[0].modChannels[1].response.source==1);
    assert(decodedMorph.ops[0].modChannels[1].destination==OperatorModDestination::PitchMorph);
    assert(decodedMorph.ops[0].modChannels[2].destination==OperatorModDestination::Pitch);
    auto modAutomation=[](float value){
        VoiceAutomation automation;
        automation.mod.storage=std::make_shared<std::vector<DspCurvePoint>>();
        automation.mod.storage->push_back({0,value,.5f,0,false});
        automation.mod.storage->push_back({8,value,.5f,0,false});
        automation.mod.points=automation.mod.storage->data();
        automation.mod.count=2;
        return automation;
    };
    SpectrachordVoice defaultZero,explicitZero,fullyMorphed;
    defaultZero.prepare(48000.0f);
    explicitZero.prepare(48000.0f);
    fullyMorphed.prepare(48000.0f);
    defaultZero.start(951,toDspPatch(morphPatch),62,.95f,0,4800.0f);
    explicitZero.start(951,toDspPatch(morphPatch),62,.95f,0,4800.0f,modAutomation(0));
    fullyMorphed.start(951,toDspPatch(morphPatch),62,.95f,0,4800.0f,modAutomation(1));
    float earlyNormal=0,earlyMorph=0;
    for(int i=0;i<2200;++i){
        const float a=defaultZero.render(),b=explicitZero.render(),c=fullyMorphed.render();
        assert(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c));
        assert(std::fabs(a-b)<1.0e-6f);
        earlyNormal+=a*a;earlyMorph+=c*c;
    }
    assert(earlyNormal>earlyMorph*2.0f);
    // Equal-destination modulation routes average rather than sum:
    // (0 + 1) / 2 and a single fixed 0.5 channel produce the same sound.
    Patch averaging=morphPatch,half=morphPatch;
    averaging.ops[0].modChannels[0].response.points[0].value=0;
    averaging.ops[0].modChannels[0].response.points[1].value=0;
    averaging.ops[0].modChannels[1].destination=OperatorModDestination::Morph;
    averaging.ops[0].modChannels[1].response.points[0].value=1;
    averaging.ops[0].modChannels[1].response.points[1].value=1;
    averaging.ops[0].modChannels[2].enabled=false;
    half.ops[0].modChannels[0].response.points[0].value=.5f;
    half.ops[0].modChannels[0].response.points[1].value=.5f;
    half.ops[0].modChannels[1].enabled=false;
    half.ops[0].modChannels[2].enabled=false;
    SpectrachordVoice meanVoice,halfVoice;
    meanVoice.prepare(48000.0f);halfVoice.prepare(48000.0f);
    meanVoice.start(952,toDspPatch(averaging),62,.95f,0,4800.0f);
    halfVoice.start(952,toDspPatch(half),62,.95f,0,4800.0f);
    for(int i=0;i<3000;++i){
        const float a=meanVoice.render(),b=halfVoice.render();
        assert(std::fabs(a-b)<2.0e-6f);
    }
    // Evolving parametric filter: six independent time curves, four V/M
    // response channels, and per-Q-node shelf/bell/high-shelf transitions
    // must survive JSON roundtrips, including adjustable T positions.
    Patch evolvingPatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    for(auto& op:evolvingPatch.ops){op.enabled=false;op.level=0;}
    evolvingPatch.ops[0].enabled=true;evolvingPatch.ops[0].level=.8f;
    evolvingPatch.ops[0].wave=Wave::Sine;
    evolvingPatch.filter.evolving=initialEvolvingFilter();
    auto& filterModel=evolvingPatch.filter.evolving;
    assert(filterModel.configured);
    assert(filterModel.lanes[0].curve.releaseIndex==2);
    filterModel.lanes[2].shape[0]=FilterShape::LowShelf;
    filterModel.lanes[2].shape[1]=FilterShape::Bell;
    filterModel.lanes[2].shape[2]=FilterShape::HighShelf;
    filterModel.lanes[2].transition[1]=.38f;
    filterModel.lanes[2].transition[2]=.75f;
    filterModel.lanes[4].curve.sustain=false;
    filterModel.lanes[3].curve.points[0].value=.83f;
    filterModel.mods[0]=initialFilterModChannel();
    filterModel.mods[0].enabled=true;
    filterModel.mods[0].destination=0; // Frequency directly overridden by M.
    filterModel.mods[0].response.source=1;
    filterModel.mods[0].response.points[0].value=.33f;
    filterModel.mods[0].response.points[1].value=.33f;
    filterModel.mods[1]=initialFilterModChannel();
    filterModel.mods[1].enabled=true;
    filterModel.mods[1].destination=5; // Q Morph.
    filterModel.mods[1].response.source=0;
    filterModel.mods[2]=initialFilterModChannel();
    filterModel.mods[2].enabled=true;
    filterModel.mods[2].destination=2; // Direct Q, typed per-node.
    filterModel.mods[2].shape[0]=FilterShape::LowShelf;
    filterModel.mods[2].shape[1]=FilterShape::Bell;
    filterModel.mods[2].transition[1]=.25f;
    const auto evolvingJson=serializePatchJson(evolvingPatch);
    Patch evolvingDecoded;
    assert(deserializePatchJson(evolvingJson,evolvingDecoded));
    assert(evolvingDecoded.filter.evolving.configured);
    assert(evolvingDecoded.filter.evolving.lanes[2].shape[0]==FilterShape::LowShelf);
    assert(evolvingDecoded.filter.evolving.lanes[2].shape[2]==FilterShape::HighShelf);
    assert(std::fabs(evolvingDecoded.filter.evolving.lanes[2].transition[1]-.38f)<.00001f);
    assert(!evolvingDecoded.filter.evolving.lanes[4].curve.sustain);
    assert(evolvingDecoded.filter.evolving.mods[0].destination==0);
    assert(evolvingDecoded.filter.evolving.mods[0].response.source==1);
    assert(evolvingDecoded.filter.evolving.mods[1].destination==5);
    assert(evolvingDecoded.filter.evolving.mods[2].shape[1]==FilterShape::Bell);
    assert(std::fabs(evolvingDecoded.filter.evolving.mods[2].transition[1]-.25f)<.00001f);
    // The Q shape transitions are normalized, not cascaded. Exercise a
    // sustained voice across Q type changes, including live M changes.
    SpectrachordVoice smoothFilter;smoothFilter.prepare(48000.0f);
    smoothFilter.start(1101,toDspPatch(evolvingDecoded),62,.8f,0,4800.0f);
    for(int i=0;i<16000;++i){
        if(i==4000)smoothFilter.setModValue(1);
        if(i==9000)smoothFilter.setModValue(0);
        const float v=smoothFilter.render();
        assert(std::isfinite(v));
        assert(std::fabs(v)<100);
    }
    // A direct frequency Mod channel owns the parameter and bypasses both
    // time envelopes. After the smooth-in, unrelated envelope edits cannot
    // change its audio samples.
    Patch evolvingOther=evolvingDecoded;
    for(auto& point:evolvingOther.filter.evolving.lanes[0].curve.points)
        point.value=.91f;
    for(auto& point:evolvingOther.filter.evolving.lanes[3].curve.points)
        point.value=.03f;
    SpectrachordVoice directA,directB;
    directA.prepare(48000.0f);directB.prepare(48000.0f);
    directA.start(1102,toDspPatch(evolvingDecoded),62,.8f,0,4800.0f);
    directB.start(1102,toDspPatch(evolvingOther),62,.8f,0,4800.0f);
    float lateDelta=0;
    for(int i=0;i<15000;++i){
        const float a=directA.render(),b=directB.render();
        assert(std::isfinite(a)&&std::isfinite(b));
        if(i>12000)lateDelta+=std::fabs(a-b);
    }
    assert(lateDelta<.05f);
    // Newly-authored Filter and Unison start with draggable static lines,
    // not a fixed ADSR shape. Both preserve zero/neutral values.
    const auto staticFilter=initialEvolvingFilter();
    for(const auto& lane:staticFilter.lanes){
        assert(lane.curve.count==2&&lane.curve.releaseIndex==0);
        assert(lane.curve.points[0].time==0.0f);
        assert(lane.curve.points[1].time==120.0f);
        assert(lane.curve.points[0].value==lane.curve.points[1].value);
    }
    const auto staticUnison=initialUnisonCurve();
    assert(staticUnison.count==2&&staticUnison.releaseIndex==0);
    assert(staticUnison.points[0].value==0.0f&&
        staticUnison.points[1].value==0.0f);
    Patch curvedUnison=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    curvedUnison.unisonCurve=initialUnisonCurve();
    curvedUnison.unisonCurve.points[0].value=.95f;
    curvedUnison.unisonCurve.points[1].value=.95f;
    const auto unisonJson=serializePatchJson(curvedUnison);
    Patch loadedUnison;
    assert(deserializePatchJson(unisonJson,loadedUnison));
    assert(loadedUnison.unisonCurve.enabled);
    assert(loadedUnison.unisonCurve.count==2);
    assert(std::fabs(loadedUnison.unisonCurve.points[0].value-.95f)<1e-5f);
    assert(loadedUnison.glide==curvedUnison.glide);
    Patch zeroUnison=curvedUnison;
    zeroUnison.unisonCurve.points[0].value=0.0f;
    zeroUnison.unisonCurve.points[1].value=0.0f;
    SpectrachordVoice wideVoice,narrowVoice;
    wideVoice.prepare(48000);narrowVoice.prepare(48000);
    wideVoice.start(1444,toDspPatch(loadedUnison),62,.8f,0,4800);
    narrowVoice.start(1444,toDspPatch(zeroUnison),62,.8f,0,4800);
    float unisonDifference=0;
    for(int sample=0;sample<6000;++sample){
        const float a=wideVoice.render(),b=narrowVoice.render();
        assert(std::isfinite(a)&&std::isfinite(b));
        if(sample>1000)unisonDifference+=std::fabs(a-b);
    }
    assert(unisonDifference>0.001f);
    // Default volume and pitch envelopes must place decay at the second
    // anchor and the release/sustain gate at the second-to-last anchor.
    const auto defaults=legacyOperatorVolume(Envelope{});
    assert(defaults.count==4&&defaults.decayIndex==1&&
        defaults.releaseIndex==defaults.count-2);
    return 0;
}
