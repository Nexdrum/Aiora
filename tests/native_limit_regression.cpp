#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "FactoryPresets.h"
#include "NexdrumKit.h"
#include "SpectrachordVoice.h"
#include "FxProcessor.h"
#include "RollTouchRouting.h"
#include "RollCurveSimplify.h"
#include <array>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

using namespace aiora;

// Playback and piano-roll editors use the same interpolation as this check.
static float inspectDrawCurve(const std::vector<CurvePoint>& points,float x){
    assert(!points.empty());
    if(x<=points.front().step)return points.front().value;
    for(size_t i=1;i<points.size();++i){
        if(x>points[i].step)continue;
        const auto& a=points[i-1];
        const auto& b=points[i];
        return curveSegment((x-a.step)/(b.step-a.step),
            a.value,b.value,b.midpoint,b.handleActive,b.handleValue);
    }
    return points.back().value;
}

static void checkDrawFit(
    const std::vector<RollDrawSample>& samples,
    const std::vector<CurvePoint>& saved,float tolerance){
    assert(saved.size()>=2);
    for(size_t i=0;i<samples.size();++i){
        assert(std::fabs(inspectDrawCurve(saved,samples[i].step)-
            samples[i].value)<=tolerance+0.0005f);
        if(i==0)continue;
        for(const float t:{0.25f,0.5f,0.75f}){
            const float x=samples[i-1].step+
                (samples[i].step-samples[i-1].step)*t;
            const float wanted=samples[i-1].value+
                (samples[i].value-samples[i-1].value)*t;
            assert(std::fabs(inspectDrawCurve(saved,x)-wanted)<=
                tolerance+0.0005f);
        }
    }
}

static void checkDrawSimplification(){
    constexpr float pi=3.14159265358979323846f;
    // Dense straight drawing becomes its two true endpoints.
    std::vector<RollDrawSample> straight;
    for(int i=0;i<=400;++i){
        const float t=static_cast<float>(i)/400.0f;
        straight.push_back({-0.5f+8*t,0.2f+0.65f*t});
    }
    const auto line=simplifyRollDrawCurve(straight,7.5f,.009f,.2f);
    assert(line.size()==2);
    checkDrawFit(straight,line,.009f);

    // The X-handle must merge smoothly curved strokes, not only lines.
    std::vector<RollDrawSample> curved;
    for(int i=0;i<=480;++i){
        const float t=static_cast<float>(i)/480.0f;
        curved.push_back({-0.5f+12*t,0.12f+0.74f*t*t});
    }
    const auto smooth=simplifyRollDrawCurve(curved,11.5f,.009f,.12f);
    assert(smooth.size()<45);
    assert(std::any_of(smooth.begin(),smooth.end(),
        [](const CurvePoint& p){return p.handleActive;}));
    checkDrawFit(curved,smooth,.009f);

    // Delicate bends above 2.5 cents and faster reversals must survive.
    std::vector<RollDrawSample> vibrato;
    for(int i=0;i<=540;++i){
        const float t=static_cast<float>(i)/540.0f;
        vibrato.push_back({-0.5f+10*t,
            0.06f*std::sin(2*pi*5*t)});
    }
    const auto bends=simplifyRollDrawCurve(vibrato,9.5f,.025f,0.0f);
    assert(bends.size()>10);
    assert(bends.size()<vibrato.size()/3);
    checkDrawFit(vibrato,bends,.025f);

    // Short high-intensity articulations cannot be averaged away.
    std::vector<RollDrawSample> accents;
    for(int i=0;i<=240;++i){
        const float t=static_cast<float>(i)/240.0f;
        const float peak=std::max(0.0f,1.0f-std::fabs(t-0.51f)/0.015f);
        accents.push_back({-0.5f+6*t,0.22f+0.55f*peak});
    }
    const auto sharp=simplifyRollDrawCurve(accents,5.5f,.009f,.22f);
    assert(sharp.size()>=4);
    checkDrawFit(accents,sharp,.009f);

    // No point count cap: a long deliberate modulation remains detailed.
    std::vector<RollDrawSample> longMod;
    for(int i=0;i<=2400;++i){
        const float t=static_cast<float>(i)/2400.0f;
        longMod.push_back({-0.5f+40*t,0.5f+0.28f*
            std::sin(2*pi*45*t)});
    }
    const auto retained=simplifyRollDrawCurve(longMod,39.5f,.009f,.5f);
    assert(retained.size()>80);
    assert(retained.size()<longMod.size());
    checkDrawFit(longMod,retained,.009f);

    // A silent default channel is correctly omitted.
    const std::vector<RollDrawSample> untouched{
        {-0.5f,1.0f},{3.5f,1.0f}};
    assert(simplifyRollDrawCurve(untouched,3.5f,.009f,1.0f).empty());
}

int main(){
    checkDrawSimplification();
    // A resumed note must accept Android POINTER_DOWN while V/M stay held.
    // PRIMARY DOWN still works if the draw finger is alone.
    constexpr auto resume=PausedRollTouch::Resume;
    constexpr auto ignore=PausedRollTouch::Ignore;
    constexpr auto finish=PausedRollTouch::CompleteAndRoute;
    assert(routePausedRollTouch(true,true,true,true,false,true)==resume);
    assert(routePausedRollTouch(true,true,true,false,true,true)==resume);
    // An extra finger away from the circle cannot cancel the recording.
    assert(routePausedRollTouch(true,true,true,false,true,false)==ignore);
    assert(routePausedRollTouch(true,true,true,true,false,false)==finish);
    // Existing tests: wrong page and inactive/unpaused sessions never resume.
    assert(routePausedRollTouch(false,true,true,false,true,true)==ignore);
    assert(routePausedRollTouch(false,true,true,true,false,true)==finish);
    assert(routePausedRollTouch(true,false,true,false,true,true)==ignore);
    assert(routePausedRollTouch(true,true,false,false,true,true)==ignore);
    assert(routePausedRollTouch(true,true,true,false,false,true)==ignore);
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

    // Pulsar and Binary are named additions; retired numeric slots remain unavailable.
    assert(kAvailableFxTypes.size()==8);
    assert(std::string(fxTypeName(FxType::Pulsar))=="Pulsar");
    assert(std::string(fxTypeName(FxType::Binary))=="Binary");
    assert(fxParamCount(FxType::Pulsar)==5);
    assert(fxParamCount(FxType::Binary)==4);
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

    // New module IDs and three master-Amp response slots round trip.
    Patch modulationPatch=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    modulationPatch.fx.count=2;
    modulationPatch.fx.chain[0]=makeFxModule(FxType::Pulsar);
    modulationPatch.fx.chain[1]=makeFxModule(FxType::Binary);
    modulationPatch.ampMods[0]=initialOperatorResponse();
    modulationPatch.ampMods[0].enabled=true;
    modulationPatch.ampMods[0].source=1;
    modulationPatch.ampMods[1]=initialOperatorResponse();
    modulationPatch.ampMods[1].enabled=true;
    modulationPatch.ampMods[1].source=2;
    Patch modulationRoundTrip;
    assert(deserializePatchJson(
        serializePatchJson(modulationPatch),modulationRoundTrip,&error));
    assert(modulationRoundTrip.fx.count==2);
    assert(modulationRoundTrip.fx.chain[0].type==FxType::Pulsar);
    assert(modulationRoundTrip.fx.chain[1].type==FxType::Binary);
    assert(modulationRoundTrip.ampMods[0].enabled);
    assert(modulationRoundTrip.ampMods[0].source==1);
    assert(modulationRoundTrip.ampMods[1].source==2);
    assert(!modulationRoundTrip.ampMods[2].enabled);
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

    // Noise/Hiss legacy patch migration must preserve independently pitched
    // primary and morph endpoints without exposing Hiss as a new selection.
    const std::string legacy=R"({"name":"Legacy Hiss","ops":[
        {"wave":"hiss","waveMorph":"noise","level":0.8},
        {},{},{},{},{}]})";
    Patch migrated;
    assert(deserializePatchJson(legacy,migrated,&error));
    assert(migrated.ops[0].wave==Wave::Noise);
    assert(migrated.ops[0].texturePitched);
    assert(migrated.ops[0].waveMorph==Wave::Noise);
    assert(!migrated.ops[0].textureMorphPitched);
    Patch reloaded;
    assert(deserializePatchJson(serializePatchJson(migrated),reloaded,&error));
    assert(reloaded.ops[0].texturePitched);
    assert(!reloaded.ops[0].textureMorphPitched);

    // Custom procedural texture parameters and both morph modes round-trip.
    Patch textured=makeDefaultPatch("Texture Test");
    textured.ops[0].wave=Wave::TextureCustom;
    textured.ops[0].texture.source=3;
    textured.ops[0].texture.density=0.82f;
    textured.ops[0].texturePitched=true;
    textured.ops[0].waveMorph=Wave::Clang;
    textured.ops[0].waveMorphConfigured=true;
    textured.ops[0].textureMorphPitched=false;
    Patch texturedCopy;
    assert(deserializePatchJson(serializePatchJson(textured),texturedCopy,&error));
    assert(texturedCopy.ops[0].wave==Wave::TextureCustom);
    assert(texturedCopy.ops[0].texture.source==3);
    assert(std::fabs(texturedCopy.ops[0].texture.density-.82f)<.001f);
    assert(texturedCopy.ops[0].texturePitched);
    assert(texturedCopy.ops[0].waveMorph==Wave::Clang);
    assert(!texturedCopy.ops[0].textureMorphPitched);
    // Exercise every new generator in both pitch modes and an active morph.
    for(Wave texture:{Wave::Noise,Wave::Grain,Wave::Rasp,
                      Wave::Clang,Wave::TextureCustom}){
        for(bool pitched:{false,true}){
            Patch current=makeDefaultPatch("Audio Texture");
            current.ops[0].wave=texture;
            current.ops[0].texturePitched=pitched;
            current.ops[0].waveMorph=Wave::Noise;
            current.ops[0].waveMorphConfigured=true;
            current.ops[0].textureMorphPitched=!pitched;
            current.ops[0].texture.source=2;
            current.ops[0].level=.85f;
            SpectrachordVoice v;
            v.prepare(24000);
            v.start(321,toDspPatch(current),62,.9f,4800u,24000);
            float energy=0.0f;
            for(int n=0;n<4096;++n){
                const float sample=v.render();
                assert(std::isfinite(sample));
                energy+=sample*sample;
            }
            assert(energy>0.0f);
        }
    }
    return 0;
}
