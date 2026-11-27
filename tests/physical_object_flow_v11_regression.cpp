#include "MaterialTimeVoice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;
static double render(const Blueprint& b){
    MaterialTimeVoice v;v.prepare(48000.f);v.start(b,60,.8f);
    double energy=0.;
    for(int i=0;i<24000;++i){
        float s=v.render();assert(std::isfinite(s)&&std::fabs(s)<=1.001f);
        energy+=double(s)*s;
    }
    return energy;
}
int main(){
    Blueprint b;b.graph=objectFlowStarterGraph();
    auto& g=b.graph;
    assert(g.objectFlow&&g.nodeCount==4&&g.linkCount==3);
    for(int i=0;i<g.nodeCount;++i)assert(g.nodes[i].role!=PhysicalRole::Interaction);
    assert(!g.compatible(0,2)&&!g.compatible(0,3));
    assert(!g.removeNode(0)&&!g.removeNode(1)&&!g.disconnect(0,1));
    const double normal=render(b);assert(normal>1.e-20);
    const auto stamp=physicalGraphStamp(g);
    g.links[1].engagement=0.f;
    assert(physicalGraphStamp(g)!=stamp&&render(b)<normal*1.e-4);
    g.links[1].engagement=.8f;g.links[1].motion=0.f;g.links[1].adhesion=1.f;
    const double attached=render(b);assert(attached>1.e-20);
    assert(std::fabs(attached-normal)>1.e-8*std::max(attached,normal));
    g.nodes[1].material=Material::Water;g.nodes[1].form.continuum=Continuum::Liquid;
    assert(physicalFundamental(g.nodes[1])>0.f);
    assert(render(b)>1.e-20);
    std::printf("PASS object flow v11: contact %.9g coupling %.9g\n",normal,attached);
}
