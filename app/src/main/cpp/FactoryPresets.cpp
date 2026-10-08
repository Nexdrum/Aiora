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


// Woodwind voices use pitched harmonic bore resonance, physically different
// reed/air edge spectra, and controlled stochastic breath or reed turbulence.
// M is a TIMBRE gesture. Velocity remains the separate loudness gesture.
// No samples or changes to the DSP/project format are necessary.
struct WoodwindTone {
    std::array<float,16> core, corePressed, bore, upper, edge, edgePressed;
    float attack{0.06f},release{0.22f};
    float cutoff{4200.0f},darkCutoff{2600.0f},brightCutoff{7200.0f},resonance{0.85f};
    float bodyLevel{0.16f},upperLevel{0.06f},edgeLevel{0.06f},edgeMax{0.22f};
    float airLevel{0.005f},airMax{0.055f},tongueLevel{0.018f},tongueMax{0.035f};
    float vibratoRate{5.2f},vibratoDepth{0.04f},fmDepth{0.009f},reverb{0.10f};
};

void configureWoodwind(Patch& p,FactoryPreset preset) {
    WoodwindTone t{};
    if(preset==FactoryPreset::Flute) {
        // Edge-blown air jet: dominant fundamental, weak harmonics.
        // At high M the jet becomes breathier, not merely louder.
        t.core={1,.23f,.085f,.042f,.021f,.012f,.007f,.004f,.003f,.002f,.001f,0,0,0,0,0};
        t.corePressed={1,.14f,.060f,.028f,.015f,.008f,.004f,.002f,.001f,0,0,0,0,0,0,0};
        t.bore={1,.32f,.11f,.038f,.018f,.008f,.003f,.002f,0,0,0,0,0,0,0,0};
        t.upper={1,.18f,.067f,.025f,.009f,.003f,.001f,0,0,0,0,0,0,0,0,0};
        t.edge={1,.29f,.12f,.075f,.044f,.025f,.014f,.008f,.004f,.003f,.002f,.001f,0,0,0,0};
        t.edgePressed={1,.35f,.19f,.13f,.09f,.066f,.047f,.033f,.022f,.015f,.010f,.006f,.004f,.002f,.001f,0};
        t.attack=.064f;t.release=.17f;
        t.cutoff=5700;t.darkCutoff=4300;t.brightCutoff=8500;t.resonance=.68f;
        t.bodyLevel=.095f;t.upperLevel=.028f;t.edgeLevel=.012f;t.edgeMax=.039f;
        t.airLevel=.004f;t.airMax=.155f;t.tongueLevel=.012f;t.tongueMax=.028f;
        t.vibratoRate=5.25f;t.vibratoDepth=.034f;t.fmDepth=.0015f;t.reverb=.085f;
    }else if(preset==FactoryPreset::Clarinet){
        // Cylindrical, effectively stopped bore: hollow odd-harmonic
        // chalumeau-like timbre, with more even and upper partials under M.
        t.core={1,.040f,.72f,.025f,.37f,.014f,.22f,.010f,.13f,.006f,.081f,.004f,.045f,.002f,.026f,.001f};
        t.corePressed={1,.13f,.86f,.095f,.55f,.07f,.39f,.055f,.27f,.036f,.18f,.025f,.12f,.019f,.077f,.015f};
        t.bore={1,.02f,.69f,.018f,.37f,.012f,.19f,.008f,.10f,.005f,.05f,.003f,.022f,.002f,.011f,.001f};
        t.upper={1,.21f,.48f,.13f,.32f,.09f,.23f,.07f,.15f,.045f,.10f,.025f,.060f,.018f,.035f,.01f};
        t.edge={1,.24f,.66f,.14f,.44f,.1f,.30f,.065f,.2f,.045f,.13f,.032f,.085f,.021f,.055f,.014f};
        t.edgePressed={1,.39f,.78f,.32f,.65f,.24f,.48f,.18f,.34f,.13f,.23f,.095f,.16f,.07f,.11f,.05f};
        t.attack=.040f;t.release=.205f;
        t.cutoff=3500;t.darkCutoff=2000;t.brightCutoff=6400;t.resonance=.89f;
        t.bodyLevel=.20f;t.upperLevel=.065f;t.edgeLevel=.045f;t.edgeMax=.23f;
        t.airLevel=.002f;t.airMax=.031f;t.tongueLevel=.015f;t.tongueMax=.035f;
        t.vibratoRate=5.0f;t.vibratoDepth=.006f;t.fmDepth=.008f;t.reverb=.092f;
    }else if(preset==FactoryPreset::Oboe){
        // Double reed + conical bore: present 2nd/3rd/4th harmonics,
        // a narrow, nasal formant, and increasingly driven reed buzz.
        t.core={.86f,1,.92f,.76f,.54f,.40f,.30f,.22f,.16f,.13f,.095f,.074f,.055f,.038f,.026f,.018f};
        t.corePressed={.82f,1,.98f,.93f,.80f,.72f,.59f,.48f,.39f,.32f,.26f,.21f,.17f,.13f,.10f,.077f};
        t.bore={.70f,.98f,1,.85f,.65f,.48f,.32f,.23f,.16f,.11f,.074f,.048f,.032f,.02f,.013f,.008f};
        t.upper={.62f,.95f,1,.85f,.71f,.56f,.45f,.35f,.27f,.20f,.14f,.10f,.07f,.047f,.03f,.02f};
        t.edge={.39f,.69f,.92f,1,.92f,.79f,.66f,.55f,.44f,.34f,.26f,.20f,.16f,.115f,.087f,.064f};
        t.edgePressed={.36f,.65f,.89f,1,.99f,.92f,.85f,.75f,.66f,.56f,.47f,.39f,.31f,.25f,.20f,.16f};
        t.attack=.051f;t.release=.19f;
        t.cutoff=4500;t.darkCutoff=2850;t.brightCutoff=8000;t.resonance=1.08f;
        t.bodyLevel=.245f;t.upperLevel=.10f;t.edgeLevel=.075f;t.edgeMax=.29f;
        t.airLevel=.007f;t.airMax=.064f;t.tongueLevel=.021f;t.tongueMax=.050f;
        t.vibratoRate=5.4f;t.vibratoDepth=.053f;t.fmDepth=.019f;t.reverb=.092f;
    }else if(preset==FactoryPreset::Bassoon){
        // The larger folded double-reed bore gives strong low/mid partials,
        // a muted woody core and a reedy growl as embouchure pressure rises.
        t.core={1,.84f,.76f,.62f,.48f,.37f,.27f,.19f,.14f,.10f,.075f,.055f,.04f,.028f,.019f,.013f};
        t.corePressed={.95f,.93f,.88f,.81f,.70f,.62f,.52f,.43f,.35f,.29f,.23f,.18f,.14f,.105f,.079f,.059f};
        t.bore={1,.95f,.79f,.53f,.33f,.20f,.11f,.063f,.037f,.022f,.013f,.008f,.004f,.003f,.001f,0};
        t.upper={.88f,1,.92f,.74f,.55f,.37f,.24f,.15f,.095f,.060f,.036f,.021f,.012f,.007f,.004f,.002f};
        t.edge={.67f,.91f,1,.87f,.69f,.52f,.40f,.30f,.23f,.17f,.12f,.089f,.063f,.046f,.032f,.022f};
        t.edgePressed={.58f,.81f,1,.98f,.89f,.79f,.69f,.60f,.51f,.43f,.35f,.29f,.23f,.18f,.14f,.11f};
        t.attack=.079f;t.release=.285f;
        t.cutoff=2600;t.darkCutoff=1550;t.brightCutoff=5600;t.resonance=1.07f;
        t.bodyLevel=.29f;t.upperLevel=.085f;t.edgeLevel=.045f;t.edgeMax=.235f;
        t.airLevel=.003f;t.airMax=.053f;t.tongueLevel=.014f;t.tongueMax=.034f;
        t.vibratoRate=4.7f;t.vibratoDepth=.041f;t.fmDepth=.013f;t.reverb=.105f;
    }

    const float a=t.attack,r=t.release;
    setOp(p.ops[0],Wave::Custom,1.0f,0.0f,.83f,a,.20f,.94f,r);
    setHarm(p.ops[0],t.core);
    setHarmMute(p.ops[0],t.corePressed);

    // Independent air-column/body and octave resonances; no chorus unison.
    setOp(p.ops[1],Wave::Custom,1.0f,0.0f,t.bodyLevel,
        a+.012f,.27f,.87f,r*1.1f);
    setHarm(p.ops[1],t.bore);
    setOp(p.ops[2],Wave::Custom,2.0f,0.0f,t.upperLevel,
        a+.006f,.23f,.78f,r*.86f);
    setHarm(p.ops[2],t.upper);

    // Reed/edge coloration. M shapes spectral distribution and this layer.
    setOp(p.ops[3],Wave::Custom,1.0f,0.0f,t.edgeLevel,
        std::max(.006f,a*.65f),.15f,.81f,r*.83f);
    setHarm(p.ops[3],t.edge);
    setHarmMute(p.ops[3],t.edgePressed);
    setOp(p.ops[4],Wave::Noise,1.0f,0.0f,t.airLevel,
        .009f,.17f,.81f,.08f);
    setOp(p.ops[5],Wave::Noise,1.0f,0.0f,t.tongueLevel,
        .0015f,.035f,.0f,.065f);
    p.matrix[3][0]=t.fmDepth;

    p.filter={FilterType::Lowpass,t.cutoff,t.resonance,.038f,
        {.035f,.21f,.58f,.16f}};
    p.amp={a,.24f,.96f,r};
    p.velocityAmp=.77f;
    p.velocityFilter=preset==FactoryPreset::Flute?.17f:.27f;
    p.lfo={t.vibratoRate,t.vibratoDepth,.30f,.34f,LfoTarget::Pitch};

    // Flute: jet turbulence; clarinet: single-reed embouchure bite;
    // oboe/bassoon: double-reed compression with different formants.
    if(preset==FactoryPreset::Flute){
        slots(p,{
            {ModTarget::Morph1,0.0f,.65f},
            {ModTarget::Morph4,0.0f,.68f},
            {ModTarget::Op4,.012f,t.edgeMax},
            {ModTarget::Op5,t.airLevel,t.airMax},
            {ModTarget::Op6,t.tongueLevel,t.tongueMax},
            {ModTarget::Cutoff,t.darkCutoff,t.brightCutoff}
        });
    }else{
        slots(p,{
            {ModTarget::Morph1,0.0f,1.0f},
            {ModTarget::Morph4,0.0f,1.0f},
            {ModTarget::Op4,t.edgeLevel,t.edgeMax},
            {ModTarget::Op5,t.airLevel,t.airMax},
            {ModTarget::Cutoff,t.darkCutoff,t.brightCutoff},
            {ModTarget::Fm,.30f,1.25f}
        });
    }
    p.unison=0.0f;p.glide=0.0f;p.volume=.86f;
    p.fx.distortion=0.0f;p.fx.delay=0.0f;
    p.fx.delayFeedback=0.0f;p.fx.reverb=t.reverb;
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
        case FactoryPreset::Flute: name="Flute"; break;
        case FactoryPreset::Clarinet: name="Clarinet"; break;
        case FactoryPreset::Oboe: name="Oboe"; break;
        case FactoryPreset::Bassoon: name="Bassoon"; break;
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

    if(preset==FactoryPreset::Flute ||
       preset==FactoryPreset::Clarinet ||
       preset==FactoryPreset::Oboe ||
       preset==FactoryPreset::Bassoon){
        configureWoodwind(p,preset);
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
        makeFactoryPatch(FactoryPreset::Contrabass),
        makeFactoryPatch(FactoryPreset::Flute),
        makeFactoryPatch(FactoryPreset::Clarinet),
        makeFactoryPatch(FactoryPreset::Oboe),
        makeFactoryPatch(FactoryPreset::Bassoon)
    };
    return bank;
}

} // namespace aiora
