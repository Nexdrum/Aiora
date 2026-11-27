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
    Blueprint b;b.graph=constructionStarterGraph();
    auto& n=b.graph.nodes[2];
    assert(n.substance.enabled&&b.graph.nodes[1].substance.enabled);
    const auto before=objectMaterial(n);
    const auto f=physicalFundamental(n);
    const double e=render(b);assert(e>1.e-20);
    const auto identityStamp=physicalGraphStamp(b.graph);
    n.material=Material::Gold;
    n.form.continuum=Continuum::Gas;
    assert(physicalGraphStamp(b.graph)==identityStamp);
    assert(objectMaterial(n).densityKgM3==before.densityKgM3);
    assert(physicalFundamental(n)==f&&render(b)==e);
    const auto stamp=physicalGraphStamp(b.graph);
    n.substance.value[Density]=.95f;
    assert(physicalGraphStamp(b.graph)!=stamp);
    assert(objectMaterial(n).densityKgM3>before.densityKgM3);
    assert(physicalFundamental(n)<=f);
    n.substance.value[ShearStiffness]=.35f;
    n.substance.value[Compressibility]=.2f;
    assert(!objectIsFluid(n)); // a compliant near-incompressible solid
    n.substance.value[ShearStiffness]=0.f;
    assert(objectIsFluid(n));
    assert(constructedShear(n.substance)==0.f);
    assert(std::isfinite(render(b)));
    for(int i=0;i<MaterialControlCount;++i){
        n.substance.value[i]=0.f;assert(std::isfinite(physicalFundamental(n)));
        n.substance.value[i]=1.f;assert(std::isfinite(physicalFundamental(n)));
        n.substance.value[i]=.5f;
    }
    assert(std::isfinite(render(b)));
    std::printf("PASS v12 construction material identity and finite bounds; energy %.9g\n",e);
}
