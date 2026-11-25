// Universal material-role v5 regression. Material / geometry must affect
// sound without selecting predefined guitar, string, reed or drum instruments.
#include "MaterialTimeVoice.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;
struct Measure {double energy{},gradient{},tail{},peak{}; int first{-1};};
static Measure render(const FlowGraph& g,float v=.9f,float m=.0f,
                      bool stopSource=false){
    Blueprint b;b.graph=g;
    MaterialTimeVoice voice;voice.prepare(48000.f);
    voice.start(b,60,v,m);
    Measure stats{};
    float prev=0.f;
    for(int i=0;i<48000;++i){
        if(stopSource&&i==10000){voice.setExpression(0.f,m);voice.release();}
        float x=voice.render();
        assert(std::isfinite(x)&&std::fabs(x)<=1.00001f);
        if(stats.first<0&&std::fabs(x)>1.e-9f)stats.first=i;
        stats.energy+=double(x)*x;
        stats.gradient+=double(x-prev)*(x-prev);
        if(i>18000)stats.tail+=double(x)*x;
        stats.peak=std::max(stats.peak,double(std::fabs(x)));
        prev=x;
    }
    return stats;
}
static bool different(const Measure& a,const Measure& b){
    return std::fabs(a.gradient-b.gradient)>
        0.005*std::max(a.gradient,b.gradient) ||
        std::fabs(a.energy-b.energy)>
        0.005*std::max(a.energy,b.energy);
}
int main(){
    FlowGraph g=universalStarterGraph();
    assert(g.nodeCount==5&&g.linkCount==4);
    assert(isUniversalSource(g.nodes[0]));
    assert(g.nodes[1].role==PhysicalRole::Transfer);
    assert(g.nodes[2].role==PhysicalRole::Interaction);
    assert(g.nodes[3].role==PhysicalRole::Resonator);
    assert(isUniversalOutput(g.nodes[4]));
    for(int i=0;i<g.nodeCount;++i)
        assert(g.nodes[i].kind==FlowKind::Universal);
    assert(!g.compatible(4,2)&&!g.compatible(2,0));
    const int other=addPhysicalRole(g,PhysicalRole::Transfer);
    assert(other>=0&&g.count(FlowKind::Universal)==6);
    g.nodes[other].y=.16f;
    assert(g.connect(0,other));
    assert(g.connect(other,2));
    assert(g.nodes[other].role==PhysicalRole::Transfer);
    // User can start from a SINGLE human-powered source and place any roles.
    FlowGraph empty=universalEmptyGraph();
    assert(empty.nodeCount==1&&isUniversalSource(empty.nodes[0]));
    assert(addPhysicalRole(empty,PhysicalRole::Source)>=0);
    assert(addPhysicalRole(empty,PhysicalRole::Output)>=0);
    // A starter graph should sound without a named instrument generator.
    g=universalStarterGraph();
    const auto baseline=render(g);
    const auto silence=render(g,0.f);
    assert(baseline.energy>1.e-18&&silence.energy<1.e-15);
    const auto quiet=render(g,.08f);
    assert(baseline.energy>quiet.energy*1.2);
    // M without assignments must not change the output, even with V constant.
    assert(std::fabs(baseline.energy-render(g,.9f,1.f).energy)<1.e-11);
    // Geometry is real DSP input, not just a label / cosmetic shape.
    g.nodes[3].form.lengthMeters=.18f;
    const auto shortBody=render(g);
    assert(different(baseline,shortBody));
    g=universalStarterGraph();
    g.nodes[3].material=Material::Rubber;
    const auto rubber=render(g);
    assert(different(baseline,rubber));
    g=universalStarterGraph();
    g.nodes[3].form.stations={.3f,.55f,2.5f,1.9f};
    const auto profile=render(g);
    assert(different(baseline,profile));
    g=universalStarterGraph();
    g.nodes[3].form.curvature=.8f;
    g.nodes[3].form.taper=-.8f;
    const auto distorted=render(g);
    assert(different(baseline,distorted));
    // Independent M assignments on multiple connections; reverse ranges
    // must be legal. M controls position only, not source energy.
    g=universalStarterGraph();
    g.links[0].modEnabled=true;
    g.links[0].modMin=.06f;
    g.links[0].modMax=.94f;
    g.links[2].modEnabled=true;
    g.links[2].modMin=.91f;
    g.links[2].modMax=.09f;
    const auto left=render(g,.9f,0.f);
    const auto right=render(g,.9f,1.f);
    assert(different(left,right));
    // A physical object can also translate along its own assigned M range.
    g=universalStarterGraph();
    g.nodes[1].movingEnabled=true;
    g.nodes[1].movingMin=.10f;
    g.nodes[1].movingMax=.95f;
    assert(different(render(g,.9f,0.f),render(g,.9f,1.f)));
    // Source motion, not the bow/pick, decides how energy is introduced.
    g=universalStarterGraph();
    g.nodes[0].driver=Driver::Breath;
    g.nodes[0].gesture=Gesture::Continuous;
    const auto breath=render(g,.6f,.0f,true);
    assert(breath.energy>1.e-14&&breath.tail>0.);
    // The first-arrival Y coordinate delays onset but does not halt ringing.
    g=universalStarterGraph();
    g.spanMs=3.f;
    const auto early=render(g);
    g.spanMs=78.f;
    const auto late=render(g);
    assert(early.first>=0&&late.first>early.first+200);
    std::printf("PASS universal material roles v5: baseline %.9g material %.9g geometry %.9g profile %.9g M %.9g/%.9g tail %.9g\n",
        baseline.energy,rubber.energy,shortBody.energy,profile.energy,
        left.energy,right.energy,breath.tail);
}
