#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "ProjectStorage.h"
#include "FactoryPresets.h"
#include "ApprovedFactoryPatches.h"
#include "SpectrelloFactoryV2.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "PlaybackSnapshot.h"
#include "SongExport.h"
#include "FxProcessor.h"
#include "RollDrawControllers.h"
#include "RollTouchRouting.h"
#include "KineticScroll.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace aiora;

int main(){
    // Standalone drum-kit files preserve every pad synth, range, name,
    // pan, and icon without carrying song notes or song-level effects.
    {
        Track original;
        original.drums=true;
        original.name="Nexdrum regression";
        const auto& factory=nexdrumKit();
        original.pads.assign(factory.begin(),factory.end());
        assert(factoryBank().size()==4);
        assert(factoryBank()[0].name=="Subula");
        assert(factoryBank()[1].name=="Spectrello");
        assert(factoryBank()[2].name=="Nebular");
        assert(factoryBank()[3].name=="Nexdrum");
        const std::array<std::string,7> approvedNames{{
            "Crash","Ride","Roto Tom","Snare","Tom","Floor Tom","Kick"
        }};
        for(size_t i=0;i<factory.size();++i)
            assert(factory[i].name==approvedNames[i]);
        assert(std::fabs(factory[1].patch.ops[3].harm[0]-.497686f)<.00001f);
        original.selectedPad=2;
        original.pads[2].name="Small roto-tom";
        original.pads[2].pan=.37f;
        original.pads[2].volume=.68f;
        original.pads[2].patch.ops[0].randomPhase=true;
        original.pads[2].patch.ops[0].randomPhaseDegrees=11.0f;
        auto& modeled=original.pads[2].patch.ops[0];
        modeled.volumeCurve.enabled=true;
        modeled.volumeCurve.sustain=false;
        modeled.volumeCurve.count=4;
        modeled.volumeCurve.decayIndex=1;
        modeled.volumeCurve.releaseIndex=2;
        modeled.volumeCurve.points[0]={0,0};
        modeled.volumeCurve.points[1]={.001f,1};
        modeled.volumeCurve.points[2]={.13f,.3f};
        modeled.volumeCurve.points[3]={.19f,0};
        modeled.waveMorph=Wave::Triangle;
        modeled.waveMorphConfigured=true;
        modeled.modChannelsConfigured=true;
        modeled.modChannels[0]=initialOperatorModChannel();
        modeled.modChannels[0].enabled=true;
        modeled.modChannels[0].destination=OperatorModDestination::WaveMorph;
        modeled.modChannels[0].response.source=1; // M
        auto& partial=original.pads[2].patch.ops[1];
        partial.wave=Wave::Custom;
        partial.hasHarm=true;
        partial.harm[0]=1.0f;
        partial.harm[1]=.27f;
        partial.harmPhase[1]=.125f;
        partial.hasHarmMute=true;
        partial.harmMute[0]=.8f;
        partial.harmMute[1]=.4f;
        partial.harmMutePhase[1]=.2f;
        const auto serialized=serializeDrumKitJson(original);
        assert(serialized.find("aiora-drum-kit")!=std::string::npos);
        assert(serialized.find("\"notes\"")==std::string::npos);
        std::vector<DrumPad> decoded;
        std::string error;
        assert(deserializeDrumKitJson(serialized,decoded,&error));
        assert(decoded.size()==original.pads.size());
        for(size_t i=0;i<decoded.size();++i){
            const auto& a=original.pads[i];
            const auto& b=decoded[i];
            assert(a.centerMidi==b.centerMidi);
            assert(a.lowMidi==b.lowMidi&&a.highMidi==b.highMidi);
            assert(a.name==b.name&&a.icon==b.icon);
            assert(std::fabs(a.pan-b.pan)<.00001f);
            assert(std::fabs(a.volume-b.volume)<.00001f);
            // Older embedded factory patches can contain values that
            // the current patch decoder clamps. Canonicalize through the
            // same public importer, then require all modern structures,
            // harmonic phases and expression mappings to match exactly.
            Patch expected;
            assert(deserializePatchJson(serializePatchJson(a.patch),expected));
            expected.fundamentalMidi=a.centerMidi;
            assert(serializePatchJson(expected)==serializePatchJson(b.patch));
        }
        assert(!deserializeDrumKitJson(
            R"({"type":"aiora-drum-kit","version":99,"pads":[]})",decoded,&error));
        assert(decoded.size()==original.pads.size()); // failed import is atomic
        const std::string file="/tmp/aiora_drum_kit_regression.kit.json";
        assert(saveDrumKitFile(file,original,&error));
        std::vector<DrumPad> fromDisk;
        assert(loadDrumKitFile(file,fromDisk,&error));
        assert(fromDisk.size()==original.pads.size());
        std::remove(file.c_str());

        auto& core=ProjectCore::instance();
        core.reset();
        const int track=core.addTrack(true);
        const int other=core.addTrack(false);
        assert(other!=track);
        core.setSongName("Do not change the song");
        const int note=core.addNote(track,38,0,1);
        assert(note>=0);
        assert(core.selectTrack(track));
        assert(core.applyDrumKit(track,std::move(fromDisk),false,&error));
        assert(core.noteCount(track)==1);
        assert(core.songName()=="Do not change the song");
        assert(core.trackCount()==2);
        assert(!core.trackIsDrums(other));
        assert(core.padCount(track)==7);
        // A sparse kit makes append tests unambiguous.
        auto one=decoded[0];
        one.id="collision";
        one.centerMidi=38;
        one.lowMidi=one.highMidi=38;
        one.patch.fundamentalMidi=38;
        assert(core.applyDrumKit(track,{one},false,&error));
        assert(core.padCount(track)==1);
        auto extra=one;
        extra.centerMidi=42;extra.lowMidi=extra.highMidi=42;
        extra.patch.fundamentalMidi=42;
        assert(core.applyDrumKit(track,{extra},true,&error));
        assert(core.padCount(track)==2);
        assert(core.padLow(track,1)==42);
        const auto snapshot=core.projectCopy();
        assert(snapshot.tracks[track].pads[0].id!=
               snapshot.tracks[track].pads[1].id);
        assert(!core.applyDrumKit(track,{one},true,&error));
        assert(core.padCount(track)==2);
        assert(core.noteCount(track)==1); // append collision left project intact
        core.reset();
    }
    // All scroll surfaces use the same time-based fling model; a drag
    // follows precisely, quick release coasts, slow drag/tap stays still.
    {
        KineticScroll quick,slow,paused,frame30,frame120;
        quick.begin(500.0f,1000);
        assert(quick.move(400.0f,1020)==100.0f);
        assert(quick.move(300.0f,1040)==100.0f);
        quick.end(1042,true);
        assert(quick.active()&&quick.speed()>180.0f);
        float travelled=0.0f;
        for(int i=0;i<200;++i){
            const float d=quick.advance(.016f);
            assert(std::isfinite(d)&&d>=0.0f);
            travelled+=d;
        }
        assert(travelled>100.0f&&!quick.active());
        quick.begin(100.0f,2000);
        assert(!quick.active());
        quick.move(95.0f,2200);
        quick.end(2202,true);
        assert(!quick.active());
        slow.begin(100.0f,1000);
        slow.move(90.0f,1100);
        slow.end(1101,true);
        assert(!slow.active());
        paused.begin(200.0f,1000);
        paused.move(30.0f,1050);
        paused.end(1200,true);
        assert(!paused.active()); // held still before release
        frame30.begin(300.0f,1000);
        frame120.begin(300.0f,1000);
        frame30.move(100.0f,1040);
        frame120.move(100.0f,1040);
        frame30.end(1041,true);
        frame120.end(1041,true);
        float distance30=0.0f,distance120=0.0f;
        for(int i=0;i<60;++i)distance30+=frame30.advance(1.0f/30.0f);
        for(int i=0;i<240;++i)distance120+=frame120.advance(1.0f/120.0f);
        assert(std::fabs(distance30-distance120)<.01f);
        frame120.stop();
        assert(!frame120.active()&&!frame120.tracking());
    }
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
        // Primary and alternate presets are independently selectable.
        assert(synth.setSelectedOperatorWave(0,Wave::Square));
        assert(synth.setSelectedOperatorWave(0,Wave::Triangle,true));
        auto bank=synth.selectedPatch().ops[0];
        assert(bank.wave==Wave::Square);
        assert(bank.waveMorph==Wave::Triangle&&bank.waveMorphConfigured);
        assert(!synth.setSelectedHarmonic(1,0,.8f,false)); // Hiss
        assert(!synth.setSelectedHarmonicPhase(1,0,.7f,false)); // Hiss
        assert(synth.setSelectedHarmonic(0,2,.7f,false));
        assert(synth.setSelectedHarmonic(0,63,.42f,false));
        assert(synth.setSelectedHarmonicPhase(0,63,.75f,false));
        assert(!synth.setSelectedHarmonic(0,64,.4f,false));
        bank=synth.selectedPatch().ops[0];
        assert(bank.wave==Wave::Custom);
        assert(bank.waveMorph==Wave::Triangle);
        assert(bank.hasHarm&&std::fabs(bank.harm[2]-.7f)<.0001f);
        assert(std::fabs(bank.harm[63]-.42f)<.0001f);
        assert(std::fabs(bank.harmPhase[63]-.75f)<.0001f);
        assert(synth.setSelectedHarmonic(0,3,.42f,true));
        assert(synth.setSelectedHarmonicPhase(0,62,.96f,true));
        bank=synth.selectedPatch().ops[0];
        assert(bank.waveMorph==Wave::Custom);
        assert(bank.hasHarmMute&&std::fabs(bank.harmMute[3]-.42f)<.0001f);
        assert(std::fabs(bank.harmMutePhase[62]-.96f)<.0001f);
        // Save/load the authored morph spectrum BEFORE intentionally
        // replacing it with Noise. Persisted phases must be exact.
        Patch authoredLoaded;
        assert(deserializePatchJson(
            serializePatchJson(synth.selectedPatch()),authoredLoaded,&parseError));
        assert(std::fabs(authoredLoaded.ops[0].harmPhase[63]-.75f)<.0001f);
        assert(std::fabs(authoredLoaded.ops[0].harmMutePhase[62]-.96f)<.0001f);
        assert(synth.setSelectedOperatorWave(0,Wave::Noise,true));
        assert(!synth.setSelectedHarmonic(0,0,.3f,true));
        assert(synth.setSelectedOperatorWave(0,Wave::Custom,true));
        OperatorModChannel bankMod=initialOperatorModChannel();
        bankMod.enabled=true;
        bankMod.response.source=1; // M
        bankMod.destination=OperatorModDestination::WaveMorph;
        assert(synth.setSelectedOperatorModChannel(0,0,bankMod));
        const auto saved=synth.selectedPatch();
        assert(deserializePatchJson(serializePatchJson(saved),loaded,&parseError));
        assert(loaded.ops[0].wave==Wave::Custom);
        assert(loaded.ops[0].waveMorph==Wave::Custom);
        assert(loaded.ops[0].hasHarmMute);
        assert(std::fabs(loaded.ops[0].harmPhase[63]-.75f)<.0001f);
        // Switching Noise -> Custom intentionally initializes a fresh
        // spectrum; it must not resurrect an abandoned phase assignment.
        assert(std::fabs(loaded.ops[0].harmMutePhase[62])<.0001f);
        assert(loaded.ops[0].modChannels[0].destination==
            OperatorModDestination::WaveMorph);
        assert(loaded.ops[0].modChannels[0].response.source==1);
        synth.reset();
    }
    // Wave presets must render sample-identically to their 64 editable
    // amplitudes and phases, even after the preset becomes Custom.
    // Test both waveform banks; previously Saw/Square/Triangle used an
    // unrelated analytic oscillator, with an audible conversion jump.
    {
        for(const Wave preset:{Wave::Sine,Wave::Saw,Wave::Square,Wave::Triangle}){
            Patch original=makeFactoryPatch(FactoryPreset::SpectrachordInit);
            auto& op=original.ops[0];
            op.wave=preset;
            op.level=.8f;
            Patch edited=original;
            edited.ops[0].wave=Wave::Custom;
            edited.ops[0].harm=presetWaveHarmonics(preset);
            edited.ops[0].harmPhase=presetWavePhases(preset);
            edited.ops[0].hasHarm=true;
            SpectrachordVoice reference,converted;
            reference.prepare(48000);converted.prepare(48000);
            reference.start(400,toDspPatch(original),64,.8f,0,4800);
            converted.start(400,toDspPatch(edited),64,.8f,0,4800);
            float energy=0.0f;
            for(int i=0;i<3500;++i){
                const float a=reference.render(),b=converted.render();
                assert(std::isfinite(a)&&std::isfinite(b));
                assert(std::fabs(a-b)<2e-6f);
                energy+=a*a;
            }
            assert(energy>.00001f);
            // The exact same invariant also holds for the alternate bank
            // when an explicit Wave Morph V/M destination selects it.
            original.ops[0].wave=Wave::Sine;
            original.ops[0].waveMorph=preset;
            original.ops[0].waveMorphConfigured=true;
            auto& channel=original.ops[0].modChannels[0];
            original.ops[0].modChannelsConfigured=true;
            channel=initialOperatorModChannel();
            channel.enabled=true;
            channel.destination=OperatorModDestination::WaveMorph;
            channel.response.source=1;
            Patch convertedAlt=original;
            convertedAlt.ops[0].waveMorph=Wave::Custom;
            convertedAlt.ops[0].harmMute=presetWaveHarmonics(preset);
            convertedAlt.ops[0].harmMutePhase=presetWavePhases(preset);
            convertedAlt.ops[0].hasHarmMute=true;
            VoiceAutomation fullM;
            fullM.mod.storage=std::make_shared<std::vector<DspCurvePoint>>();
            fullM.mod.storage->push_back({0,1,.5f,0,false});
            fullM.mod.storage->push_back({8,1,.5f,0,false});
            fullM.mod.points=fullM.mod.storage->data();
            fullM.mod.count=2;
            SpectrachordVoice altPreset,altCustom;
            altPreset.prepare(48000);altCustom.prepare(48000);
            altPreset.start(401,toDspPatch(original),64,.8f,0,4800,fullM);
            altCustom.start(401,toDspPatch(convertedAlt),64,.8f,0,4800,fullM);
            for(int i=0;i<3500;++i){
                const float x=altPreset.render(),y=altCustom.render();
                assert(std::isfinite(x)&&std::isfinite(y));
                assert(std::fabs(x-y)<2e-6f);
            }
        }
    }
    // Band-limited waveform spectra must be built once on the non-audio
    // patch-preparation path. Repeated notes share immutable tables while
    // their envelope, phase, K/V/M and waveform crossfade stay independent.
    {
        Patch bass=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        for(auto& op:bass.ops){op.enabled=false;op.level=0;}
        auto& op=bass.ops[0];
        op.enabled=true;
        op.level=.8f;
        op.wave=Wave::Custom;
        op.hasHarm=true;
        op.harm=presetWaveHarmonics(Wave::Saw);
        op.harmPhase=presetWavePhases(Wave::Saw);
        op.waveMorph=Wave::Custom;
        op.waveMorphConfigured=true;
        op.hasHarmMute=true;
        op.harmMute=presetWaveHarmonics(Wave::Triangle);
        op.harmMutePhase=presetWavePhases(Wave::Triangle);
        op.modChannelsConfigured=true;
        for(auto& route:op.modChannels)route.enabled=false;
        op.modChannels[0]=initialOperatorModChannel();
        op.modChannels[0].enabled=true;
        op.modChannels[0].destination=OperatorModDestination::WaveMorph;
        op.modChannels[0].response.source=1;
        auto& second=bass.ops[1];
        second.enabled=true;second.level=.20f;
        second.wave=Wave::Sine;
        second.waveMorph=Wave::Sine;
        second.waveMorphConfigured=true;

        const auto first=toDspPatch(bass);
        const auto repeated=toDspPatch(bass);
        assert(first.spectralBanks.primary[0]&&first.spectralBanks.alternate[0]);
        assert(first.spectralBanks.primary[0]==repeated.spectralBanks.primary[0]);
        assert(first.spectralBanks.alternate[0]==repeated.spectralBanks.alternate[0]);
        assert(first.spectralBanks.primary[1]==repeated.spectralBanks.primary[1]);
        assert(first.spectralBanks.primary[1]==first.spectralBanks.alternate[1]);
        assert(!first.spectralBanks.primary[2]); // disabled operators need no tables

        // Editing modulation and decay should NOT rebuild any spectrum.
        Patch articulated=bass;
        articulated.ops[0].modChannels[0].response.source=2;
        articulated.ops[0].volumeCurve=legacyOperatorVolume(
            Envelope{.003f,.3f,.8f,.4f});
        const auto unchanged=toDspPatch(articulated);
        assert(first.spectralBanks.primary[0]==unchanged.spectralBanks.primary[0]);
        assert(first.spectralBanks.alternate[0]==unchanged.spectralBanks.alternate[0]);

        // Editing an alternate harmonic rebuilds just the alternate bank.
        Patch newMorph=bass;
        newMorph.ops[0].harmMute[2]=.34f;
        const auto updatedMorph=toDspPatch(newMorph);
        assert(first.spectralBanks.primary[0]==updatedMorph.spectralBanks.primary[0]);
        assert(first.spectralBanks.alternate[0]!=updatedMorph.spectralBanks.alternate[0]);
        assert(first.spectralBanks.primary[1]==updatedMorph.spectralBanks.primary[1]);
        // Phase changes must also invalidate the correct bank.
        Patch newPhase=bass;
        newPhase.ops[0].harmPhase[3]=.25f;
        const auto updatedPhase=toDspPatch(newPhase);
        assert(first.spectralBanks.primary[0]!=updatedPhase.spectralBanks.primary[0]);
        assert(first.spectralBanks.alternate[0]==updatedPhase.spectralBanks.alternate[0]);
        assert(first.spectralBanks.primary[1]==updatedPhase.spectralBanks.primary[1]);

        // A running voice owns its old banks after the patch is edited.
        VoiceAutomation fullMorph;
        fullMorph.mod.storage=std::make_shared<std::vector<DspCurvePoint>>();
        fullMorph.mod.storage->push_back({0,1,.5f,0,false});
        fullMorph.mod.storage->push_back({8,1,.5f,0,false});
        fullMorph.mod.points=fullMorph.mod.storage->data();
        fullMorph.mod.count=2;
        SpectrachordVoice previous,oldReference,edited;
        previous.prepare(48000);oldReference.prepare(48000);edited.prepare(48000);
        previous.start(8801,first,39,.8f,0,4800,fullMorph);
        oldReference.start(8801,repeated,39,.8f,0,4800,fullMorph);
        edited.start(8801,updatedMorph,39,.8f,0,4800,fullMorph);
        float changedSpectrum=0;
        for(int n=0;n<3500;++n){
            const float old=previous.render();
            const float same=oldReference.render();
            const float newer=edited.render();
            assert(std::isfinite(old)&&std::isfinite(same)&&std::isfinite(newer));
            assert(std::fabs(old-same)<1.0e-6f);
            changedSpectrum+=std::fabs(old-newer);
        }
        assert(changedSpectrum>1.0e-4f);
        // A noise operator needs no prepared tonal table.
        Patch hiss=bass;
        hiss.ops[0].wave=Wave::Hiss;
        hiss.ops[0].waveMorph=Wave::Noise;
        const auto stochastic=toDspPatch(hiss);
        assert(!stochastic.spectralBanks.primary[0]);
        assert(!stochastic.spectralBanks.alternate[0]);
        SpectrachordVoice noiseVoice;
        noiseVoice.prepare(48000);
        noiseVoice.start(8802,stochastic,39,.8f,0,4800);
        for(int n=0;n<128;++n)assert(std::isfinite(noiseVoice.render()));
    }
    // Harmonic extrapolation follows the octave-copy rule, obeys the 1/n
    // ceiling, and interpolates phase around the shortest circle arc.
    {
        HarmonicBank amps{},phases{};
        amps[32]=1.0f;amps[33]=.75f;
        phases[32]=.95f;phases[33]=.05f;
        std::array<float,kSynthHarmonics> spectrum{},angle{};
        extrapolateHarmonics(amps,phases,spectrum,angle);
        assert(kEditableHarmonics==64&&kSynthHarmonics==128);
        assert(std::fabs(spectrum[65]-1.0f/66.0f)<1e-6f);
        assert(std::fabs(spectrum[67]-1.0f/68.0f)<1e-6f);
        assert(std::fabs(angle[65]-.95f)<1e-6f);
        assert(std::fabs(angle[67]-.05f)<1e-6f);
        assert(std::fabs(angle[66])<1e-6f);
        assert(spectrum[64]<=1.0f/65.0f);
        assert(spectrum[127]<=1.0f/128.0f);
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
    // A distinct Metal Core ID preserves old distortion patches and adds
    // testable punch for low-string muted transients.
    assert(static_cast<int>(FxType::Reverb)==5);
    assert(static_cast<int>(FxType::MetalCore)==6);
    assert(fxParamCount(FxType::MetalCore)==8);
    assert(std::string(fxTypeName(FxType::MetalCore))=="Metal Core");
    Fx coreRack{};coreRack.rackConfigured=true;coreRack.count=1;
    coreRack.chain[0]=makeFxModule(FxType::MetalCore);
    assert(coreRack.chain[0].params[1]>=7.0f);
    FxProcessor lowPunch,highPunch;
    lowPunch.prepare(48000);highPunch.prepare(48000);
    coreRack.chain[0].params[3]=0.0f;lowPunch.set(coreRack);
    coreRack.chain[0].params[3]=10.0f;highPunch.set(coreRack);
    double loEnergy=0,hiEnergy=0;
    for(int n=0;n<12000;++n){
        const float env=n<4200?.30f:n<8000?.08f:0.0f;
        const float sample=env*std::sin(2*3.14159265359*110*n/48000.);
        const auto a=lowPunch.processStereo(sample,sample);
        const auto b=highPunch.processStereo(sample,sample);
        assert(std::isfinite(a[0])&&std::isfinite(b[0]));
        assert(std::isfinite(a[1])&&std::isfinite(b[1]));
        if(n>=200&&n<4200){loEnergy+=std::fabs(a[0]);hiEnergy+=std::fabs(b[0]);}
    }
    assert(hiEnergy>loEnergy*1.03);
    Patch corePatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    corePatch.fx=coreRack;
    Patch loadedCore;
    assert(deserializePatchJson(serializePatchJson(corePatch),loadedCore));
    assert(loadedCore.fx.count==1);
    assert(loadedCore.fx.chain[0].type==FxType::MetalCore);
    assert(loadedCore.fx.chain[0].params[3]==10.0f);
    // Ampere and Cabeus are independent new modules; legacy IDs and
    // serialized effect chains must remain unchanged.
    assert(static_cast<int>(FxType::Ampere)==7);
    assert(static_cast<int>(FxType::Cabeus)==8);
    assert(std::string(fxTypeName(FxType::Ampere))=="Ampere");
    assert(std::string(fxTypeName(FxType::Cabeus))=="Cabeus");
    assert(fxParamCount(FxType::Ampere)==8);
    assert(fxParamCount(FxType::Cabeus)==8);
    Fx modern{};modern.rackConfigured=true;modern.count=2;
    modern.chain[0]=makeFxModule(FxType::Ampere);
    modern.chain[1]=makeFxModule(FxType::Cabeus);
    assert(modern.chain[0].params[6]==0.0f); // soft gate is opt-in
    assert(modern.chain[1].params[0]==2.0f); // default 4x12
    Patch modernPatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    modernPatch.fx=modern;
    Patch modernLoaded;
    assert(deserializePatchJson(serializePatchJson(modernPatch),modernLoaded));
    assert(modernLoaded.fx.count==2);
    assert(modernLoaded.fx.chain[0].type==FxType::Ampere);
    assert(modernLoaded.fx.chain[1].type==FxType::Cabeus);
    Project modernProject;
    Track modernTrack;modernTrack.patch=modernPatch;
    modernProject.tracks.push_back(std::move(modernTrack));
    Project modernRestored;
    assert(deserializeProjectJson(serializeProjectJson(modernProject),modernRestored));
    assert(modernRestored.tracks[0].patch.fx.chain[1].type==FxType::Cabeus);

    // Audio-derived saturation must preserve actual amplitude dynamics:
    // soft notes remain softer than hard ones even with a high drive.
    Fx ampOnly{};ampOnly.rackConfigured=true;ampOnly.count=1;
    ampOnly.chain[0]=makeFxModule(FxType::Ampere);
    FxProcessor quietAmp,loudAmp;
    quietAmp.prepare(48000);loudAmp.prepare(48000);
    quietAmp.set(ampOnly);loudAmp.set(ampOnly);
    double quietEnergy=0,loudEnergy=0,ampDifference=0;
    for(int n=0;n<10000;++n){
        const float wave=std::sin(2.0*3.14159265359*146.832*n/48000.0);
        const float soft=.055f*wave,hard=.25f*wave;
        const auto a=quietAmp.processStereo(soft,soft*.6f);
        const auto b=loudAmp.processStereo(hard,hard*.6f);
        assert(std::isfinite(a[0])&&std::isfinite(a[1]));
        assert(std::isfinite(b[0])&&std::isfinite(b[1]));
        if(n>2400){
            quietEnergy+=std::fabs(a[0]);
            loudEnergy+=std::fabs(b[0]);
            ampDifference+=std::fabs(b[1]-a[1]);
        }
    }
    assert(quietEnergy>.1);
    assert(loudEnergy>quietEnergy*1.1);
    assert(ampDifference>.1);
    // Quiet sustained input can remain saturated but a deliberate cut must
    // decay to silence rather than leave a feedback tail or hiss source.
    float cutEnergy=0.0f;
    for(int n=0;n<10000;++n){
        const auto a=loudAmp.processStereo(0.0f,0.0f);
        if(n>8000)cutEnergy+=std::fabs(a[0])+std::fabs(a[1]);
        assert(std::isfinite(a[0])&&std::isfinite(a[1]));
    }
    assert(cutEnergy<.01f);
    Fx gated=ampOnly;
    gated.chain[0].params[6]=.8f;
    FxProcessor gatedAmp;gatedAmp.prepare(48000);gatedAmp.set(gated);
    for(int n=0;n<1200;++n)
        assert(gatedAmp.processStereo(0.0f,0.0f)[0]==0.0f);

    // Cabeus convolution has distinct 1x12/4x12 voices and passes audio
    // through correctly when its wet mix is exactly zero.
    Fx cabOnly{};cabOnly.rackConfigured=true;cabOnly.count=1;
    cabOnly.chain[0]=makeFxModule(FxType::Cabeus);
    FxProcessor smallCab,largeCab,dryCab;
    smallCab.prepare(48000);largeCab.prepare(48000);dryCab.prepare(48000);
    cabOnly.chain[0].params[0]=0.0f;smallCab.set(cabOnly);
    cabOnly.chain[0].params[0]=2.0f;largeCab.set(cabOnly);
    cabOnly.chain[0].params[6]=0.0f;dryCab.set(cabOnly);
    double cabDifference=0,cabEnergy=0;
    for(int n=0;n<7000;++n){
        const float x=.14f*std::sin(n*.12f)+.035f*std::sin(n*.39f);
        const float a=smallCab.processStereo(x,x*.65f)[0];
        const float b=largeCab.processStereo(x,x*.65f)[0];
        const float dry=dryCab.processStereo(x,x*.65f)[0];
        assert(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(dry));
        assert(std::fabs(dry-x)<1e-6f);
        if(n>256){cabDifference+=std::fabs(a-b);cabEnergy+=std::fabs(b);}
    }
    assert(cabDifference>.05&&cabEnergy>.1);
    FxProcessor ampCab,cabAmp;
    ampCab.prepare(48000);cabAmp.prepare(48000);
    ampCab.set(modern);
    std::swap(modern.chain[0],modern.chain[1]);cabAmp.set(modern);
    double modernOrderDifference=0;
    for(int n=0;n<8000;++n){
        const float x=.15f*std::sin(n*.12f);
        const auto a=ampCab.processStereo(x,x*.7f);
        const auto b=cabAmp.processStereo(x,x*.7f);
        assert(std::isfinite(a[0])&&std::isfinite(b[0]));
        if(n>256)modernOrderDifference+=std::fabs(a[0]-b[0]);
    }
    assert(modernOrderDifference>.1);
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

    // Factory Nebular/Subula/Spectrello must be the exact approved importable patches,
    // not the legacy four-operator sound designs. Compare the full serialized
    // decoded models to catch differences in phases, morphs, curves, V/M/K
    // routing, effects and the new patch-wide key map.
    const auto nebular=makeFactoryPatch(FactoryPreset::Nebular);
    const auto spectrello=makeFactoryPatch(FactoryPreset::Spectrello);
    const auto subula=makeFactoryPatch(FactoryPreset::Subula);
    const auto hasInstrumentTarget=[](const auto& slots,ModTarget target){
        for(const auto& slot:slots)if(slot.target==target)return true;
        return false;
    };
    for(const auto& entry:std::array<std::pair<FactoryPreset,std::string_view>,3>{{
        {FactoryPreset::Nebular,factory_patch_data::nebularSteelPhysicsV2},
        {FactoryPreset::Subula,factory_patch_data::subulaMetalFretlessV4},
        {FactoryPreset::Spectrello,factory_patch_data::spectrelloCarbonFiberV2}
    }}){
        Patch expected;
        std::string error;
        assert(deserializePatchJson(entry.second,expected,&error));
        const Patch actual=makeFactoryPatch(entry.first);
        expected.name=entry.first==FactoryPreset::Nebular?"Nebular":
            (entry.first==FactoryPreset::Subula?"Subula":"Spectrello");
        assert(actual.name==expected.name);
        assert(serializePatchJson(expected)==serializePatchJson(actual));
        assert(serializePatchJson(factoryBank()[static_cast<size_t>(entry.first)])==
            serializePatchJson(actual));
        assert(actual.filter.evolving.configured);
        assert(actual.ops[0].wave==Wave::Custom&&actual.ops[0].hasHarm);
        assert(actual.ops[0].modChannelsConfigured);
        assert(actual.ops[5].enabled);
        for(const auto& op:actual.ops)
            if(op.wave==Wave::Custom)assert(op.hasHarm);
        const auto serialized=serializePatchJson(actual);
        Patch roundTrip;
        assert(deserializePatchJson(serialized,roundTrip,&error));
        assert(serializePatchJson(roundTrip)==serialized);
        // Confirm even the complex patches have prepared spectral banks.
        const auto dsp=toDspPatch(actual);
        assert(dsp.spectralBanks.primary[0]!=nullptr);
    }
    assert(nebular.ops[3].wave==Wave::Hiss);
    assert(nebular.ops[5].wave==Wave::Custom);
    assert(subula.keyMapLow==19&&subula.keyMapHigh==57);
    assert(subula.ops[4].wave==Wave::Custom);
    assert(std::fabs(subula.ops[4].ratio-1.0f)<.00001f);
    assert(subula.ops[5].wave==Wave::Hiss);
    assert(effectiveWaveMorph(subula.ops[5])==Wave::Noise);
    // The five-string carbon-fiber cello uses the modern per-operator
    // V/M/K channels, natural R-point release, and an entirely dry output.
    assert(spectrello.name=="Spectrello");
    assert(!spectrello.cymbalModel&&!spectrello.timpaniMembrane);
    assert(spectrello.lfo.target==LfoTarget::None&&spectrello.lfo.amount==0.0f);
    assert(spectrello.fx.distortion==0.0f);
    assert(spectrello.fx.delay==0.0f);
    assert(spectrello.fx.reverb==0.0f);
    assert(spectrello.unison==0.0f);
    assert(spectrello.keyMapLow==36&&spectrello.keyMapHigh==88);
    assert(spectrello.ops[4].wave==Wave::Custom);
    assert(spectrello.ops[5].wave==Wave::Hiss);
    assert(effectiveWaveMorph(spectrello.ops[5])==Wave::Noise);
    for(size_t i=0;i<spectrello.ops.size();++i){
        const auto& op=spectrello.ops[i];
        assert(op.enabled&&op.modChannelsConfigured);
        assert(op.volumeCurve.enabled&&op.volumeMorphCurve.enabled);
        assert(op.volumeCurve.sustain&&op.volumeMorphCurve.sustain);
        const auto checkRelease=[](const OperatorCurve& curve){
            assert(curve.releaseIndex<curve.count-1);
            const float duration=curve.points[curve.count-1].time-
                curve.points[curve.releaseIndex].time;
            assert(duration>0.015f&&duration<=0.28f);
            assert(curve.points[curve.count-1].value==0.0f);
        };
        checkRelease(op.volumeCurve);
        checkRelease(op.volumeMorphCurve);
    }
    // OP5/OP6 cease friction promptly when the bow lifts; OP3 carries the
    // longer but still short hollow-neck resonance.
    const auto releaseSeconds=[](const OperatorCurve& c){
        return c.points[c.count-1].time-c.points[c.releaseIndex].time;
    };
    assert(releaseSeconds(spectrello.ops[4].volumeCurve)<.05f);
    assert(releaseSeconds(spectrello.ops[5].volumeCurve)<.04f);
    assert(releaseSeconds(spectrello.ops[2].volumeCurve)>.20f);

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
    // Per-operator random phase is opt-in. Old patch files stay fixed-phase,
    // saved toggles survive patch export, and the anti-click fade only
    // touches tonal operators with phase variation enabled.
    {
        Patch phasePatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        for(auto& op:phasePatch.ops){
            op.enabled=false;op.level=0.0f;op.randomPhase=false;
        }
        phasePatch.ops[0].enabled=true;
        phasePatch.ops[0].wave=Wave::Sine;
        phasePatch.ops[0].level=.85f;
        phasePatch.ops[0].ratio=1.0f;
        phasePatch.ops[0].env={.0005f,.01f,1.0f,.04f};
        phasePatch.amp={.0005f,.01f,1.0f,.04f};
        phasePatch.filter.evolving.configured=false;
        phasePatch.filter={FilterType::Lowpass,14000.0f,.707f,0.0f,
            {.005f,.1f,0,.1f}};
        phasePatch.velocityAmp=0.0f;
        phasePatch.modSlots.clear();phasePatch.modSlotCount=0;
        phasePatch.velocitySlots.clear();phasePatch.velocitySlotCount=0;
        phasePatch.masterAmpCurve={};
        phasePatch.unisonCurve={};
        phasePatch.unison=0.0f;
        const auto fixedJson=serializePatchJson(phasePatch);
        assert(fixedJson.find("\"randomPhase\":false")!=std::string::npos);
        assert(fixedJson.find("\"randomPhaseDegrees\":10")!=std::string::npos);
        // Drop all new flags to emulate a patch saved by Beta 0.7.68.
        std::string oldFormat=fixedJson;
        const std::string oldFlag=",\"randomPhase\":false";
        for(;;){
            const auto at=oldFormat.find(oldFlag);
            if(at==std::string::npos)break;
            oldFormat.erase(at,oldFlag.size());
        }
        // Prior beta files also lacked the new per-operator range field.
        const std::string rangeKey=",\"randomPhaseDegrees\":";
        for(;;){
            const auto at=oldFormat.find(rangeKey);
            if(at==std::string::npos)break;
            const auto end=oldFormat.find(',',at+rangeKey.size());
            assert(end!=std::string::npos);
            oldFormat.erase(at,end-at);
        }
        Patch oldLoaded;
        assert(deserializePatchJson(oldFormat,oldLoaded));
        for(const auto& op:oldLoaded.ops){
            assert(!op.randomPhase);
            assert(op.randomPhaseDegrees==10.0f);
        }
        SpectrachordVoice fixedA,fixedB;
        fixedA.prepare(48000.0f);fixedB.prepare(48000.0f);
        fixedA.start(4001,toDspPatch(oldLoaded),57,1.0f,10000u,10000.0f);
        fixedB.start(4029,toDspPatch(oldLoaded),57,1.0f,10000u,10000.0f);
        for(int n=0;n<512;++n){
            const float a=fixedA.render(),b=fixedB.render();
            assert(std::isfinite(a)&&std::isfinite(b));
            assert(std::fabs(a-b)<1e-6f);
        }
        phasePatch.ops[0].randomPhase=true;
        phasePatch.ops[0].randomPhaseDegrees=10.0f;
        phasePatch.ops[2].randomPhase=true; // disabled operators also serialize
        phasePatch.ops[2].randomPhaseDegrees=45.0f;
        const auto phaseJson=serializePatchJson(phasePatch);
        Patch roundTrip;
        assert(deserializePatchJson(phaseJson,roundTrip));
        assert(roundTrip.ops[0].randomPhase);
        assert(!roundTrip.ops[1].randomPhase);
        assert(roundTrip.ops[2].randomPhase);
        assert(roundTrip.ops[0].randomPhaseDegrees==10.0f);
        assert(roundTrip.ops[2].randomPhaseDegrees==45.0f);
        assert(toDspPatch(roundTrip).ops[0].randomPhase);
        assert(toDspPatch(roundTrip).ops[2].randomPhaseDegrees==45.0f);
        // Every operator retains its own range in complete song JSON too.
        Project phaseProject;
        Track phaseTrack;phaseTrack.patch=roundTrip;
        phaseProject.tracks.push_back(std::move(phaseTrack));
        Project restoredProject;
        assert(deserializeProjectJson(serializeProjectJson(phaseProject),restoredProject));
        assert(restoredProject.tracks[0].patch.ops[0].randomPhaseDegrees==10.0f);
        assert(restoredProject.tracks[0].patch.ops[2].randomPhaseDegrees==45.0f);
        // Editor setters cannot silently affect neighboring operators.
        auto& phaseCore=ProjectCore::instance();
        assert(phaseCore.replaceProject(phaseProject,0));
        assert(phaseCore.setSelectedOperatorRandomPhaseDegrees(0,20.0f));
        assert(phaseCore.setSelectedOperatorRandomPhase(1,true));
        assert(phaseCore.selectedPatch().ops[0].randomPhaseDegrees==20.0f);
        assert(phaseCore.selectedPatch().ops[1].randomPhase);
        assert(phaseCore.selectedPatch().ops[1].randomPhaseDegrees==10.0f);
        assert(phaseCore.selectedPatch().ops[2].randomPhaseDegrees==45.0f);
        assert(phaseCore.setSelectedOperatorRandomPhaseDegrees(0,999.0f));
        assert(phaseCore.selectedPatch().ops[0].randomPhaseDegrees==180.0f);
        assert(phaseCore.setSelectedOperatorRandomPhaseDegrees(0,-12.0f));
        assert(phaseCore.selectedPatch().ops[0].randomPhaseDegrees==0.0f);
        phaseCore.reset();
        SpectrachordVoice randomA,randomB;
        randomA.prepare(48000.0f);randomB.prepare(48000.0f);
        randomA.start(4001,toDspPatch(roundTrip),57,1.0f,10000u,10000.0f);
        randomB.start(4029,toDspPatch(roundTrip),57,1.0f,10000u,10000.0f);
        assert(randomA.render()==0.0f);
        assert(randomB.render()==0.0f);
        float phaseDifference=0.0f,signalEnergy=0.0f;
        for(int n=1;n<512;++n){
            const float a=randomA.render(),b=randomB.render();
            assert(std::isfinite(a)&&std::isfinite(b));
            if(n>100){
                phaseDifference+=std::fabs(a-b);
                signalEnergy+=std::fabs(a);
            }
        }
        assert(signalEnergy>.1f);
        assert(phaseDifference>.1f);
        // A disabled operator's random toggle cannot affect the output.
        phasePatch.ops[0].randomPhase=false;
        SpectrachordVoice inactivePhase;
        inactivePhase.prepare(48000.0f);
        inactivePhase.start(4029,toDspPatch(phasePatch),57,1.0f,10000u,10000.0f);
        Patch samePatch=phasePatch;
        samePatch.ops[2].randomPhase=false;
        SpectrachordVoice reference;
        reference.prepare(48000.0f);
        reference.start(4029,toDspPatch(samePatch),57,1.0f,10000u,10000.0f);
        for(int n=0;n<256;++n)
            assert(std::fabs(inactivePhase.render()-reference.render())<1e-6f);
        // A zero-degree range is audibly equivalent to fixed phase, and
        // bypasses even the opt-in 1 ms fade.
        Patch zeroAmount=samePatch;
        zeroAmount.ops[0].randomPhase=true;
        zeroAmount.ops[0].randomPhaseDegrees=0.0f;
        SpectrachordVoice zeroRange,fixedRange;
        zeroRange.prepare(48000.0f);fixedRange.prepare(48000.0f);
        zeroRange.start(5031,toDspPatch(zeroAmount),57,1.0f,10000u,10000.0f);
        fixedRange.start(5031,toDspPatch(samePatch),57,1.0f,10000u,10000.0f);
        for(int n=0;n<256;++n)
            assert(zeroRange.render()==fixedRange.render());
        // Even at the maximum +/-180 degrees every sample remains finite.
        zeroAmount.ops[0].randomPhaseDegrees=180.0f;
        SpectrachordVoice wideRange;
        wideRange.prepare(48000.0f);
        wideRange.start(5031,toDspPatch(zeroAmount),57,1.0f,10000u,10000.0f);
        assert(wideRange.render()==0.0f);
        for(int n=0;n<512;++n)assert(std::isfinite(wideRange.render()));
        // Neither Noise nor Hiss may be affected by changing this control.
        for(Wave wave:{Wave::Noise,Wave::Hiss}){
            Patch before=samePatch;
            before.ops[0].wave=wave;
            Patch after=before;
            after.ops[0].randomPhase=true;
            after.ops[0].randomPhaseDegrees=180.0f;
            SpectrachordVoice n1,n2;
            n1.prepare(48000.0f);n2.prepare(48000.0f);
            n1.start(6201,toDspPatch(before),57,1.0f,10000u,10000.0f);
            n2.start(6201,toDspPatch(after),57,1.0f,10000u,10000.0f);
            for(int n=0;n<256;++n)
                assert(n1.render()==n2.render());
        }
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
        // Approved physical instruments shape timbre and articulation with
        // velocity: higher V is not required to be 1.5x *louder* overall.
        assert(hard>0.000001f&&soft>0.000001f);
        assert(std::fabs(hard-soft)>1.0e-7f);
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

    Patch marimba=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    marimba.ops[2].ratio=9.18f;
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
        p.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
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
    // Approved factory Nexdrum is the exact imported seven-pad kit.
    // Legacy hard-coded cymbal/metal-kick assertions are deliberately removed:
    // modern patches use custom operator envelopes and M/V routed morphs.
    const auto& kit=nexdrumKit();
    assert(kit.size()==7);
    assert(kit[0].name=="Crash"&&kit[1].name=="Ride");
    assert(kit[2].name=="Roto Tom"&&kit[3].name=="Snare");
    assert(kit[4].name=="Tom"&&kit[5].name=="Floor Tom");
    assert(kit[6].name=="Kick");
    assert(kit[0].centerMidi==80&&kit[1].centerMidi==73);
    assert(kit[0].lowMidi==80&&kit[0].highMidi==86);
    assert(kit[1].lowMidi==73&&kit[1].highMidi==79);
    for(size_t i=0;i<kit.size();++i) {
        assert(kit[i].centerMidi>=kit[i].lowMidi &&
               kit[i].centerMidi<=kit[i].highMidi);
        assert(kit[i].patch.fundamentalMidi==kit[i].centerMidi);
    }
    assert(kit[0].patch.filter.evolving.configured);
    assert(kit[1].patch.filter.evolving.configured);
    assert(kit[0].patch.masterAmpCurve.enabled);
    assert(kit[1].patch.masterAmpCurve.enabled);
    assert(kit[0].patch.ops[0].enabled&&kit[1].patch.ops[0].enabled);
    assert(kit[0].patch.ops[3].wave==Wave::Custom);
    assert(kit[1].patch.ops[3].wave==Wave::Custom);
    assert(std::fabs(kit[1].patch.ops[3].harm[0]-.497686f)<.00001f);
    assert(kit[1].patch.masterAmpCurve.points[
        kit[1].patch.masterAmpCurve.count-1].time>
        kit[0].patch.masterAmpCurve.points[
        kit[0].patch.masterAmpCurve.count-1].time);
    Project kitProject;
    Track kitTrack;kitTrack.name="Nexdrum";kitTrack.drums=true;
    kitTrack.pads.assign(kit.begin(),kit.end());
    kitProject.tracks.push_back(std::move(kitTrack));
    Project kitRestored;
    assert(deserializeProjectJson(serializeProjectJson(kitProject),kitRestored,&error));
    assert(kitRestored.tracks.size()==1);
    assert(kitRestored.tracks[0].pads.size()==7);
    for(size_t i=0;i<kit.size();++i){
        const auto& restored=kitRestored.tracks[0].pads[i];
        assert(restored.name==kit[i].name);
        assert(restored.centerMidi==kit[i].centerMidi);
        assert(restored.lowMidi==kit[i].lowMidi);
        assert(restored.highMidi==kit[i].highMidi);
    }
    assert(std::fabs(kitRestored.tracks[0].pads[1].patch.ops[3].harm[0]-
                     .497686f)<.00001f);
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
    // Quiet tails may last beyond two seconds, especially on a ride.
    // They should diminish and terminate without nonfinite samples.
    assert(crashLate<crashEarly*2.0f);
    assert(rideLate<rideEarly*2.0f);
    for(int frame=24000;frame<60000;++frame) {
        assert(std::isfinite(crashTail.render()));
        assert(std::isfinite(rideTail.render()));
    }
    assert(!crashTail.active()&&!rideTail.active());
    // Hiss must be pitch responsive on ANY patch, with no cymbal flag.
    // Existing Noise must stay pitch independent and identical to legacy.
    // Isolate generic Hiss-vs-Noise pitch behavior from the approved
    // crash patch's multi-operator morph and FM configurations.
    Patch colored=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    colored.ops[1].wave=Wave::Hiss;
    colored.ops[1].level=.28f;
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
    assert(filterModel.lanes[0].curve.releaseIndex==0);
    // Convert Q from the default static line to a four-point envelope
    // to exercise multiple type transitions across saved Q nodes.
    auto& qTime=filterModel.lanes[2].curve;
    qTime.count=4;qTime.releaseIndex=2;qTime.decayIndex=1;
    qTime.points[0]={0,.5f};
    qTime.points[1]={.04f,.5f};
    qTime.points[2]={.12f,.6f};
    qTime.points[3]={.3f,.5f};
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
    // Frequency Key Track, three distinct editable response curves, and
    // the independent master amplitude envelope must round-trip.
    Patch customResponse=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    for(auto& op:customResponse.ops){op.enabled=false;op.level=0;}
    customResponse.ops[0].enabled=true;
    customResponse.ops[0].level=.8f;
    customResponse.ops[0].wave=Wave::Saw;
    customResponse.filter.evolving=initialEvolvingFilter();
    auto& evolving=customResponse.filter.evolving;
    evolving.keyTrack=true;
    evolving.slopes[2]=initialFilterSlope(2);
    evolving.slopes[2].enabled=true;
    evolving.lanes[2].shape[0]=FilterShape::HighShelf;
    evolving.lanes[2].shape[1]=FilterShape::HighShelf;
    evolving.lanes[1].curve.points[0].value=0;
    evolving.lanes[1].curve.points[1].value=0;
    // Custom lowpass-like profile has zero gain past the cutoff, instead
    // of retaining a finite -24 dB high-shelf plateau.
    assert(evolving.slopes[2].profile.points[4].value==0);
    customResponse.masterAmpCurve=legacyOperatorVolume(
        Envelope{.001f,.04f,.5f,.07f});
    const auto responseJson=serializePatchJson(customResponse);
    Patch responseDecoded;
    assert(deserializePatchJson(responseJson,responseDecoded));
    assert(responseDecoded.filter.evolving.keyTrack);
    assert(responseDecoded.filter.evolving.slopes[2].enabled);
    assert(responseDecoded.filter.evolving.slopes[2].profile.count==5);
    assert(responseDecoded.masterAmpCurve.enabled);
    assert(responseDecoded.masterAmpCurve.releaseIndex==2);
    // Type and response are restored without changing the saved legacy EQ.
    assert(responseDecoded.peak.enabled==customResponse.peak.enabled);
    assert(responseDecoded.filter.type==customResponse.filter.type);
    SpectrachordVoice followedC,followedA,flatC;
    followedC.prepare(48000);followedA.prepare(48000);flatC.prepare(48000);
    followedC.start(1730,toDspPatch(responseDecoded),48,.8f,0,4800);
    followedA.start(1730,toDspPatch(responseDecoded),69,.8f,0,4800);
    Patch fixedCutoff=responseDecoded;fixedCutoff.filter.evolving.keyTrack=false;
    flatC.start(1730,toDspPatch(fixedCutoff),48,.8f,0,4800);
    float frequencyDifference=0;
    float trackedNoteDifference=0;
    for(int i=0;i<6000;++i){
        const float a=followedC.render(),b=followedA.render(),c=flatC.render();
        assert(std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c));
        assert(std::fabs(a)<50&&std::fabs(b)<50&&std::fabs(c)<50);
        frequencyDifference+=std::fabs(a-c);
        trackedNoteDifference+=std::fabs(a-b);
    }
    assert(frequencyDifference>1.0f);
    assert(trackedNoteDifference>1.0f);
    // The master envelope multiplies the combined output after operators,
    // even if they use individually authored volume envelopes.
    Patch masterOn=responseDecoded,masterOff=responseDecoded;
    masterOn.filter.evolving=EvolvingFilter{};
    masterOff.filter.evolving=EvolvingFilter{};
    masterOn.ops[0].volumeCurve=legacyOperatorVolume(
        Envelope{.001f,.03f,1.0f,.08f});
    masterOff.ops[0].volumeCurve=masterOn.ops[0].volumeCurve;
    masterOn.masterAmpCurve=initialUnisonCurve(0.0f);
    masterOff.masterAmpCurve={};
    SpectrachordVoice masterMuted,masterNormal;
    masterMuted.prepare(48000);masterNormal.prepare(48000);
    masterMuted.start(1790,toDspPatch(masterOn),57,.8f,0,4800);
    masterNormal.start(1790,toDspPatch(masterOff),57,.8f,0,4800);
    float silentPower=0,normalPower=0;
    for(int i=0;i<3000;++i){
        const float s=masterMuted.render(),n=masterNormal.render();
        silentPower+=s*s;normalPower+=n*n;
    }
    assert(silentPower<1e-9f&&normalPower>1e-5f);
    // Wave Morph is a fifth, universal operator V/M destination.
    // A legacy patch-level Morph1 link alone must no longer morph waves.
    {
        Patch raw=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        raw.ops[0].wave=Wave::Saw;
        raw.ops[0].waveMorph=Wave::Square;
        raw.ops[0].waveMorphConfigured=true;
        raw.modSlots={{ModTarget::Morph1,0.0f,1.0f}};
        raw.modSlotCount=1;
        VoiceAutomation fullMod;
        fullMod.mod.storage=std::make_shared<std::vector<DspCurvePoint>>();
        fullMod.mod.storage->push_back({0,1,.5f,0,false});
        fullMod.mod.storage->push_back({8,1,.5f,0,false});
        fullMod.mod.points=fullMod.mod.storage->data();
        fullMod.mod.count=2;
        SpectrachordVoice legacyOff,legacyOn;
        legacyOff.prepare(48000);legacyOn.prepare(48000);
        legacyOff.start(2180,toDspPatch(raw),62,.9f,0,4800);
        legacyOn.start(2180,toDspPatch(raw),62,.9f,0,4800,fullMod);
        for(int i=0;i<2500;++i)
            assert(std::fabs(legacyOff.render()-legacyOn.render())<1e-5f);
        auto& op=raw.ops[0];
        op.modChannelsConfigured=true;
        op.modChannels[0]=initialOperatorModChannel();
        op.modChannels[0].enabled=true;
        op.modChannels[0].destination=OperatorModDestination::WaveMorph;
        op.modChannels[0].response.source=1;
        const std::string json=serializePatchJson(raw);
        Patch converted;
        assert(deserializePatchJson(json,converted));
        assert(converted.ops[0].modChannels[0].destination==
            OperatorModDestination::WaveMorph);
        assert(converted.ops[0].waveMorph==Wave::Square);
        SpectrachordVoice primary,secondary;
        primary.prepare(48000);secondary.prepare(48000);
        primary.start(2181,toDspPatch(converted),62,.9f,0,4800);
        secondary.start(2181,toDspPatch(converted),62,.9f,0,4800,fullMod);
        float changed=0;
        for(int i=0;i<2500;++i){
            const float a=primary.render(),b=secondary.render();
            assert(std::isfinite(a)&&std::isfinite(b));
            changed+=std::fabs(a-b);
        }
        assert(changed>1.0f);
    }
    // Six channels per operator and filter, with played-key K mapping.
    // Old four-channel presets retain their first four routes unchanged.
    {
        static_assert(std::tuple_size<decltype(Operator{}.modChannels)>::value==6);
        static_assert(std::tuple_size<decltype(EvolvingFilter{}.mods)>::value==6);
        assert(normalizedKeyMap(24,36,72)==0.0f);
        assert(normalizedKeyMap(90,36,72)==1.0f);
        assert(std::fabs(normalizedKeyMap(54,36,72)-.5f)<1e-6f);

        Patch k=makeFactoryPatch(FactoryPreset::SpectrachordInit);
        for(auto& op:k.ops){op.enabled=false;op.level=0;}
        auto& tone=k.ops[0];
        tone.enabled=true;tone.level=.8f;tone.wave=Wave::Sine;
        tone.env={.001f,.12f,.95f,.08f};
        tone.volumeCurve={};tone.volumeMorphCurve={};
        tone.modChannelsConfigured=true;
        for(auto& mod:tone.modChannels)mod=initialOperatorModChannel();
        tone.modChannels[0].enabled=true;
        tone.modChannels[0].destination=OperatorModDestination::Vol;
        tone.modChannels[0].response.source=2; // played key K
        // Slot six must persist through JSON, not be silently truncated.
        tone.modChannels[5].enabled=true;
        tone.modChannels[5].destination=OperatorModDestination::Vol;
        tone.modChannels[5].response.source=2;
        k.keyMapLow=36;k.keyMapHigh=48;
        k.filter.evolving={};
        k.filter.cutoff=18000.0f;k.velocityAmp=0;k.velocityFilter=0;
        k.masterAmpCurve={};
        const auto keyJson=serializePatchJson(k);
        Patch decoded;
        assert(deserializePatchJson(keyJson,decoded));
        assert(decoded.keyMapLow==36&&decoded.keyMapHigh==48);
        assert(decoded.ops[0].modChannels[5].enabled);
        assert(decoded.ops[0].modChannels[5].response.source==2);
        const auto dsp=toDspPatch(decoded);
        assert(dsp.keyMapLow==36&&dsp.keyMapHigh==48);

        // A historical JSON without the new keyMap field uses all 128 keys.
        auto legacyKeyJson=keyJson;
        const auto keyAt=legacyKeyJson.find(",\"keyMap\":");
        const auto ampAt=legacyKeyJson.find(",\"amp\":",keyAt);
        assert(keyAt!=std::string::npos&&ampAt!=std::string::npos);
        legacyKeyJson.erase(keyAt,ampAt-keyAt);
        Patch legacyDecoded;
        assert(deserializePatchJson(legacyKeyJson,legacyDecoded));
        assert(legacyDecoded.keyMapLow==0&&legacyDecoded.keyMapHigh==127);

        // On the SAME played pitch, a different K range changes loudness.
        auto lowKey=decoded;lowKey.keyMapLow=48;lowKey.keyMapHigh=72;
        SpectrachordVoice zero,full;
        zero.prepare(48000);full.prepare(48000);
        zero.start(8121,toDspPatch(lowKey),48,.8f,0,4800);
        full.start(8121,toDspPatch(decoded),48,.8f,0,4800);
        float zeroEnergy=0,fullEnergy=0;
        for(int i=0;i<2400;++i){
            const float a=zero.render(),b=full.render();
            zeroEnergy+=a*a;fullEnergy+=b*b;
        }
        assert(zeroEnergy<1e-10f&&fullEnergy>1e-5f);

        // Duplicate targets average: K=0 and a V=1 produce half the
        // unmodulated amplitude rather than replacing either route.
        auto mixed=lowKey;
        mixed.ops[0].modChannels[5].response.source=0;
        mixed.ops[0].modChannels[5].response.points[0].value=1;
        mixed.ops[0].modChannels[5].response.points[1].value=1;
        SpectrachordVoice half;
        half.prepare(48000);
        half.start(8121,toDspPatch(mixed),48,.8f,0,4800);
        float halfEnergy=0;
        for(int i=0;i<2400;++i){const float a=half.render();halfEnergy+=a*a;}
        assert(std::fabs(halfEnergy/fullEnergy-.25f)<.001f);

        // The filter's sixth channel also understands K and round-trips.
        auto filtering=decoded;
        filtering.ops[0].modChannelsConfigured=false;
        filtering.ops[0].expressionCurve={};
        filtering.filter.evolving=initialEvolvingFilter();
        auto& fm=filtering.filter.evolving.mods[5];
        fm=initialFilterModChannel();fm.enabled=true;fm.destination=1;
        fm.response.source=2;
        const auto filterJson=serializePatchJson(filtering);
        Patch filteredDecoded;
        assert(deserializePatchJson(filterJson,filteredDecoded));
        assert(filteredDecoded.filter.evolving.mods[5].enabled);
        assert(filteredDecoded.filter.evolving.mods[5].response.source==2);
        auto filteringLow=filteredDecoded;
        filteringLow.keyMapLow=48;filteringLow.keyMapHigh=72;
        SpectrachordVoice fl,fh;
        fl.prepare(48000);fh.prepare(48000);
        fl.start(8122,toDspPatch(filteringLow),48,.8f,0,4800);
        fh.start(8122,toDspPatch(filteredDecoded),48,.8f,0,4800);
        float filterDelta=0;
        for(int i=0;i<3000;++i){
            const float a=fl.render(),b=fh.render();
            assert(std::isfinite(a)&&std::isfinite(b));
            filterDelta+=std::fabs(a-b);
        }
        assert(filterDelta>1e-4f);
    }
    // Default volume and pitch envelopes must place decay at the second
    // anchor and the release/sustain gate at the second-to-last anchor.
    const auto defaults=legacyOperatorVolume(Envelope{});
    assert(defaults.count==4&&defaults.decayIndex==1&&
        defaults.releaseIndex==defaults.count-2);
    return 0;
}
