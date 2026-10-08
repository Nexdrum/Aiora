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


struct BowedStringTone {
    std::array<float,16> stringOpen, stringPressed, body, mid, bridge, bridgePressed;
    float attack, release, cutoff, softCutoff, hardCutoff, resonance, vibratoRate, vibratoDepth;
    float bodyLevel, midLevel, bridgeLevel, noiseLevel, scratchLevel, noiseMax, scratchMax, fmDepth, reverb;
};

// All resonating layers are synthesized from Spectrachord's existing custom
// harmonic oscillators and per-operator ADSRs. Bow friction occupies OP5/OP6.
// The M curve changes bow PRESSURE/TEXTURE, while V remains independent volume.
void configureBowedString(Patch& p, FactoryPreset preset) {
    BowedStringTone tone{};
    if (preset == FactoryPreset::Violin) {
        tone = {
            {1.0f,0.78f,0.61f,0.49f,0.4f,0.32f,0.27f,0.23f,0.2f,0.16f,0.13f,0.11f,0.09f,0.07f,0.055f,0.04f}, // Open string: natural harmonic envelope
            {1.0f,0.89f,0.82f,0.75f,0.66f,0.59f,0.53f,0.48f,0.43f,0.38f,0.34f,0.29f,0.26f,0.22f,0.18f,0.15f}, // Increased pressure: stronger high harmonics
            {1.0f,0.77f,0.36f,0.21f,0.11f,0.065f,0.04f,0.024f,0.013f,0.008f,0.005f,0.003f,0.002f,0.001f,0.0f,0.0f}, // Lower body coloration
            {0.9f,0.6f,0.38f,0.23f,0.13f,0.08f,0.045f,0.026f,0.015f,0.008f,0.005f,0.003f,0.001f,0.0f,0.0f,0.0f}, // Midrange resonant coloration
            {0.8f,0.61f,0.46f,0.34f,0.23f,0.17f,0.12f,0.09f,0.064f,0.045f,0.03f,0.021f,0.015f,0.01f,0.008f,0.005f}, // Upper string/bridge response
            {0.84f,0.78f,0.67f,0.56f,0.47f,0.38f,0.32f,0.27f,0.22f,0.18f,0.14f,0.11f,0.09f,0.07f,0.055f,0.04f}, // Bow bite at high pressure
            0.032f,0.22f,6100.0f,3300.0f,10500.0f,1.05f,5.65f,0.075f,0.29f,0.16f,0.12f,0.025f,0.012f,0.15f,0.09f,0.018f,0.1f
        };
    } else     if (preset == FactoryPreset::Viola) {
        tone = {
            {1.0f,0.64f,0.48f,0.43f,0.37f,0.25f,0.17f,0.115f,0.08f,0.06f,0.043f,0.033f,0.023f,0.016f,0.01f,0.006f}, // Open string: natural harmonic envelope
            {1.0f,0.77f,0.66f,0.62f,0.58f,0.44f,0.36f,0.29f,0.24f,0.19f,0.15f,0.12f,0.09f,0.07f,0.055f,0.04f}, // Increased pressure: stronger high harmonics
            {1.0f,0.82f,0.61f,0.37f,0.18f,0.095f,0.053f,0.029f,0.017f,0.01f,0.005f,0.003f,0.002f,0.001f,0.0f,0.0f}, // Lower body coloration
            {0.73f,0.84f,0.69f,0.49f,0.34f,0.22f,0.14f,0.09f,0.05f,0.031f,0.018f,0.01f,0.006f,0.003f,0.001f,0.0f}, // Midrange resonant coloration
            {0.78f,0.57f,0.34f,0.19f,0.11f,0.066f,0.039f,0.026f,0.017f,0.01f,0.006f,0.004f,0.002f,0.001f,0.0f,0.0f}, // Upper string/bridge response
            {0.82f,0.75f,0.58f,0.48f,0.38f,0.32f,0.26f,0.2f,0.16f,0.12f,0.09f,0.07f,0.05f,0.04f,0.03f,0.02f}, // Bow bite at high pressure
            0.052f,0.34f,3900.0f,2000.0f,7300.0f,1.25f,5.1f,0.068f,0.38f,0.22f,0.085f,0.02f,0.01f,0.13f,0.078f,0.015f,0.105f
        };
    } else     if (preset == FactoryPreset::Cello) {
        tone = {
            {1.0f,0.79f,0.53f,0.38f,0.29f,0.24f,0.19f,0.14f,0.1f,0.08f,0.06f,0.045f,0.034f,0.024f,0.017f,0.012f}, // Open string: natural harmonic envelope
            {1.0f,0.88f,0.72f,0.63f,0.56f,0.47f,0.4f,0.34f,0.28f,0.23f,0.2f,0.17f,0.14f,0.11f,0.085f,0.065f}, // Increased pressure: stronger high harmonics
            {1.0f,0.89f,0.64f,0.37f,0.19f,0.105f,0.06f,0.035f,0.02f,0.012f,0.007f,0.004f,0.002f,0.001f,0.0f,0.0f}, // Lower body coloration
            {1.0f,0.68f,0.39f,0.22f,0.14f,0.078f,0.042f,0.024f,0.013f,0.008f,0.004f,0.002f,0.001f,0.0f,0.0f,0.0f}, // Midrange resonant coloration
            {0.73f,0.48f,0.32f,0.19f,0.12f,0.08f,0.051f,0.033f,0.021f,0.014f,0.009f,0.006f,0.004f,0.002f,0.001f,0.0f}, // Upper string/bridge response
            {0.76f,0.66f,0.53f,0.4f,0.31f,0.25f,0.2f,0.16f,0.12f,0.09f,0.07f,0.055f,0.042f,0.032f,0.023f,0.016f}, // Bow bite at high pressure
            0.074f,0.44f,4200.0f,1700.0f,7400.0f,1.0f,4.85f,0.062f,0.46f,0.19f,0.075f,0.02f,0.01f,0.115f,0.065f,0.013f,0.12f
        };
    } else     if (preset == FactoryPreset::Contrabass) {
        tone = {
            {1.0f,0.65f,0.43f,0.32f,0.23f,0.18f,0.14f,0.115f,0.088f,0.07f,0.052f,0.039f,0.028f,0.02f,0.014f,0.01f}, // Open string: natural harmonic envelope
            {1.0f,0.82f,0.68f,0.57f,0.47f,0.4f,0.34f,0.29f,0.25f,0.21f,0.18f,0.145f,0.12f,0.1f,0.08f,0.065f}, // Increased pressure: stronger high harmonics
            {1.0f,0.88f,0.72f,0.46f,0.28f,0.15f,0.085f,0.045f,0.025f,0.014f,0.008f,0.004f,0.002f,0.001f,0.0f,0.0f}, // Lower body coloration
            {1.0f,0.85f,0.49f,0.29f,0.165f,0.085f,0.044f,0.025f,0.014f,0.008f,0.005f,0.003f,0.001f,0.0f,0.0f,0.0f}, // Midrange resonant coloration
            {0.68f,0.48f,0.3f,0.19f,0.11f,0.07f,0.043f,0.029f,0.018f,0.012f,0.008f,0.005f,0.003f,0.002f,0.001f,0.0f}, // Upper string/bridge response
            {0.78f,0.66f,0.5f,0.39f,0.3f,0.23f,0.18f,0.14f,0.11f,0.085f,0.065f,0.052f,0.04f,0.03f,0.02f,0.013f}, // Bow bite at high pressure
            0.116f,0.58f,2100.0f,950.0f,4600.0f,1.0f,4.35f,0.04f,0.51f,0.2f,0.06f,0.017f,0.008f,0.112f,0.063f,0.01f,0.095f
        };
    }
    const float a=tone.attack, r=tone.release;
    setOp(p.ops[0],Wave::Custom,1.0f,0.0f,0.88f,a,0.22f,0.95f,r);
    setHarm(p.ops[0],tone.stringOpen);
    setHarmMute(p.ops[0],tone.stringPressed);
    setOp(p.ops[1],Wave::Custom,1.0f,-1.8f,tone.bodyLevel,a+0.013f,0.32f,0.88f,r*1.22f);
    setHarm(p.ops[1],tone.body);
    setOp(p.ops[2],Wave::Custom,2.0f,1.1f,tone.midLevel,a+0.008f,0.25f,0.83f,r*0.92f);
    setHarm(p.ops[2],tone.mid);
    setOp(p.ops[3],Wave::Custom,3.0f,0.0f,tone.bridgeLevel,a+0.003f,0.14f,0.74f,r*0.8f);
    setHarm(p.ops[3],tone.bridge);
    setHarmMute(p.ops[3],tone.bridgePressed);
    setOp(p.ops[4],Wave::Noise,1.0f,0.0f,tone.noiseLevel,0.012f,0.16f,0.84f,0.10f);
    setOp(p.ops[5],Wave::Noise,1.0f,0.0f,tone.scratchLevel,0.0015f,0.060f,0.03f,0.09f);
    p.matrix[3][0]=tone.fmDepth; // very subtle nonlinear string bite
    p.filter={FilterType::Lowpass,tone.cutoff,tone.resonance,0.05f,
              {0.045f,0.30f,0.6f,0.27f}};
    p.amp={a,0.22f,0.94f,r};
    p.velocityAmp=0.78f;
    p.velocityFilter=0.24f;
    p.lfo={tone.vibratoRate,tone.vibratoDepth,0.38f,0.25f,LfoTarget::Pitch};
    slots(p, {
        {ModTarget::Morph1,0.0f,1.0f}, // fundamental harmonics: smooth -> pressed
        {ModTarget::Morph4,0.0f,1.0f}, // high harmonic/bow bite
        {ModTarget::Op5,0.006f,tone.noiseMax}, // continuous hair/string friction
        {ModTarget::Op6,0.002f,tone.scratchMax}, // scratch at bow onset
        {ModTarget::Cutoff,tone.softCutoff,tone.hardCutoff},
        {ModTarget::Fm,0.25f,1.5f} // subtle nonlinear roughness
    });
    p.unison=0.0f; // solo instrument: avoid synthetic ensemble doubling
    p.glide=0.0f;
    p.volume=0.87f;
    p.fx.distortion=0.0f;
    p.fx.delay=0.0f;
    p.fx.delayFeedback=0.0f;
    p.fx.reverb=tone.reverb;
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
        case FactoryPreset::Violin: name="Violin"; break;
        case FactoryPreset::Viola: name="Viola"; break;
        case FactoryPreset::Cello: name="Cello"; break;
        case FactoryPreset::Contrabass: name="Contrabass"; break;
        default: break;
    }
    Patch p = makeDefaultPatch(name);

    if(preset==FactoryPreset::Violin ||
       preset==FactoryPreset::Viola ||
       preset==FactoryPreset::Cello ||
       preset==FactoryPreset::Contrabass){
        configureBowedString(p,preset);
        return p;
    }

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
        makeFactoryPatch(FactoryPreset::Nexdrum),
        makeFactoryPatch(FactoryPreset::Violin),
        makeFactoryPatch(FactoryPreset::Viola),
        makeFactoryPatch(FactoryPreset::Cello),
        makeFactoryPatch(FactoryPreset::Contrabass)
    };
    return bank;
}

} // namespace aiora
