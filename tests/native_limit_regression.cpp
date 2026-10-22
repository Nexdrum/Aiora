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
        // Factory patches must be expressed entirely in the modern model.
        assert(serialized.find("\"fx\":")==std::string::npos);
        assert(serialized.find("\"vel\":")==std::string::npos);
        assert(serialized.find("\"mod\":")==std::string::npos);
        assert(serialized.find("\"fxChain\":")!=std::string::npos);
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

    // FX rack settings round-trip without retired patch-level knobs.
    Patch fxPatch=makeDefaultPatch("Modern FX");
    fxPatch.fx.count=1;
    fxPatch.fx.chain[0]=makeFxModule(FxType::Vastrel);
    fxPatch.fx.chain[0].params[0]=.43f;
    fxPatch.fx.chain[0].params[1]=2.4f;
    const std::string fxJson=serializePatchJson(fxPatch);
    assert(fxJson.find("\"fx\":")==std::string::npos);
    Patch fxDecoded;
    assert(deserializePatchJson(fxJson,fxDecoded,&error));
    assert(fxDecoded.fx.count==1);
    assert(fxDecoded.fx.chain[0].type==FxType::Vastrel);
    assert(std::fabs(fxDecoded.fx.chain[0].params[0]-.43f)<1e-5f);
    assert(std::fabs(fxDecoded.fx.chain[0].params[1]-2.4f)<1e-5f);

    // A master rack is saved directly, rather than synthesized from
    // obsolete master reverb controls.
    song.masterFx.count=1;
    song.masterFx.chain[0]=makeFxModule(FxType::Recurra);
    song.masterFx.chain[0].params[0]=.31f;
    const std::string modernProject=serializeProjectJson(song);
    assert(modernProject.find("\"mrev\"")==std::string::npos);
    assert(modernProject.find("\"mrevDecay\"")==std::string::npos);
    assert(modernProject.find("\"mrevRoom\"")==std::string::npos);
    Project decodedModern;
    assert(deserializeProjectJson(modernProject,decodedModern,&error));
    assert(decodedModern.masterFx.count==1);
    assert(decodedModern.masterFx.chain[0].type==FxType::Recurra);
    assert(std::fabs(decodedModern.masterFx.chain[0].params[0]-.31f)<1e-5f);

    // Exactly six supported effects; abandoned numeric slots never decode.
    assert(kAvailableFxTypes.size()==6);
    assert(std::string(fxTypeName(FxType::Embera))=="Embera");
    assert(std::string(fxTypeName(FxType::Recurra))=="Recurra");
    assert(std::string(fxTypeName(FxType::Vastrel))=="Vastrel");
    assert(std::string(fxTypeName(FxType::Prisma))=="Prisma");
    assert(std::string(fxTypeName(FxType::Ampere))=="Ampere");
    assert(std::string(fxTypeName(FxType::Cabeus))=="Cabeus");
    for(int retired:{1,2,6}){
        assert(!isAvailableFxType(static_cast<FxType>(retired)));
        std::string invalid=fxJson;
        const auto location=invalid.find("\"type\":5");
        assert(location!=std::string::npos);
        invalid.replace(location,std::string("\"type\":5").size(),
            "\"type\":"+std::to_string(retired));
        Patch rejected;
        assert(deserializePatchJson(invalid,rejected,&error));
        assert(rejected.fx.count==0);
    }

    // Embera's bitcrush is a fourth parameter, bypassed exactly at zero.
    assert(fxParamCount(FxType::Embera)==4);
    Fx embera;
    embera.count=1;
    embera.chain[0]=makeFxModule(FxType::Embera);
    assert(embera.chain[0].params[3]==0.0f);
    embera.chain[0].params[1]=0.0f;
    FxProcessor crushDsp;
    crushDsp.prepare(48000);
    crushDsp.set(embera);
    const float clean=crushDsp.process(.237f)[0];
    assert(std::fabs(clean-std::tanh(.237f))<1e-6f);
    embera.chain[0].params[3]=1.0f;
    crushDsp.set(embera);
    const float crushed=crushDsp.process(.237f)[0];
    assert(std::fabs(crushed-clean)>.03f);

    // Prisma stores, edits and actually processes 16 bands and Output.
    assert(kPrismaBands==16);
    assert(fxParamCount(FxType::Prisma)==17);
    assert(std::string(fxParamDesc(FxType::Prisma,15).name)=="14k dB");
    assert(std::string(fxParamDesc(FxType::Prisma,16).name)=="Output");
    Patch prismaPatch=makeDefaultPatch("Prisma test");
    prismaPatch.fx.count=2;
    prismaPatch.fx.chain[0]=embera.chain[0];
    prismaPatch.fx.chain[1]=makeFxModule(FxType::Prisma);
    prismaPatch.fx.chain[1].params[9]=-5.5f;
    prismaPatch.fx.chain[1].params[15]=8.25f;
    prismaPatch.fx.chain[1].params[16]=.81f;
    Patch prismaDecoded;
    assert(deserializePatchJson(serializePatchJson(prismaPatch),prismaDecoded,&error));
    assert(prismaDecoded.fx.count==2);
    assert(prismaDecoded.fx.chain[0].type==FxType::Embera);
    assert(prismaDecoded.fx.chain[0].params[3]==1.0f);
    assert(prismaDecoded.fx.chain[1].type==FxType::Prisma);
    assert(std::fabs(prismaDecoded.fx.chain[1].params[9]+5.5f)<1e-5f);
    assert(std::fabs(prismaDecoded.fx.chain[1].params[15]-8.25f)<1e-5f);
    assert(std::fabs(prismaDecoded.fx.chain[1].params[16]-.81f)<1e-5f);
    Fx prism;
    prism.count=1;
    prism.chain[0]=makeFxModule(FxType::Prisma);
    FxProcessor prismDsp;
    prismDsp.prepare(48000);
    prismDsp.set(prism);
    float neutralEnergy=0.0f;
    for(int i=0;i<4800;++i){
        const float signal=.12f*std::sin(2.0f*3.14159265f*14000.0f*
            static_cast<float>(i)/48000.0f);
        const float y=prismDsp.process(signal)[0];
        if(i>1000)neutralEnergy+=y*y;
    }
    prism.chain[0].params[15]=12.0f;
    prismDsp.set(prism);
    prismDsp.reset();
    float boostedEnergy=0.0f;
    for(int i=0;i<4800;++i){
        const float signal=.12f*std::sin(2.0f*3.14159265f*14000.0f*
            static_cast<float>(i)/48000.0f);
        const float y=prismDsp.process(signal)[0];
        assert(std::isfinite(y));
        if(i>1000)boostedEnergy+=y*y;
    }
    assert(boostedEnergy>neutralEnergy*1.3f);

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
