#include "PhysicalInstrumentVoice.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace aiora::physical;

struct Result { double energy{},late{},peak{}; };
Result audition(const Blueprint& b,int midi=42,float duration=2.0f){
    constexpr int rate=48000;
    PhysicalInstrumentVoice v;v.prepare(rate);v.start(b,midi,.8f,.0f);
    Result result;
    const int n=static_cast<int>(duration*rate);
    for(int i=0;i<n;++i){
        const float x=v.render();
        assert(std::isfinite(x));
        assert(std::fabs(x)<=3.0f);
        result.energy+=x*x;
        if(i>=n/2)result.late+=x*x;
        result.peak=std::max(result.peak,static_cast<double>(std::fabs(x)));
    }
    return result;
}
int main(){
    static_assert(kMaterialCount>=28);
    for(int i=0;i<kMaterialCount;++i){
        const auto m=materialProperties(static_cast<Material>(i));
        assert(m.densityKgM3>0&&m.youngModulusPa>0&&m.lossFactor>0);
        assert(std::isfinite(m.youngModulusPa));
    }
    FlowGraph g=defaultFlowGraph();
    assert(g.nodeCount==6&&g.linkCount==6);
    assert(!g.compatible(0,3));
    assert(!g.connect(0,3));
    assert(!g.connect(0,1));
    assert(g.linked(0,1)&&g.linked(2,3)&&g.linked(3,5));
    const int body2=g.addNode(FlowKind::Body);
    assert(body2>=0&&g.linked(2,body2)&&g.linked(body2,5));
    const int mute2=g.addNode(FlowKind::Damper);
    assert(mute2>=0&&g.linked(mute2,1));
    assert(g.removeNode(body2));
    assert(g.nodeCount==7&&g.linkCount==7);
    assert(g.removeNode(6));
    assert(g.nodeCount==6&&g.linkCount==6);
    assert(!g.removeNode(0)); // mandatory primary exciter
    Blueprint b;
    b.graph=defaultFlowGraph();
    const auto original=audition(b);
    assert(original.energy>1.e-8&&original.peak>1.e-6);
    // Remove the body output edge: mode sound must not contribute.
    assert(b.graph.disconnect(3,5));
    const auto noBody=audition(b);
    assert(std::fabs(original.energy-noBody.energy)>original.energy*.015);
    // Removing the only excitation path creates silence.
    assert(b.graph.disconnect(0,1));
    const auto silent=audition(b);
    assert(silent.energy<1.e-12);
    b.graph=defaultFlowGraph();
    b.graph.nodes[3].thicknessMm=12.0f;
    const auto thick=audition(b);
    assert(std::fabs(original.energy-thick.energy)>original.energy*.01);
    b.graph.nodes[3].thicknessMm=3.f;
    b.graph.nodes[3].damping=4.f;
    const auto damped=audition(b);
    assert(std::fabs(original.late-damped.late)>original.late*.01);
    // Four connected body branches must not multiply the output by four.
    b.graph=defaultFlowGraph();
    b.graph.addNode(FlowKind::Body);
    b.graph.addNode(FlowKind::Body);
    b.graph.addNode(FlowKind::Body);
    const auto fourBodies=audition(b);
    assert(fourBodies.peak<original.peak*4.f+.005);
    // Matched resonances should remain finite and not experience extreme
    // Q-dependent gain growth when changing materials.
    b.graph=defaultFlowGraph();
    for(Material m:{Material::Steel,Material::Bronze,Material::Glass,
                    Material::Wood,Material::Hardwood,Material::Ceramic}){
        b.graph.nodes[3].material=m;
        for(float thickness:{.5f,3.f,18.f}){
            b.graph.nodes[3].thicknessMm=thickness;
            for(float damping:{.2f,1.f,6.f}){
                b.graph.nodes[3].damping=damping;
                for(int note=36;note<=84;note+=12){
                    const auto r=audition(b,note,0.25f);
                    assert(std::isfinite(r.energy)&&r.peak<2.95);
                }
            }
        }
    }
    std::printf("PASS flow v2: materials=%d open=%g no-body=%g high-damping=%g branches=%g\n",
        kMaterialCount,original.energy,noBody.energy,damped.energy,fourBodies.energy);
}
