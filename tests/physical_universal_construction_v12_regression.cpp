#include "MaterialTimeVoice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;
static double render(const Blueprint& b){
    MaterialTimeVoice v;v.prepare(48000.f);v.start(b,60,.8f);
    double sum=0.;
    for(int i=0;i<30000;++i){const float s=v.render();assert(std::isfinite(s)&&std::fabs(s)<=1.001f);sum+=double(s)*s;}
    return sum;
}
int main(){
    Blueprint b;b.graph=constructionStarterGraph();
    auto& n=b.graph.nodes[2];auto& g=n.form;
    const double a=render(b);assert(a>1.e-20);
    assert(usesConstructedMaterial(b.graph));
    assert(!usesConstructedMaterial(objectFlowStarterGraph()));
    const double calibratedRms=std::sqrt(a/30000.)*kConstructionOutputGain;
    assert(calibratedRms>.005&&calibratedRms<.5);
    std::printf("Default dry preview calibrated RMS %.8g\n",calibratedRms);
    const float full=occupiedVolume(n);
    const auto stamp=physicalGraphStamp(b.graph);
    g.wallMm=1.f;g.wallFill=0.f;g.cavities[0].length=.6f;
    assert(occupiedVolume(n)<full&&physicalGraphStamp(b.graph)!=stamp);
    const float closed=cavityHz(g,0);assert(closed>0.f);
    g.openings[0].size=.4f;g.openings[0].position=.5f;
    const float opened=cavityHz(g,0);assert(opened>0.f&&opened!=closed);
    assert(std::isfinite(render(b)));
    g.edges=3;g.rounding=0.f;const float triangle=sectionFactor(g);
    g.edges=32;assert(sectionFactor(g)!=triangle);
    b.graph.links[2].aperture=0.f;assert(render(b)==0.);
    b.graph.links[2].aperture=.22f;
    auto& filament=b.graph.nodes[1];
    filament.tensionNewtons=0.f;
    assert(std::isfinite(physicalFundamental(filament))&&physicalFundamental(filament)>0.f);
    auto& t=b.graph.nodes[1];t.substance.value[ShearStiffness]=0.f;
    t.substance.value[Density]=.087f;t.substance.value[Compressibility]=.856f;
    assert(objectIsFluid(t));
    const float confined=physicalFundamental(t);
    t.form.confinement=0.f;t.form.fillLevel=.8f;t.form.spread=.8f;
    assert(physicalFundamental(t)!=confined&&std::isfinite(render(b)));
    b.graph.links[1].engagement=0.f;assert(render(b)<a*1.e-4);
    std::printf("PASS integrated geometry, cavity, opening and free-fluid bounds; %.8g\n",a);
}
