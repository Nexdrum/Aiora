#include "MaterialTimeVoice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
using namespace aiora::physical;
#ifdef AIORA_BEFORE_FIX
namespace aiora::physical {
// Example only: these are the same editable values exposed by the builder.
// No guitar-specific behavior is selected by the solver.
inline FlowGraph constructionGuitarGraph() noexcept {
    FlowGraph g=constructionEmptyGraph();
    g.spanMs=8.f;
    auto& energy=g.nodes[0];
    energy.sourceAcceleration=85.f;energy.sourcePotential=.85f;energy.sourceTimeMs=5.f;
    auto& pick=g.nodes[1];pick.x=.42f;pick.y=.20f;
    pick.form.lengthMeters=.03f;pick.form.widthMeters=.015f;pick.form.depthMeters=.0008f;
    pick.form.edges=3;pick.form.rounding=.2f;pick.form.fixing=.8f;pick.tensionNewtons=0.f;
    pick.substance.value={.75f,.7f,.95f,.75f,.30f,.32f,.18f,.65f,.25f,.25f,0.f,.32f,.12f};
    const int string=addPhysicalRole(g,PhysicalRole::Resonator);
    const int palm=addPhysicalRole(g,PhysicalRole::Resonator);
    const int output=addPhysicalRole(g,PhysicalRole::Output);
    auto& s=g.nodes[string];initializeConstruction(s);
    s.x=.43f;s.y=.43f;s.form.lengthMeters=.65f;s.form.widthMeters=.0005f;s.form.depthMeters=.0005f;
    s.form.edges=32;s.form.rounding=1.f;s.form.fixing=.8f;s.tensionNewtons=56.f;s.damping=.35f;
    s.substance.value={.933f,.93f,.99f,.908f,.100f,.05f,.0f,.9f,.2f,.2f,0.f,.15f,.12f};
    auto& h=g.nodes[palm];initializeConstruction(h);
    h.x=.13f;h.y=.72f;h.form.lengthMeters=.08f;h.form.widthMeters=.04f;h.form.depthMeters=.016f;
    h.form.rounding=.8f;h.form.fixing=.25f;h.tensionNewtons=0.f;h.damping=2.f;
    h.substance.value={.738f,.12f,.35f,.40f,.34f,.90f,.35f,.15f,.3f,.4f,.02f,.65f,.12f};
    g.nodes[output].x=.73f;g.nodes[output].y=.92f;
    g.connect(1,string);
    auto& strike=g.links[g.linkCount-1];strike.position=.21f;strike.fromPosition=.8f;
    strike.aperture=.85f;strike.engagement=1.f;strike.motion=.8f;strike.angleDeg=30.f;
    strike.adhesion=0.f;strike.friction=.55f;strike.elasticity=.55f;strike.restitution=.55f;
    g.connect(string,palm);
    auto& mute=g.links[g.linkCount-1];mute.position=0.f;mute.modEnabled=true;mute.modMin=0.f;mute.modMax=.5f;
    mute.fromPosition=.12f;mute.aperture=.8f;mute.engagement=1.f;mute.motion=0.f;
    mute.adhesion=0.f;mute.friction=.8f;mute.elasticity=.45f;mute.restitution=.05f;
    g.connect(string,output);
    auto& pickup=g.links[g.linkCount-1];pickup.position=.23f;pickup.aperture=.8f;
    return g;
}

}
#endif
struct Result {double first{},middle{},late{},peak{},energyAtEnd{},afterMute{};};
static Result run(const Blueprint& b,float initialM,bool gesture,bool sustained){
    MaterialTimeVoice v;v.prepare(48000.f);v.start(b,50,.8f,initialM);
    Result r{};
    for(int i=0;i<192000;++i){
        if(i==12000&&gesture)v.setExpression(.8f,1.f);
        if(i==(sustained?48000:12000))v.release();
        const float s=v.render();assert(std::isfinite(s));
        r.peak=std::max(r.peak,double(std::fabs(s)));
        const double sq=double(s)*s;
        if(i>=2400&&i<12000)r.first+=sq;
        if(i>=12000&&i<24000)r.afterMute+=sq;
        if(i>=48000&&i<96000)r.middle+=sq;
        if(i>=144000)r.late+=sq;
    }
    r.energyAtEnd=v.storedModalEnergy();
    return r;
}
static void print(const char* label,const Result& r){
    std::printf("%s first %.9g post-M %.9g middle %.9g late %.9g peak %.9g stored %.9g\n",
      label,r.first,r.afterMute,r.middle,r.late,r.peak,r.energyAtEnd);
}
int main(){
    Blueprint b;b.graph=constructionGuitarGraph();
    assert(b.graph.nodeCount==5&&b.graph.linkCount==4);
    const auto open=run(b,0.f,false,false);
    const auto muted=run(b,1.f,false,false);
    const auto gesture=run(b,0.f,true,false);
    print("guitar open",open);print("guitar muted",muted);print("guitar M after pluck",gesture);
    Blueprint starter;starter.graph=constructionStarterGraph();
    const auto defaultRun=run(starter,0.f,false,false);print("starter",defaultRun);
    auto& contact=starter.graph.links[1];contact.adhesion=1.f;contact.motion=0.f;contact.engagement=1.f;contact.aperture=1.f;contact.elasticity=1.f;contact.restitution=1.f;
    starter.graph.nodes[2].damping=.15f;
    starter.graph.nodes[2].substance.value[InternalLoss]=0.f;
    starter.graph.nodes[2].substance.value[ElasticRecovery]=1.f;
    const auto hard=run(starter,0.f,false,false);print("high coupling finite",hard);
    starter.graph.nodes[0].sourceFollowNote=true;
    const auto held=run(starter,0.f,false,true);print("high coupling held/released",held);
#ifndef AIORA_BEFORE_FIX
    assert(open.first>1.e-12);
    assert(muted.first<open.first);
    assert(gesture.afterMute<open.afterMute);
    assert(open.late<open.first*.01+1.e-12);
    assert(defaultRun.late<defaultRun.first*.01+1.e-12);
    assert(hard.late<hard.first*.01+1.e-12);
    assert(held.late<held.first*.05+1.e-10);
    assert(open.energyAtEnd<1.e-8&&hard.energyAtEnd<1.e-8);
#endif
}
