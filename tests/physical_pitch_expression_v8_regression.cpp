#include "PhysicalFrequencyAnalyzer.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
using namespace aiora::physical;
static bool closeCents(float a,float b,float cents=1.1f){
    return a>0.f&&b>0.f&&std::fabs(1200.f*std::log2(a/b))<cents;
}
static Blueprint filament(){
    Blueprint b;
    b.graph=universalEmptyGraph();
    const int string=addPhysicalRole(b.graph,PhysicalRole::Resonator);
    const int output=addPhysicalRole(b.graph,PhysicalRole::Output);
    auto& n=b.graph.nodes[string];
    n.material=Material::Nylon;
    n.form.continuum=Continuum::Filament;
    n.form.lengthMeters=.65f;
    n.form.widthMeters=.001f;
    n.form.depthMeters=.001f;
    n.tensionNewtons=80.f;
    n.y=.38f;b.graph.nodes[output].y=.90f;
    assert(b.graph.connect(0,string));
    assert(b.graph.connect(string,output));
    b.pitch.referenceMidi=60;
    b.pitch.trackedNode=string;
    b.pitch.count=1;
    b.pitch.assignments[0]={static_cast<int8_t>(string),
        PitchProperty::Tension,1.f,400.f,1.f};
    return b;
}
static float interpolatedHz(const Blueprint& b,float position){
    std::array<float,kPitchSlots> values{};
    assert(physicalPitchValuesAt(b,position,values));
    auto g=b.graph;
    for(int i=0;i<b.pitch.count;++i)
        assert(pitchWrite(g,b.pitch.assignments[i],values[i]));
    return trackedFundamental(g,b.pitch.trackedNode);
}
static float hz(float midi){
    return 440.f*std::exp2((midi-69.f)/12.f);
}
int main(){
    auto b=filament();
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    assert(preparePhysicalBendMap(b));
    assert(physicalBendMapCurrent(b));
    for(float midi:{60.f,60.125f,60.25f,60.5f,60.75f,60.875f,61.f,
                    62.125f,64.3333f,68.5f,69.f}){
        assert(closeCents(interpolatedHz(b,midi),hz(midi),1.1f));
    }
    const auto base=interpolatedHz(b,60.f);
    assert(closeCents(base,hz(60.f)));
    const auto valueAtMidi60=b.pitch.values[60][0];
    MaterialTimeVoice voice;
    voice.prepare(48000.f);
    voice.start(b,60,.80f,0.f);
    // Let matching impulses propagate before retuning one of two otherwise
    // identical voices. The audio comparison proves B changes the SOUND,
    // not merely an internal pitch/frequency readout.
    MaterialTimeVoice unbent;
    unbent.prepare(48000.f);
    unbent.start(b,60,.80f,0.f);
    for(int i=0;i<7000;++i){
        (void)voice.render();
        (void)unbent.render();
    }
    const float stored=voice.storedModalEnergy();
    assert(stored>1.e-11f);
    voice.setPitchBend(.5f);
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(60.5f),1.1f));
    assert(closeCents(voice.currentPhysicalPitchHz(),interpolatedHz(b,60.5f),.1f));
    assert(voice.currentPitchPropertyValue(0)!=valueAtMidi60);
    assert(voice.storedModalEnergy()==stored); // NO modal state reset
    double pcmDifference=0.,referenceEnergy=0.;
    for(int i=0;i<2000;++i){
        const double dry=unbent.render();
        const double bent=voice.render();
        assert(std::isfinite(bent));
        pcmDifference+=(bent-dry)*(bent-dry);
        referenceEnergy+=dry*dry;
    }
    assert(pcmDifference>referenceEnergy*1.e-5+1.e-17);
    const float energyAfterPlayback=voice.storedModalEnergy();
    voice.setExpression(.65f,.74f); // B does not steal V or M
    voice.setPitchBend(1.375f);
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(61.375f),1.1f));
    assert(voice.storedModalEnergy()==energyAfterPlayback);
    assert(voice.active());
    voice.setPitchBend(0.f);
    assert(closeCents(voice.currentPhysicalPitchHz(),base,.2f));
    // Per-note polyphony: retuning one voice must not retune its neighbor.
    MaterialTimeVoice other;
    other.prepare(48000.f);
    other.start(b,60,.70f,0.f);
    voice.setPitchBend(4.25f);
    assert(closeCents(other.currentPhysicalPitchHz(),base,.2f));
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(64.25f),1.1f));
    // Physical range limits; tuning remains reachable rather than inventing
    // a pitch shift to a high note the mechanism cannot produce.
    voice.setPitchBend(75.f);
    assert(voice.currentPhysicalPitchHz()<hz(90.f));
    assert(voice.currentPhysicalPitchHz()>0.f);
    // Changing the instrument invalidates the sub-semitone map.
    b.graph.nodes[1].form.lengthMeters=.50f;
    assert(!physicalBendMapCurrent(b));
    b=filament();
    b.pitch.count=2;
    b.pitch.assignments[0]={1,PitchProperty::Tension,40.f,130.f,2.f};
    b.pitch.assignments[1]={1,PitchProperty::Length,.25f,.65f,1.f};
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    assert(preparePhysicalBendMap(b));
    for(float midi:{60.125f,61.5f,63.25f,65.5f,69.125f}){
        assert(closeCents(interpolatedHz(b,midi),hz(midi),1.4f));
    }
    voice.start(b,60,.75f,.3f);
    const float beforeTension=voice.currentPitchPropertyValue(0);
    const float beforeLength=voice.currentPitchPropertyValue(1);
    voice.setPitchBend(9.125f);
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(69.125f),1.4f));
    assert(voice.currentPitchPropertyValue(0)!=beforeTension||
           voice.currentPitchPropertyValue(1)!=beforeLength);
    // A string feeds a physical air chamber; bend moves only the chamber,
    // leaving all string geometry and tension unchanged.
    auto downstream=filament();
    const int chamber=addPhysicalRole(downstream.graph,PhysicalRole::Resonator);
    downstream.graph.nodes[chamber].form.continuum=Continuum::Gas;
    downstream.graph.nodes[chamber].material=Material::Air;
    downstream.graph.nodes[chamber].form.lengthMeters=.65f;
    downstream.graph.nodes[chamber].form.opening=.95f;
    downstream.graph.nodes[chamber].y=.65f;
    assert(downstream.graph.disconnect(1,2));
    assert(downstream.graph.connect(1,chamber));
    assert(downstream.graph.connect(chamber,2));
    downstream.pitch.trackedNode=chamber;
    downstream.pitch.assignments[0]={
        static_cast<int8_t>(chamber),PitchProperty::Length,.10f,1.5f,1.f};
    assert(buildPhysicalKeyboard(downstream.graph,downstream.pitch));
    assert(preparePhysicalBendMap(downstream));
    voice.start(downstream,60,.70f,0.f);
    const auto tension=downstream.graph.nodes[1].tensionNewtons;
    const auto stringLength=downstream.graph.nodes[1].form.lengthMeters;
    voice.setPitchBend(-.5f);
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(59.5f),1.4f));
    assert(downstream.graph.nodes[1].tensionNewtons==tension);
    assert(downstream.graph.nodes[1].form.lengthMeters==stringLength);
    const auto unchanged=voice.storedModalEnergy();
    voice.setPitchBend(.25f);
    assert(closeCents(voice.currentPhysicalPitchHz(),hz(60.25f),1.4f));
    assert(voice.storedModalEnergy()==unchanged);
    // A disabled tuning map must not silently turn B into generic audio pitch.
    downstream.pitch.enabled=false;
    voice.start(downstream,60,.75f,0.f);
    const float fixed=voice.currentPhysicalPitchHz();
    voice.setPitchBend(4.0f);
    assert(voice.currentPhysicalPitchHz()==fixed);
    std::printf("PASS v8 physical pitch bend: sub-semitones, modal state continuity, polyphony, multi-control, air chamber, bounds, no pitch shifting.\n");
}
