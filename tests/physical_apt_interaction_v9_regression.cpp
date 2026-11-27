// Aiora v9: universal Acceleration / Potential / Time energy source and
// bidirectional contact response. No instrument-specific source presets.
#include "MaterialTimeVoice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;

static Blueprint setup(bool locked=false,ContactLaw law=ContactLaw::Friction){
    Blueprint b;
    b.graph=universalStarterGraph();
    auto& src=b.graph.nodes[0];
    src.sourceAcceleration=42.f;
    src.sourcePotential=.80f;
    src.sourceTimeMs=90.f;
    src.sourceFollowNote=locked;
    auto& interaction=b.graph.nodes[2];
    interaction.law=law;
    interaction.interactionAngleDeg=72.f;
    interaction.interactionEngagement=.78f;
    return b;
}
static double energy(const Blueprint& b,int frames=20000){
    MaterialTimeVoice v;v.prepare(48000.f);
    v.start(b,60,.8f,0.f);
    double e=0.;
    for(int i=0;i<frames;++i){
        const float s=v.render();
        assert(std::isfinite(s)&&std::fabs(s)<=1.001f);
        e+=double(s)*s;
    }
    return e;
}
int main(){
    // Geometric transfer mass is material-density-dependent; A is applied
    // to THIS mass (F=m*A), not used as an arbitrary audio-envelope slope.
    auto b=setup();
    MaterialTimeVoice a;a.prepare(48000.f);a.start(b,60,.8f);
    const float nylonMass=a.sourceDrivenMass();
    assert(nylonMass>1.e-6f&&nylonMass<.1f);
    b.graph.nodes[1].material=Material::Steel;
    MaterialTimeVoice steel;steel.prepare(48000.f);steel.start(b,60,.8f);
    assert(steel.sourceDrivenMass()>nylonMass*4.f);

    // Finite Potential limits total delivered work; Time is a cutoff for
    // further driving, not a note-envelope or resonator-state reset.
    b=setup(false);
    b.graph.nodes[0].sourceTimeMs=4.f;
    a.start(b,60,.8f);
    for(int i=0;i<250;++i)(void)a.render();
    assert(a.sourceMechanicalWork()>0.f);
    assert(a.sourceMechanicalWork()<=.000022f*.8f*.8f*1.10f);
    const float moving=a.sourceMotionVelocity();
    assert(moving>=0.f);
    for(int i=0;i<10000;++i)(void)a.render();
    assert(a.sourceMotionVelocity()<moving*.2f+1.e-5f);

    // Manual time is independent of MIDI note-off. Two physically equal
    // strokes remain equal when one note is released before the deadline.
    b=setup(false);
    MaterialTimeVoice released,unreleased;
    released.prepare(48000.f);unreleased.prepare(48000.f);
    released.start(b,60,.8f);unreleased.start(b,60,.8f);
    for(int i=0;i<2500;++i){
        if(i==200)released.release();
        (void)released.render();(void)unreleased.render();
    }
    assert(std::fabs(released.sourceMotionVelocity()-
                     unreleased.sourceMotionVelocity())<1.e-6f);

    // Note-length lock keeps applying APT after manual Time would end,
    // then note-off stops new energy without clearing resonating objects.
    b=setup(true);
    b.graph.nodes[0].sourceTimeMs=.4f;
    a.start(b,60,.8f);
    for(int i=0;i<12000;++i)(void)a.render();
    const float heldSpeed=a.sourceMotionVelocity();
    assert(heldSpeed>.001f);
    const float ringing=a.storedModalEnergy();
    a.release();
    assert(a.storedModalEnergy()==ringing);
    for(int i=0;i<6000;++i)(void)a.render();
    assert(std::fabs(a.sourceMotionVelocity())<heldSpeed*.3f+1.e-4f);

    // V changes the potential continuously on a sounding held note. It
    // must not restart its mass, resonance phase or note-on acceleration.
    b=setup(true);
    MaterialTimeVoice low,high;
    low.prepare(48000.f);high.prepare(48000.f);
    low.start(b,60,.42f);high.start(b,60,.42f);
    for(int i=0;i<3000;++i){(void)low.render();(void)high.render();}
    const float sameStart=low.sourceMotionVelocity();
    assert(sameStart==high.sourceMotionVelocity());
    low.setExpression(.08f,0.f);high.setExpression(1.f,0.f);
    for(int i=0;i<8000;++i){(void)low.render();(void)high.render();}
    assert(high.sourceMotionVelocity()>low.sourceMotionVelocity()+.0001f);

    // Material-contact methods and relative angle alter outgoing waves,
    // not the saved APT settings. Input and resonator remain the same.
    b=setup(true,ContactLaw::Friction);
    const double friction=energy(b);
    b.graph.nodes[2].law=ContactLaw::Contact;
    b.graph.nodes[2].interactionAngleDeg=0.f;
    const double contact=energy(b);
    b.graph.nodes[2].law=ContactLaw::Flow;
    const double flow=energy(b);
    assert(friction>1.e-19&&contact>1.e-19&&flow>1.e-19);
    assert(std::fabs(friction-contact)>1.e-4*std::max(friction,contact));
    assert(std::fabs(contact-flow)>1.e-4*std::max(contact,flow));

    // Contact deformation is voice-private, continuous and nonzero.
    b=setup(true,ContactLaw::Friction);
    a.start(b,60,.8f);
    for(int i=0;i<10000;++i)(void)a.render();
    assert(std::fabs(a.interactionElasticState(2))>1.e-10f);
    // Physics-affecting controls invalidate precomputed calibration keys.
    const auto stamp=physicalGraphStamp(b.graph);
    b.graph.nodes[0].sourceAcceleration+=5.f;
    assert(physicalGraphStamp(b.graph)!=stamp);
    const auto updated=physicalGraphStamp(b.graph);
    b.graph.nodes[2].interactionAngleDeg-=20.f;
    assert(physicalGraphStamp(b.graph)!=updated);

    std::printf("PASS APT v9: mass %.6g kg, held v %.6g m/s, interaction energies %.6g / %.6g / %.6g\n",
                nylonMass,heldSpeed,friction,contact,flow);
}
