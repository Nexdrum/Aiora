#include "MaterialTimeVoice.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;
static bool nearCents(float a,float b,float limit=1.f){
    return a>0.f&&b>0.f&&std::fabs(1200.f*std::log2(a/b))<=limit;
}
static Blueprint filamentPatch(){
    Blueprint b;
    b.graph=universalEmptyGraph();
    const int wire=addPhysicalRole(b.graph,PhysicalRole::Resonator);
    const int output=addPhysicalRole(b.graph,PhysicalRole::Output);
    auto& n=b.graph.nodes[wire];
    n.form.continuum=Continuum::Filament;
    n.material=Material::Nylon;
    n.form.lengthMeters=.65f;
    n.form.widthMeters=.001f;
    n.form.depthMeters=.001f;
    n.tensionNewtons=80.f;
    n.x=.45f;n.y=.37f;
    b.graph.nodes[output].y=.90f;
    assert(b.graph.connect(0,wire));
    assert(b.graph.connect(wire,output));
    b.pitch.trackedNode=wire;
    b.pitch.referenceMidi=60;
    b.pitch.count=1;
    b.pitch.assignments[0]={static_cast<int8_t>(wire),PitchProperty::Tension,1.f,400.f,1.f};
    return b;
}
int main(){
    auto b=filamentPatch();
    assert(pitchHasCompleteEnergyPath(b.graph,1));
    {
        auto disconnected=b;
        assert(disconnected.graph.disconnect(1,2));
        assert(!buildPhysicalKeyboard(disconnected.graph,disconnected.pitch));
    }
    const auto natural=physicalFundamental(b.graph.nodes[1]);
    assert(natural>150.f&&natural<250.f);
    assert(nearCents(noteFrequency12TET(69),440.f,.01f));
    assert(nearCents(noteFrequency12TET(60),261.62557f,.01f));
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    assert(pitchMapCurrent(b));
    for(int n:{48,57,60,61,64,69}){
        assert(b.pitch.playable[n]);
        auto g=b.graph;
        assert(applyMappedPitch(b.pitch,g,n));
        assert(nearCents(physicalFundamental(g.nodes[1]),noteFrequency12TET(n),.3f));
        assert(g.nodes[1].form.lengthMeters==b.graph.nodes[1].form.lengthMeters);
        assert(g.nodes[1].material==b.graph.nodes[1].material);
        assert(g.nodes[0].driver==b.graph.nodes[0].driver);
        assert(g.links[0].modEnabled==b.graph.links[0].modEnabled);
        assert(g.nodes[1].movingEnabled==b.graph.nodes[1].movingEnabled);
    }
    assert(!b.pitch.playable[90]); // tension cannot achieve this high note
    b.graph.nodes[1].form.lengthMeters=.61f;
    assert(!pitchMapCurrent(b)); // edited geometry invalidates map
    b=filamentPatch();
    // A second physical pitch assignment can take over after tension hits
    // its max, instead of faking pitch shifts after the physics engine.
    b.pitch.count=2;
    b.pitch.assignments[0]={1,PitchProperty::Tension,40.f,130.f,2.f};
    b.pitch.assignments[1]={1,PitchProperty::Length,.25f,.65f,1.f};
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    for(int n:{60,64,69}){
        auto g=b.graph;
        assert(b.pitch.playable[n]);
        assert(applyMappedPitch(b.pitch,g,n));
        assert(nearCents(physicalFundamental(g.nodes[1]),noteFrequency12TET(n),1.f));
    }
    // A string can remain physically unchanged while a downstream
    // resonating chamber changes length to determine the played pitch.
    b=filamentPatch();
    const int chamber=addPhysicalRole(b.graph,PhysicalRole::Resonator);
    b.graph.nodes[chamber].form.continuum=Continuum::Gas;
    b.graph.nodes[chamber].material=Material::Air;
    b.graph.nodes[chamber].form.lengthMeters=.65f;
    b.graph.nodes[chamber].form.opening=.95f;
    b.graph.nodes[chamber].x=.7f;b.graph.nodes[chamber].y=.65f;
    // The chamber is genuinely IN the energy path, not an isolated object.
    assert(b.graph.disconnect(1,2));
    assert(b.graph.connect(1,chamber));
    assert(b.graph.connect(chamber,2));
    b.pitch.trackedNode=chamber;
    b.pitch.assignments[0]={static_cast<int8_t>(chamber),
        PitchProperty::Length,.10f,1.5f,1.f};
    b.pitch.count=1;
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    const auto originalString=b.graph.nodes[1];
    for(int n:{48,60,69,72}){
        assert(b.pitch.playable[n]);
        auto g=b.graph;
        assert(applyMappedPitch(b.pitch,g,n));
        assert(nearCents(physicalFundamental(g.nodes[chamber]),
            noteFrequency12TET(n),.4f));
        assert(g.nodes[1].form.lengthMeters==originalString.form.lengthMeters);
        assert(g.nodes[1].tensionNewtons==originalString.tensionNewtons);
        assert(g.nodes[1].form.widthMeters==originalString.form.widthMeters);
    }
    // A parameter that only changes loudness/interaction cannot falsely
    // calibrate the actual oscillator frequency of the tracked medium.
    b=filamentPatch();
    b.pitch.assignments[0]={1,PitchProperty::Coupling,.05f,.95f,1.f};
    assert(!buildPhysicalKeyboard(b.graph,b.pitch));
    // Source V remains an energy control even with a precomputed note map.
    b=filamentPatch();
    assert(buildPhysicalKeyboard(b.graph,b.pitch));
    MaterialTimeVoice voice;
    voice.prepare(48000.f);
    voice.start(b,60,0.f,0.f);
    double zero=0.;
    for(int i=0;i<30000;++i){const float x=voice.render();zero+=x*x;}
    assert(zero<1.e-14);
    voice.start(b,60,.75f,0.f);
    double energy=0.;
    for(int i=0;i<30000;++i){const float x=voice.render();energy+=x*x;}
    assert(energy>1.e-16);
    std::printf("PASS v6 physical pitch calibration: natural %.2f Hz C4 %.3fHz, bounded 12-TET, multi, air chamber, stale rejection, no pitch-shift.\n",
        natural,noteFrequency12TET(60));
}
