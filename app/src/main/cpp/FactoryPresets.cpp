#include "FactoryPresets.h"
#include <algorithm>

namespace aiora {
namespace {

void setOp(Operator& o, Wave wave, float ratio, float detune, float level,
           float a, float d, float s, float r) {
    o.enabled = true;
    o.wave = wave;
    o.ratio = ratio;
    o.detuneCents = detune;
    o.level = level;
    o.env = {a,d,s,r};
}

void disable(Operator& o) { o.enabled = false; o.level = 0.0f; }
void setHarm(Operator& o, const std::array<float,16>& h) { o.harm = h; o.hasHarm = true; }
void setHarmMute(Operator& o, const std::array<float,16>& h) { o.harmMute = h; o.hasHarmMute = true; }

void slots(Patch& p, std::initializer_list<ModSlot> list) {
    p.modSlots.fill({});
    const size_t count=std::min(p.modSlots.size(),list.size());
    p.modSlotCount=static_cast<uint8_t>(count);
    size_t i=0;
    for(const auto& s:list){
        if(i>=count)break;
        p.modSlots[i++]=s;
    }
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
    const char* name = "Spectrachord Init";
    switch (preset) {
        case FactoryPreset::Subula: name="Subula"; break;
        case FactoryPreset::Spectrello: name="Spectrello"; break;
        case FactoryPreset::Nebular: name="Nebular"; break;
        case FactoryPreset::Nexdrum: name="Nexdrum"; break;
        default: break;
    }
    Patch p = makeDefaultPatch(name);

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
        p.modSlots.fill({});
        p.modSlotCount=0;
        p.fx={
            0.0f, // distortion
            0.0f, // delay level
            0.32f,
            0.32f,
            0.0f  // reverb level
        };
        p.unison=0.0f;
        p.glide=0.0f;
        p.octave=0.0f;
        p.volume=0.8f;
        return p;
    }

    if (preset == FactoryPreset::Spectrello) {
        setOp(p.ops[0],Wave::Custom,1.0f,0.0f,0.85f,0.09f,0.3f,0.9f,0.45f);
        setHarm(p.ops[0], {1,0.55f,0.38f,0.28f,0.2f,0.15f,0.11f,0.085f,0.06f,0.05f,0.04f,0.03f,0.025f,0.02f,0.015f,0.01f});
        setHarmMute(p.ops[0], {1,0.28f,0.11f,0.045f,0.018f,0.008f,0.004f,0.002f,0.001f,0,0,0,0,0,0,0});
        setOp(p.ops[1],Wave::Saw,1.004f,8.0f,0.5f,0.09f,0.3f,0.9f,0.45f);
        setOp(p.ops[2],Wave::Sine,2.003f,0.0f,0.22f,0.06f,0.3f,0.8f,0.4f);
        setOp(p.ops[3],Wave::Noise,1.0f,0.0f,0.12f,0.09f,0.3f,0.9f,0.45f);
        disable(p.ops[4]);disable(p.ops[5]);
        p.matrix[2][0]=0.18f;
        p.filter={FilterType::Lowpass,2600.0f,1.4f,0.18f,{0.12f,0.35f,0.7f,0.4f}};
        p.amp={0.08f,0.3f,0.92f,0.5f};
        p.velocityAmp=0.7f;p.velocityFilter=0.6f;
        p.lfo={5.6f,0.3f,0.4f,0.5f,LfoTarget::Pitch};
        slots(p, {{ModTarget::Op4,0.0f,0.12f},{ModTarget::Morph1,0.0f,1.0f},
                  {ModTarget::Cutoff,2600.0f,1100.0f},{ModTarget::AmpRelease,0.5f,0.14f}});
        p.unison=0.55f;p.volume=0.75f;
        p.fx.distortion=0.0f;p.fx.delay=0.18f;p.fx.delayFeedback=0.32f;p.fx.reverb=0.3f;
    } else if (preset == FactoryPreset::Nebular) {
        setOp(p.ops[0],Wave::Custom,1.0f,0.0f,0.9f,0.003f,0.35f,0.15f,0.5f);
        setHarm(p.ops[0], {1,0.6f,0.35f,0.15f,0.07f,0.03f,0.015f,0.008f,0.004f,0.002f,0,0,0,0,0,0});
        setHarmMute(p.ops[0], {1,0.22f,0.07f,0.018f,0.005f,0.001f,0,0,0,0,0,0,0,0,0,0});
        setOp(p.ops[1],Wave::Sine,2.0f,0.0f,0.3f,0.003f,0.25f,0.1f,0.4f);
        setOp(p.ops[2],Wave::Noise,1.0f,0.0f,0.5f,0.003f,0.05f,0.0f,0.1f);
        disable(p.ops[3]);disable(p.ops[4]);disable(p.ops[5]);
        p.matrix[1][0]=0.4f;
        p.filter={FilterType::Lowpass,1600.0f,1.0f,0.3f,{0.005f,0.2f,0.5f,0.25f}};
        p.amp={0.003f,0.35f,0.2f,0.4f};
        p.velocityAmp=0.8f;p.velocityFilter=0.7f;
        p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
        slots(p, {{ModTarget::Morph1,0.0f,1.0f},{ModTarget::Cutoff,1600.0f,620.0f},
                  {ModTarget::AmpRelease,0.4f,0.05f},{ModTarget::Volume,1.0f,0.65f}});
        p.unison=0.0f;p.volume=0.8f;
        p.fx.distortion=0.0f;p.fx.delay=0.15f;p.fx.delayFeedback=0.3f;p.fx.reverb=0.2f;
    } else if (preset == FactoryPreset::Subula) {
        setOp(p.ops[0],Wave::Saw,1.0f,0.0f,0.7f,0.004f,0.15f,0.8f,0.2f);
        setOp(p.ops[1],Wave::Saw,1.007f,6.0f,0.4f,0.004f,0.15f,0.8f,0.2f);
        setOp(p.ops[2],Wave::Sine,0.5f,0.0f,0.8f,0.004f,0.12f,1.0f,0.18f);
        disable(p.ops[3]);disable(p.ops[4]);disable(p.ops[5]);
        p.filter={FilterType::Lowpass,750.0f,3.0f,0.3f,{0.005f,0.15f,0.5f,0.18f}};
        p.amp={0.004f,0.12f,0.85f,0.2f};
        p.velocityAmp=0.6f;p.velocityFilter=0.5f;
        slots(p, {{ModTarget::Cutoff,750.0f,260.0f},{ModTarget::AmpRelease,0.2f,0.06f},{ModTarget::Volume,1.0f,0.7f}});
        p.unison=0.3f;p.volume=0.8f;
    } else if (preset == FactoryPreset::Nexdrum) {
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

const std::array<Patch, static_cast<size_t>(FactoryPreset::Count)>& factoryBank() {
    static const std::array<Patch, static_cast<size_t>(FactoryPreset::Count)> bank{
        makeFactoryPatch(FactoryPreset::SpectrachordInit),
        makeFactoryPatch(FactoryPreset::Subula),
        makeFactoryPatch(FactoryPreset::Spectrello),
        makeFactoryPatch(FactoryPreset::Nebular),
        makeFactoryPatch(FactoryPreset::Nexdrum)
    };
    return bank;
}

} // namespace aiora
