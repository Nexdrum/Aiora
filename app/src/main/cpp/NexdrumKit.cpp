#include "NexdrumKit.h"
#include "FactoryPresets.h"
#include <initializer_list>

namespace aiora {
namespace {
void drumOp(Patch& p,size_t i,Wave w,float ratio,float level,
            float attack,float decay,float release,float sustain=0.0f) {
    auto& o=p.ops[i];o={};o.enabled=true;o.wave=w;o.ratio=ratio;
    o.level=level;o.env={attack,decay,sustain,release};
}
void drumReset(Patch& p) {
    for(auto& o:p.ops){o={};o.enabled=false;o.level=0.0f;}
    for(auto& row:p.matrix)row.fill(0.0f);
    p.unison=0.0f;p.glide=0.0f;p.octave=0.0f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
}
void drumMods(Patch& p,std::initializer_list<ModSlot> slots) {
    p.modSlots.assign(slots);p.modSlotCount=p.modSlots.size();
}
void drumVelocity(Patch& p,std::initializer_list<ModSlot> slots) {
    p.velocitySlots.assign(slots);p.velocitySlotCount=p.velocitySlots.size();
}
Patch pad0() {
    Patch p=makeDefaultPatch("Cymbal - Tap / Crash / Choke");
    drumReset(p);
    drumOp(p,0,Wave::Noise,1.0f,.56f,.001f,.19f,.22f);
    drumOp(p,1,Wave::Noise,1.0f,.26f,.001f,1.22f,1.55f);
    drumOp(p,2,Wave::Custom,1.68f,.23f,.001f,1.05f,1.5f);
    p.ops[2].hasHarm=true;
    p.ops[2].harm={.08f,.36f,.07f,.52f,.06f,.36f,.05f,.28f,.04f,.17f,.03f,.12f,.02f,.08f,.01f,.05f};
    drumOp(p,3,Wave::Noise,1.0f,.37f,.001f,.027f,.026f);
    drumOp(p,4,Wave::Sine,2.93f,.12f,.001f,1.14f,1.65f);
    drumOp(p,5,Wave::Sine,4.41f,.085f,.001f,.67f,1.0f);
    p.cymbalModel=1;p.timpaniMembrane=true;
    p.filter={FilterType::Highpass,1050.0f,.78f,0.0f,{.005f,.1f,.5f,.1f}};
    p.amp={.001f,1.65f,0.0f,2.45f};
    p.velocityAmp=.77f;p.velocityFilter=.15f;
    // V excites the noise wash progressively: a soft tap remains metallic,
    // while a hard strike brings the full broad-spectrum crash.
    // M stays free for independent choking/damping and edge color.
    drumMods(p,{{ModTarget::Cutoff,950.0f,1600.0f},
                {ModTarget::AmpRelease,2.45f,.09f}});
    drumVelocity(p,{{ModTarget::Op1,.035f,.56f},
                    {ModTarget::Op2,.014f,.26f},
                    {ModTarget::Op3,.10f,.24f},
                    {ModTarget::Op4,.020f,.37f}});
    p.fx={0.0f,0.0f,.32f,0.0f,.095f};
    p.volume=.61f;p.fundamentalMidi=80;
    return p;
}

Patch pad1() {
    Patch p=makeDefaultPatch("Ride - Ping / Crash / Choke");
    drumReset(p);
    drumOp(p,0,Wave::Noise,1.0f,.25f,.001f,.22f,.32f);
    drumOp(p,1,Wave::Sine,2.72f,.30f,.001f,1.45f,2.8f);
    drumOp(p,2,Wave::Custom,1.0f,.24f,.001f,1.95f,2.8f);
    p.ops[2].hasHarm=true;
    p.ops[2].harm={.06f,.12f,.09f,.66f,.10f,.58f,.06f,.4f,.04f,.27f,.03f,.2f,.02f,.15f,.01f,.09f};
    drumOp(p,3,Wave::Noise,1.0f,.11f,.001f,.032f,.035f);
    drumOp(p,4,Wave::Sine,4.17f,.13f,.001f,2.2f,2.8f);
    drumOp(p,5,Wave::Sine,5.68f,.075f,.001f,1.7f,2.5f);
    p.cymbalModel=2;p.timpaniMembrane=true;
    // A thicker metal plate retains its lower modes. The short stick tick
    // and moderate wash respond to V, while M controls bow/bell and choke.
    p.filter={FilterType::Highpass,420.0f,.78f,0.0f,{.005f,.1f,.5f,.1f}};
    p.amp={.001f,2.70f,0.0f,5.80f};
    p.velocityAmp=.74f;p.velocityFilter=.12f;
    drumMods(p,{{ModTarget::Op2,.34f,.18f},
                {ModTarget::Op3,.24f,.08f},
                {ModTarget::Cutoff,420.0f,1100.0f},
                {ModTarget::AmpRelease,5.80f,.12f}});
    drumVelocity(p,{{ModTarget::Op1,.008f,.22f},
                    {ModTarget::Op4,.006f,.10f},
                    {ModTarget::Op5,.050f,.16f},
                    {ModTarget::Op6,.020f,.105f}});
    p.fx={0.0f,0.0f,.32f,0.0f,.08f};
    p.volume=.64f;p.fundamentalMidi=73;
    return p;
}

Patch pad2() {
    Patch p = makeDefaultPatch("Bongo 0.4");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Sine; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.9f; p.ops[0].env={0.001f,0.18f,0.0f,0.14f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=2.0f; p.ops[1].detuneCents=3.0f; p.ops[1].level=0.3f; p.ops[1].env={0.001f,0.14f,0.0f,0.12f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Noise; p.ops[2].ratio=1.0f; p.ops[2].detuneCents=0.0f; p.ops[2].level=0.18f; p.ops[2].env={0.001f,0.03f,0.0f,0.05f};
    p.ops[3].enabled=false; p.ops[3].wave=Wave::Sine; p.ops[3].ratio=1.01f; p.ops[3].detuneCents=0.0f; p.ops[3].level=0.0f; p.ops[3].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Bandpass,900.0f,1.2f,0.0f,{0.005f,0.1f,0.5f,0.12f}};
    p.amp={0.001f,0.18f,0.0f,0.15f};
    p.velocityAmp=0.55f; p.velocityFilter=0.35f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;p.modSlots.resize(3);
    p.modSlots[0]={ModTarget::Cutoff,900.0f,380.0f};
    p.modSlots[1]={ModTarget::AmpRelease,0.15f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    drumVelocity(p,{{ModTarget::Op3,.035f,.18f}});
    p.fx={0.0f,0.0f,0.32f,0.0f,0.15f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.8f;
    p.fundamentalMidi=66;
    return p;
}

Patch pad3() {
    Patch p=makeDefaultPatch("Snare - Head / Wires");
    drumReset(p);
    drumOp(p,0,Wave::Sine,1.0f,.63f,.001f,.18f,.07f);
    drumOp(p,1,Wave::Sine,1.67f,.18f,.001f,.065f,.04f);
    drumOp(p,2,Wave::Noise,1.0f,.58f,.001f,.16f,.22f);
    drumOp(p,3,Wave::Noise,1.0f,.33f,.001f,.052f,.08f);
    drumOp(p,4,Wave::Sine,2.49f,.11f,.001f,.055f,.06f);
    p.filter={FilterType::Highpass,145.0f,.74f,0.0f,{.005f,.1f,.5f,.1f}};
    p.amp={.001f,.20f,0.0f,.115f};
    p.velocityAmp=.70f;p.velocityFilter=.18f;
    p.timpaniMembrane=true;
    drumMods(p,{{ModTarget::Op1,.72f,.38f},
                {ModTarget::AmpRelease,.13f,.045f}});
    drumVelocity(p,{{ModTarget::Op3,.08f,.58f},
                    {ModTarget::Op4,.025f,.33f},
                    {ModTarget::Op2,.075f,.18f}});
    p.fx={0.0f,0.0f,.32f,0.0f,.07f};
    p.volume=.69f;p.fundamentalMidi=59;
    return p;
}

Patch pad4() {
    Patch p = makeDefaultPatch("Tom -0.X");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Sine; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.9f; p.ops[0].env={0.002f,0.3f,0.0f,0.2f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=1.5f; p.ops[1].detuneCents=4.0f; p.ops[1].level=0.35f; p.ops[1].env={0.002f,0.24f,0.0f,0.16f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Noise; p.ops[2].ratio=1.0f; p.ops[2].detuneCents=0.0f; p.ops[2].level=0.22f; p.ops[2].env={0.001f,0.05f,0.0f,0.08f};
    p.ops[3].enabled=false; p.ops[3].wave=Wave::Sine; p.ops[3].ratio=1.01f; p.ops[3].detuneCents=0.0f; p.ops[3].level=0.0f; p.ops[3].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Lowpass,1400.0f,1.2f,0.0f,{0.005f,0.1f,0.5f,0.12f}};
    p.amp={0.002f,0.32f,0.0f,0.22f};
    p.velocityAmp=0.5f; p.velocityFilter=0.3f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;p.modSlots.resize(3);
    p.modSlots[0]={ModTarget::Cutoff,1400.0f,560.0f};
    p.modSlots[1]={ModTarget::AmpRelease,0.22f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    drumVelocity(p,{{ModTarget::Op3,.045f,.22f}});
    p.fx={0.0f,0.0f,0.32f,0.0f,0.2f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.85f;
    p.fundamentalMidi=52;
    return p;
}

Patch pad5() {
    Patch p = makeDefaultPatch("Floor tom -1.5");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Sine; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.79f; p.ops[0].env={0.091f,0.39f,0.14f,0.42f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=1.0f; p.ops[1].detuneCents=37.0f; p.ops[1].level=0.67f; p.ops[1].env={0.001f,0.08f,0.0f,0.13f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Noise; p.ops[2].ratio=2.0f; p.ops[2].detuneCents=0.0f; p.ops[2].level=0.29f; p.ops[2].env={0.001f,0.005f,0.0f,0.02f};
    p.ops[3].enabled=false; p.ops[3].wave=Wave::Sine; p.ops[3].ratio=1.01f; p.ops[3].detuneCents=0.0f; p.ops[3].level=0.0f; p.ops[3].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].semitoneOffset=-1.0f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Lowpass,5000.0f,10.0f,0.0f,{0.01f,0.09f,0.17f,0.08f}};
    p.amp={0.001f,0.9f,0.0f,1.0f};
    p.velocityAmp=0.5f; p.velocityFilter=0.3f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;p.modSlots.resize(3);
    p.modSlots[0]={ModTarget::Cutoff,5000.0f,2000.0f};
    p.modSlots[1]={ModTarget::AmpRelease,1.0f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    drumVelocity(p,{{ModTarget::Op3,.035f,.29f}});
    p.fx={0.0f,0.0f,0.32f,0.0f,0.35f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.75f;
    p.fundamentalMidi=45;
    return p;
}

Patch pad6() {
    Patch p=makeDefaultPatch("Kick - Metal Double Pedal");
    drumReset(p);
    // Settled D2 membrane fundamental stays ratio one with zero detune.
    drumOp(p,0,Wave::Sine,1.0f,.91f,.0005f,.148f,.072f);
    drumOp(p,1,Wave::Sine,2.0f,.28f,.0005f,.042f,.04f);
    drumOp(p,2,Wave::Noise,1.0f,.45f,.0005f,.013f,.012f);
    drumOp(p,3,Wave::Triangle,4.0f,.19f,.0005f,.023f,.017f);
    drumOp(p,4,Wave::Noise,1.0f,.18f,.0005f,.006f,.006f);
    p.drumPitchDrop=15.0f;p.drumPitchDropSeconds=.018f;
    p.filter={FilterType::Lowpass,7400.0f,.70f,0.0f,{.005f,.10f,.5f,.1f}};
    p.amp={.0005f,.165f,0.0f,.075f};
    p.velocityAmp=.78f;p.velocityFilter=.12f;
    // M raises hard-plastic beater click and tightens note release.
    drumMods(p,{{ModTarget::AmpRelease,.095f,.037f},
                {ModTarget::Cutoff,4900.0f,10200.0f}});
    drumVelocity(p,{{ModTarget::Op3,.05f,.45f},
                    {ModTarget::Op4,.02f,.19f},
                    {ModTarget::Op5,.025f,.18f}});
    p.fx={0.0f,0.0f,.32f,0.0f,.012f};
    p.volume=.88f;p.fundamentalMidi=38;
    return p;
}

} // namespace

const std::array<DrumPad,7>& nexdrumKit() {
    static const std::array<DrumPad,7> kit{{
        DrumPad{80,80,86,"crash",pad0(),0.76f,0.24f},
        DrumPad{73,73,79,"ride",pad1(),0.78f,-0.20f},
        DrumPad{66,66,72,"bongo",pad2(),0.72f,0.27f},
        DrumPad{59,59,65,"snare",pad3(),0.86f,0.0f},
        DrumPad{52,52,58,"tom",pad4(),0.72f,0.19f},
        DrumPad{45,45,51,"floortom",pad5(),0.77f,-0.18f},
        DrumPad{38,38,44,"kick",pad6(),0.92f,0.0f}
    }};
    return kit;
}

} // namespace aiora
