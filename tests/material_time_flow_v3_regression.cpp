#include "MaterialTimeVoice.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;

struct Result {double energy{},edgeEnergy{},lateEnergy{},peak{};int onset{-1};};
Result render(Blueprint b,int midi=57,float m=0.f,float duration=1.2f){
    MaterialTimeVoice v;
    v.prepare(48000.f);
    v.start(b,midi,.85f,m);
    Result r;
    float old=0.f;
    const int frames=static_cast<int>(duration*48000.f);
    for(int i=0;i<frames;++i){
        if(i==frames/2)v.release();
        const float x=v.render();
        assert(std::isfinite(x)&&std::fabs(x)<=1.01f);
        r.peak=std::max(r.peak,static_cast<double>(std::fabs(x)));
        r.energy+=x*x;
        r.edgeEnergy+=(x-old)*(x-old);
        if(i>=frames/2)r.lateEnergy+=x*x;
        if(r.onset<0&&std::fabs(x)>0.000002f)r.onset=i;
        old=x;
    }
    return r;
}
int main(){
    static_assert(kMaterialCount==30);
    FlowGraph empty=blankFlowGraph();
    assert(empty.nodeCount==2&&empty.linkCount==0);
    Blueprint patch;
    patch.graph=empty;
    const auto blank=render(patch);
    assert(blank.energy<1e-14);
    for(FlowTemplate t:{FlowTemplate::String,FlowTemplate::Drum,FlowTemplate::Plate,
            FlowTemplate::Bar,FlowTemplate::Air,FlowTemplate::Bell}){
        patch.graph=templateFlowGraph(t);
        assert(patch.graph.first(FlowKind::Exciter)>=0);
        assert(patch.graph.first(FlowKind::Output)>=0);
        const auto out=render(patch);
        assert(out.energy>1e-13);
        assert(out.onset>=0);
        std::printf("template %d energy %.8g onset %d\n",
                     static_cast<int>(t),out.energy,out.onset);
    }
    patch.graph=templateFlowGraph(FlowTemplate::Plate);
    assert(patch.graph.first(FlowKind::String)<0);
    assert(patch.graph.first(FlowKind::Bridge)<0);
    // Any vibrating object may be directly excited. No bridge required.
    const int out=patch.graph.first(FlowKind::Output);
    const int plate=patch.graph.first(FlowKind::Plate);
    assert(out>=0&&plate>=0);
    assert(!patch.graph.compatible(out,plate));
    assert(!patch.graph.connect(out,plate));
    assert(patch.graph.linked(plate,out));
    const auto metal=render(patch);
    patch.graph.nodes[plate].material=Material::Rubber;
    const auto rubber=render(patch);
    assert(std::fabs(metal.energy-rubber.energy)>
        .04*std::max(metal.energy,rubber.energy));
    patch.graph.nodes[plate].material=Material::Glass;
    const auto glass=render(patch);
    assert(std::fabs(metal.energy-glass.energy)>
        .02*std::max(metal.energy,glass.energy));
    patch.graph.nodes[plate].material=Material::Steel;
    patch.graph.spanMs=5.f;
    const auto quick=render(patch);
    patch.graph.spanMs=80.f;
    const auto slow=render(patch);
    assert(slow.onset>quick.onset+700); // audible chronological propagation
    assert(patch.graph.moveNode(plate,.7f,.60f));
    assert(patch.graph.nodes[plate].y>.35f);
    assert(patch.graph.linked(plate,out));
    // Contact in the pathway must absorb more vibration as M increases.
    patch.graph=templateFlowGraph(FlowTemplate::Plate);
    const int b=patch.graph.first(FlowKind::Body);
    assert(patch.graph.disconnect(plate,out));
    assert(patch.graph.disconnect(plate,b));
    assert(patch.graph.disconnect(b,out));
    const int contact=patch.graph.addNode(FlowKind::Damper);
    assert(contact>=0);
    patch.graph.nodes[contact].y=.66f;
    assert(patch.graph.connect(plate,contact));
    assert(patch.graph.connect(contact,out));
    const auto dry=render(patch,57,0.f);
    const auto muted=render(patch,57,1.f);
    assert(muted.energy<dry.energy*.85);
    // Four distinct geometries and all material choices must stay bounded.
    patch.graph=templateFlowGraph(FlowTemplate::Bell);
    const int body=patch.graph.first(FlowKind::Body);
    assert(body>=0);
    for(int material=0;material<kMaterialCount;++material){
        auto& n=patch.graph.nodes[body];
        n.material=static_cast<Material>(material);
        n.thicknessMm=(material%2)?18.f:.35f;
        n.damping=(material%3)?0.15f:6.f;
        for(int midi:{36,57,84}){
            const auto r=render(patch,midi,0.f,.38f);
            assert(std::isfinite(r.energy)&&r.peak<=1.01);
        }
    }
    // Arbitrary cascades of different vibrating objects are permitted.
    patch.graph=blankFlowGraph();
    const int m=patch.graph.addNode(FlowKind::Membrane);
    const int bar=patch.graph.addNode(FlowKind::Bar);
    patch.graph.nodes[m].y=.25f;
    patch.graph.nodes[bar].y=.58f;
    assert(patch.graph.connect(0,m));
    assert(patch.graph.connect(m,bar));
    assert(patch.graph.connect(bar,1));
    const auto hybrid=render(patch);
    assert(hybrid.energy>1e-15);
    std::printf("PASS material-time flow: materials=%d; steel=%.6g, rubber=%.6g, glass=%.6g, fast onset=%d, slow onset=%d, contact=%.6g/%.6g\n",
        kMaterialCount,metal.energy,rubber.energy,glass.energy,quick.onset,slow.onset,dry.energy,muted.energy);
}
