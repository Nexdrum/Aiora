#include "NexdrumKit.h"
#include "FactoryPresets.h"

namespace aiora {
namespace {
Patch pad0() {
    Patch p = makeDefaultPatch("Cymbal 1.6 - Full Choke");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Noise; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.83f; p.ops[0].env={0.001f,1.35f,0.0f,1.7f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Noise; p.ops[1].ratio=0.125f; p.ops[1].detuneCents=0.0f; p.ops[1].level=0.64f; p.ops[1].env={0.628f,1.05f,0.0f,0.69f};
    p.ops[1].harm={1.0f,0.5f,0.33f,0.25f,0.2f,0.16f,0.14f,0.12f,0.1f,0.09f,0.08f,0.07f,0.06f,0.05f,0.04f,0.03f}; p.ops[1].hasHarm=true;
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Sine; p.ops[2].ratio=1.0f; p.ops[2].detuneCents=-3.0f; p.ops[2].level=0.35f; p.ops[2].env={0.001f,1.14f,0.0f,1.25f};
    p.ops[2].harm={1.0f,1.0f,1.0f,0.25f,1.0f,0.0f,0.14f,0.0f,0.1f,0.09f,0.08f,0.0f,0.0f,0.0f,0.0f,1.0f}; p.ops[2].hasHarm=true;
    p.ops[2].harmMute={1.0f,0.12f,0.045f,0.018f,0.008f,0.004f,0.002f,0.001f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f}; p.ops[2].hasHarmMute=true;
    p.ops[3].enabled=true; p.ops[3].wave=Wave::Noise; p.ops[3].ratio=2.0f; p.ops[3].detuneCents=0.0f; p.ops[3].level=0.84f; p.ops[3].env={0.001f,0.18f,0.43f,0.02f};
    p.ops[3].harm={1.0f,0.5f,0.33f,0.25f,0.2f,0.16f,0.14f,0.12f,0.1f,0.09f,0.08f,0.07f,0.06f,0.05f,0.04f,0.03f}; p.ops[3].hasHarm=true;
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=80.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Highpass,5800.0f,0.75f,0.0f,{0.005f,0.1f,0.5f,0.12f}};
    p.amp={0.001f,1.55f,0.0f,1.7f};
    p.velocityAmp=0.5f; p.velocityFilter=0.22f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=4;
    p.modSlots[0]={ModTarget::Volume,1.0f,0.0f};
    p.modSlots[1]={ModTarget::Op1,0.5f,0.025f};
    p.modSlots[2]={ModTarget::Op2,0.28f,0.035f};
    p.modSlots[3]={ModTarget::Morph3,0.0f,1.0f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.0f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.78f;
    p.fundamentalMidi=80;
    return p;
}

Patch pad1() {
    Patch p = makeDefaultPatch("Ride 0.E - Bell to Crash");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Noise; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.19f; p.ops[0].env={0.001f,1.2f,0.0f,1.35f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=2.72f; p.ops[1].detuneCents=2.0f; p.ops[1].level=0.09f; p.ops[1].env={0.001f,0.72f,0.0f,0.36f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Custom; p.ops[2].ratio=1.0f; p.ops[2].detuneCents=-3.0f; p.ops[2].level=0.08f; p.ops[2].env={0.001f,1.01f,0.0f,0.74f};
    p.ops[2].harm={0.1f,0.05f,0.16f,1.0f,0.08f,0.62f,0.05f,0.42f,0.04f,0.3f,0.03f,0.22f,0.025f,0.17f,0.02f,0.13f}; p.ops[2].hasHarm=true;
    p.ops[2].harmMute={0.28f,0.22f,0.35f,0.31f,0.42f,0.28f,0.36f,0.24f,0.31f,0.22f,0.27f,0.18f,0.23f,0.15f,0.19f,0.13f}; p.ops[2].hasHarmMute=true;
    p.ops[3].enabled=true; p.ops[3].wave=Wave::Sine; p.ops[3].ratio=4.17f; p.ops[3].detuneCents=-5.0f; p.ops[3].level=0.04f; p.ops[3].env={0.001f,0.58f,0.0f,0.47f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=80.0f; p.ops[5].level=0.54f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Highpass,1800.0f,1.35f,0.0f,{0.005f,0.1f,0.5f,0.12f}};
    p.amp={0.001f,1.25f,0.0f,1.55f};
    p.velocityAmp=0.52f; p.velocityFilter=0.18f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=4;
    p.modSlots[0]={ModTarget::Morph3,0.0f,1.0f};
    p.modSlots[1]={ModTarget::Op1,0.08f,0.62f};
    p.modSlots[2]={ModTarget::Op2,0.56f,0.08f};
    p.modSlots[3]={ModTarget::Cutoff,2300.0f,850.0f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.14f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.78f;
    p.fundamentalMidi=73;
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
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,900.0f,380.0f};
    p.modSlots[1]={ModTarget::AmpRelease,0.15f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.15f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.8f;
    p.fundamentalMidi=66;
    return p;
}

Patch pad3() {
    Patch p = makeDefaultPatch("Snare -0.3");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Sine; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.59f; p.ops[0].env={0.001f,0.15f,0.0f,0.02f};
    p.ops[0].harm={1.0f,0.5f,0.33f,0.25f,0.2f,0.16f,0.14f,0.12f,0.1f,0.09f,0.08f,0.07f,0.06f,0.05f,0.04f,0.03f}; p.ops[0].hasHarm=true;
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=1.0f; p.ops[1].detuneCents=0.0f; p.ops[1].level=0.04f; p.ops[1].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Noise; p.ops[2].ratio=1.0f; p.ops[2].detuneCents=0.0f; p.ops[2].level=0.6f; p.ops[2].env={0.001f,0.12f,0.0f,0.46f};
    p.ops[3].enabled=false; p.ops[3].wave=Wave::Sine; p.ops[3].ratio=1.01f; p.ops[3].detuneCents=0.0f; p.ops[3].level=0.0f; p.ops[3].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Bandpass,1800.0f,1.0f,0.0f,{0.005f,0.1f,0.5f,0.12f}};
    p.amp={0.001f,0.18f,0.0f,0.14f};
    p.velocityAmp=0.5f; p.velocityFilter=0.3f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,1800.0f,720.0f};
    p.modSlots[1]={ModTarget::AmpRelease,0.14f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.25f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.85f;
    p.fundamentalMidi=59;
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
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,1400.0f,560.0f};
    p.modSlots[1]={ModTarget::AmpRelease,0.22f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
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
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=-100.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Lowpass,5000.0f,10.0f,0.0f,{0.01f,0.09f,0.17f,0.08f}};
    p.amp={0.001f,0.9f,0.0f,1.0f};
    p.velocityAmp=0.5f; p.velocityFilter=0.3f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,5000.0f,2000.0f};
    p.modSlots[1]={ModTarget::AmpRelease,1.0f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.35f};
    p.unison=0.0f; p.glide=0.0f; p.octave=0.0f; p.volume=0.75f;
    p.fundamentalMidi=45;
    return p;
}

Patch pad6() {
    Patch p = makeDefaultPatch("Kick -2");
    p.ops[0].enabled=true; p.ops[0].wave=Wave::Sine; p.ops[0].ratio=1.0f; p.ops[0].detuneCents=0.0f; p.ops[0].level=0.85f; p.ops[0].env={0.13f,0.295f,0.4f,1.51f};
    p.ops[1].enabled=true; p.ops[1].wave=Wave::Sine; p.ops[1].ratio=1.0f; p.ops[1].detuneCents=45.0f; p.ops[1].level=0.75f; p.ops[1].env={0.001f,0.06f,0.0f,0.07f};
    p.ops[2].enabled=true; p.ops[2].wave=Wave::Noise; p.ops[2].ratio=4.0f; p.ops[2].detuneCents=0.0f; p.ops[2].level=0.23f; p.ops[2].env={0.004f,0.005f,0.03f,0.02f};
    p.ops[3].enabled=true; p.ops[3].wave=Wave::Saw; p.ops[3].ratio=0.5f; p.ops[3].detuneCents=35.0f; p.ops[3].level=1.0f; p.ops[3].env={0.001f,0.03f,0.0f,0.02f};
    p.ops[4].enabled=false; p.ops[4].wave=Wave::Sine; p.ops[4].ratio=0.5f; p.ops[4].detuneCents=0.0f; p.ops[4].level=0.0f; p.ops[4].env={0.005f,0.1f,0.8f,0.15f};
    p.ops[5].enabled=false; p.ops[5].wave=Wave::Sine; p.ops[5].ratio=4.0f; p.ops[5].detuneCents=0.0f; p.ops[5].level=0.0f; p.ops[5].env={0.005f,0.1f,0.8f,0.15f};
    p.filter={FilterType::Lowpass,689.0f,6.4f,0.0f,{0.005f,0.08f,0.41f,0.12f}};
    p.amp={0.001f,0.29f,0.0f,0.23f};
    p.velocityAmp=0.67f; p.velocityFilter=0.24f;
    p.lfo={5.0f,0.0f,0.0f,0.0f,LfoTarget::None};
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,4500.0f,1800.0f};
    p.modSlots[1]={ModTarget::AmpRelease,1.3f,0.05f};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    p.fx={0.0f,0.0f,0.32f,0.0f,0.3f};
    p.unison=0.58f; p.glide=0.0f; p.octave=0.0f; p.volume=1.0f;
    p.fundamentalMidi=38;
    return p;
}

} // namespace

const std::array<DrumPad,7>& nexdrumKit() {
    static const std::array<DrumPad,7> kit{{
        DrumPad{80,80,86,"crash",pad0(),1.0f,0.0f},
        DrumPad{73,73,79,"ride",pad1(),1.0f,0.0f},
        DrumPad{66,66,72,"bongo",pad2(),1.0f,0.0f},
        DrumPad{59,59,65,"snare",pad3(),1.0f,0.0f},
        DrumPad{52,52,58,"tom",pad4(),1.0f,0.0f},
        DrumPad{45,45,51,"floortom",pad5(),1.0f,0.0f},
        DrumPad{38,38,44,"kick",pad6(),1.0f,0.0f}
    }};
    return kit;
}

} // namespace aiora
