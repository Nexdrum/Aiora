#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "FxProcessor.h"
#include <array>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

using namespace aiora;

int main(){
    static constexpr std::array<const char*,4> instruments{{
        "Subula","Spectrello","Nebular","Nexdrum"}};
    const auto& bank=factoryBank();
    assert(bank.size()==instruments.size());
    for(size_t i=0;i<bank.size();++i){
        assert(bank[i].name==instruments[i]);
        assert(bank[i].filter.evolving.configured);
        const auto serialized=serializePatchJson(bank[i]);
        Patch decoded;
        std::string error;
        assert(deserializePatchJson(serialized,decoded,&error));
        assert(decoded.name==bank[i].name);
    }
    const auto& kit=nexdrumKit();
    static constexpr std::array<const char*,7> names{{
        "Crash","Ride","Roto Tom","Snare","Tom","Floor Tom","Kick"}};
    assert(kit.size()==names.size());
    for(size_t i=0;i<kit.size();++i){
        const auto& pad=kit[i];
        assert(pad.name==names[i]);
        assert(pad.lowMidi<=pad.centerMidi&&pad.centerMidi<=pad.highMidi);
        assert(pad.patch.filter.evolving.configured);
        assert(pad.patch.ops[0].volumeCurve.enabled);
        assert(pad.patch.masterAmpCurve.enabled);
    }
    assert(kit[0].centerMidi==80&&kit[1].centerMidi==73);
    assert(std::fabs(kit[1].patch.ops[3].harm[0]-.497686f)<1e-5f);

    Track factoryTrack;
    factoryTrack.name="Nexdrum";factoryTrack.drums=true;
    factoryTrack.pads.assign(kit.begin(),kit.end());
    const auto json=serializeDrumKitJson(factoryTrack);
    std::vector<DrumPad> imported;
    std::string error;
    assert(deserializeDrumKitJson(json,imported,&error));
    assert(imported.size()==7);
    for(size_t i=0;i<imported.size();++i){
        assert(imported[i].name==kit[i].name);
        assert(imported[i].centerMidi==kit[i].centerMidi);
        assert(imported[i].highMidi==kit[i].highMidi);
        assert(serializePatchJson(imported[i].patch)==
               serializePatchJson(kit[i].patch));
    }
    Project song;
    song.tracks.push_back(factoryTrack);
    Project decodedSong;
    assert(deserializeProjectJson(serializeProjectJson(song),decodedSong,&error));
    assert(decodedSong.tracks.size()==1);
    assert(decodedSong.tracks[0].pads.size()==7);

    // Test full modern synthesis at both ends of the continuous V/M controls.
    for(size_t i=0;i<kit.size();++i){
        for(float velocity:{.3f,.9f}){
            SpectrachordVoice voice;
            voice.prepare(24000);
            voice.start(123,toDspPatch(kit[i].patch),kit[i].centerMidi,
                        velocity,4800u,24000);
            float energy=0;
            for(int sample=0;sample<48000;++sample){
                const float output=voice.render();
                assert(std::isfinite(output));
                energy+=output*output;
            }
            assert(energy>0);
        }
    }

    // Every new operator is driven by a point envelope, not hidden ADSR.
    Patch scratch=makeDefaultPatch("New Synth");
    assert(scratch.ops[0].volumeCurve.enabled);
    assert(scratch.masterAmpCurve.enabled);
    assert(scratch.filter.evolving.configured);
    auto mod=initialOperatorModChannel();
    mod.enabled=true;mod.response.source=1;mod.destination=OperatorModDestination::Vol;
    mod.response.points[0]={0,.2f};
    mod.response.points[1]={1,1.0f};
    scratch.ops[0].modChannels[0]=mod;
    scratch.ops[0].modChannelsConfigured=true;
    Patch again;
    assert(deserializePatchJson(serializePatchJson(scratch),again,&error));
    assert(again.ops[0].modChannels[0].enabled);
    assert(again.ops[0].modChannels[0].response.source==1);
    assert(again.filter.evolving.configured);
    assert(again.masterAmpCurve.enabled);
    return 0;
}
