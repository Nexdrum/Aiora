#include "FactoryPresets.h"
#include "ApprovedFactoryPatches.h"
#include "SpectrelloFactoryV2.h"
#include "ProjectCodec.h"
#include <array>
#include <stdexcept>
#include <string_view>

namespace aiora {
namespace {
void setOp(Operator& o,Wave wave,float ratio,float detune,float level,
           float a,float d,float s,float r) {
    o.enabled=true;o.wave=wave;o.ratio=ratio;o.detuneCents=detune;
    o.level=level;o.env={a,d,s,r};
}
void disable(Operator& o){o.enabled=false;o.level=0.0f;}
void slots(Patch& p,std::initializer_list<ModSlot> list){
    p.modSlots.assign(list.begin(),list.end());p.modSlotCount=p.modSlots.size();
}
} // namespace

Patch makeDefaultPatch(const char* name) {
    Patch p;
    p.name = name ? name : "Spectrachord Init";
    const std::array<float,6> ratios{1.0f,2.0f,3.0f,1.01f,0.5f,4.0f};
    const std::array<float,6> levels{0.9f,0.0f,0.0f,0.0f,0.0f,0.0f};
    for (size_t i=0;i<6;++i) {
        p.ops[i].enabled = true;
        p.ops[i].wave = Wave::Sine;
        p.ops[i].ratio = ratios[i];
        p.ops[i].detuneCents = 0.0f;
        p.ops[i].level = levels[i];
        p.ops[i].env = {0.005f,0.1f,0.8f,0.15f};
    }
    p.filter = {FilterType::Lowpass,6500.0f,0.7f,0.0f,{0.01f,0.15f,0.7f,0.2f}};
    p.amp = {0.008f,0.12f,0.85f,0.25f};
    p.lfo = {5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.velocityAmp = 0.0f;
    p.velocityFilter = 0.0f;
    slots(p, {{ModTarget::Cutoff,6500.0f,1800.0f},
              {ModTarget::AmpRelease,0.25f,0.06f},
              {ModTarget::Volume,1.0f,0.7f}});
    p.fx = {0.0f,0.18f,0.32f,0.32f,0.25f};
    p.unison=0.0f;p.glide=0.0f;p.octave=0.0f;p.volume=0.8f;
    return p;
}


Patch makeFactoryPatch(FactoryPreset preset) {
    const char* name="Spectrachord Init";
    switch(preset){
        case FactoryPreset::Subula:name="Subula";break;
        case FactoryPreset::Spectrello:name="Spectrello";break;
        case FactoryPreset::Nebular:name="Nebular";break;
        case FactoryPreset::Nexdrum:name="Nexdrum";break;
        default:break;
    }
    if(preset==FactoryPreset::Subula||
       preset==FactoryPreset::Spectrello||
       preset==FactoryPreset::Nebular){
        const std::string_view source=preset==FactoryPreset::Nebular?
            factory_patch_data::nebularSteelPhysicsV2:
            (preset==FactoryPreset::Subula?
                factory_patch_data::subulaMetalFretlessV4:
                factory_patch_data::spectrelloCarbonFiberV2);
        Patch approved;
        std::string error;
        if(!deserializePatchJson(source,approved,&error))
            throw std::runtime_error("Invalid approved factory patch: "+error);
        approved.name=name;
        return approved;
    }
    Patch p=makeDefaultPatch(name);
    if(preset==FactoryPreset::SpectrachordInit){
        // A genuinely blank starting point: six identical pure-sine
        // operators, with only OP1 active.
        const Operator neutral=[]{
            Operator o;
            o.enabled=false;
            o.wave=Wave::Sine;
            o.ratio=1.0f;
            o.semitoneOffset=0.0f;
            o.detuneCents=0.0f;
            o.level=0.9f;
            o.env={0.005f,0.1f,0.8f,0.15f};
            o.harm.fill(0.0f);
            o.harmMute.fill(0.0f);
            o.hasHarm=false;
            o.hasHarmMute=false;
            return o;
        }();
        p.ops.fill(neutral);
        p.ops[0].enabled=true;
        for(auto& row:p.matrix)row.fill(0.0f);

        p.filter={
            FilterType::Lowpass,
            18000.0f,0.7f,0.0f,
            {0.01f,0.15f,0.7f,0.2f}};
        p.amp={0.008f,0.12f,0.85f,0.25f};
        p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
        p.velocityAmp=0.0f;
        p.velocityFilter=0.0f;
        p.modSlots.clear();
        p.modSlotCount=0;
        p.fx={
            0.0f, // distortion
            0.0f, // delay level
            0.03f, // minimum delay time
            0.0f, // feedback
            0.0f  // reverb level
        };
        p.unison=0.0f;
        p.glide=0.0f;
        p.octave=0.0f;
        p.volume=0.8f;
        return p;
    }


    if (preset == FactoryPreset::Nexdrum) {
        setOp(p.ops[0],Wave::Sine,1.0f,0.0f,0.7f,0.002f,0.25f,0.0f,0.15f);
        setOp(p.ops[1],Wave::Sine,0.5f,0.0f,0.5f,0.002f,0.22f,0.0f,0.12f);
        disable(p.ops[2]);disable(p.ops[3]);disable(p.ops[4]);disable(p.ops[5]);
        p.filter={FilterType::Lowpass,900.0f,1.0f,0.2f,{0.005f,0.15f,0.5f,0.15f}};
        p.amp={0.002f,0.25f,0.0f,0.18f};
        p.velocityAmp=0.6f;p.velocityFilter=0.4f;
        slots(p, {{ModTarget::Cutoff,900.0f,400.0f},{ModTarget::AmpRelease,0.18f,0.06f},{ModTarget::Volume,1.0f,0.7f}});
        p.unison=0.0f;p.volume=0.9f;
        p.fx.distortion=0.0f;p.fx.delay=0.0f;p.fx.delayFeedback=0.3f;p.fx.reverb=0.18f;
        p.nexdrumLow=40;p.nexdrumHigh=60;
    }
    return p;
}

const std::array<Patch,static_cast<size_t>(FactoryPreset::Count)>& factoryBank(){
    static const std::array<Patch,static_cast<size_t>(FactoryPreset::Count)> bank{
        makeFactoryPatch(FactoryPreset::Subula),
        makeFactoryPatch(FactoryPreset::Spectrello),
        makeFactoryPatch(FactoryPreset::Nebular),
        makeFactoryPatch(FactoryPreset::Nexdrum)
    };
    return bank;
}
} // namespace aiora
