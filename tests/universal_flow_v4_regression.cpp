// Aiora v4 universal physical graph regression, including the dual wind
// hybrid and the distinction between source energy and physical M movement.
#include "MaterialTimeVoice.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;

struct Stats {double early{},tail{},high{},delta{},energy{},max{};int onset{-1};bool aliveAfterRelease{};};
Stats listen(FlowGraph graph,float V,float M,float seconds=.9f,bool release=false){
    Blueprint bp;bp.graph=graph;
    MaterialTimeVoice voice;
    voice.prepare(48000);
    voice.start(bp,57,V,M);
    Stats r{};
    float previous=0.f;
    const int frames=static_cast<int>(seconds*48000.f);
    for(int i=0;i<frames;++i){
        if(release&&i==int(.22f*48000.f)){voice.setExpression(0.f,M);voice.release();}
        const float x=voice.render();
        assert(std::isfinite(x)&&std::fabs(x)<=1.0001f);
        if(r.onset<0&&std::fabs(x)>2e-6f)r.onset=i;
        if(i<.20f*48000.f)r.early+=x*x;
        if(i>.32f*48000.f)r.tail+=x*x;
        r.energy+=x*x;
        r.high+=(x-previous)*(x-previous);
        r.max=std::max(r.max,double(std::fabs(x)));
        previous=x;
    }
    r.aliveAfterRelease=voice.active();
    return r;
}
int main(){
    FlowGraph g=templateFlowGraph(FlowTemplate::Plate);
    const int plate=g.first(FlowKind::Plate);
    const int source=g.first(FlowKind::Exciter);
    const int out=g.first(FlowKind::Output);
    assert(plate>=0&&source>=0&&out>=0);
    const auto silent=listen(g,0.f,0.f);
    const auto soft=listen(g,.10f,0.f);
    const auto strong=listen(g,.90f,0.f);
    assert(silent.energy<1e-13);
    assert(soft.energy>0);
    assert(strong.energy>soft.energy*1.1);
    // M is not a direct timbre/gain modulation knob. With nothing assigned
    // to move, an M gesture must NOT alter the instrument.
    const auto m0=listen(g,.8f,0.f);
    const auto m1=listen(g,.8f,1.f);
    assert(std::fabs(m0.energy-m1.energy)<1e-9);
    // X belongs to the incoming connection; M moves this entry position.
    const int sourceLink=0;
    assert(g.links[sourceLink].from==source&&g.links[sourceLink].to==plate);
    g.links[sourceLink].position=.08f;
    g.links[sourceLink].modTravel=.70f;
    const auto entryStart=listen(g,.8f,0.f);
    const auto entryMoved=listen(g,.8f,1.f);
    assert(std::fabs(entryMoved.energy-entryStart.energy)>
        .008*std::max(entryMoved.energy,entryStart.energy));
    // Y controls FIRST arrival; changing Y does not change stored geometry.
    g=templateFlowGraph(FlowTemplate::Plate);
    g.spanMs=5.f;
    const auto fast=listen(g,.8f,0.f);
    g.spanMs=78.f;
    const auto late=listen(g,.8f,0.f);
    assert(late.onset>fast.onset+400);
    // Breath can simultaneously excite a reed and a jet edge, then enter
    // one shared air column at distinct spatial positions.
    g=templateFlowGraph(FlowTemplate::HybridWind);
    const int router=g.first(FlowKind::Router);
    const int reed=g.first(FlowKind::Reed);
    const int edge=g.first(FlowKind::Edge);
    const int bore=g.first(FlowKind::AirColumn);
    assert(router>=0&&reed>=0&&edge>=0&&bore>=0);
    assert(g.linked(source,router)&&g.linked(router,reed)&&
           g.linked(router,edge)&&g.linked(reed,bore)&&
           g.linked(edge,bore)&&g.linked(bore,out));
    float reedEntry=-1,edgeEntry=-1;
    for(int j=0;j<g.linkCount;++j){
        const auto& link=g.links[j];
        if(link.from==reed&&link.to==bore)reedEntry=link.position;
        if(link.from==edge&&link.to==bore)edgeEntry=link.position;
    }
    assert(reedEntry>=0&&edgeEntry>=0&&
           std::fabs(reedEntry-edgeEntry)>.25f);
    const auto reedRoute=listen(g,.85f,0.f);
    const auto edgeRoute=listen(g,.85f,1.f);
    assert(reedRoute.energy>1e-11&&edgeRoute.energy>1e-11);
    assert(std::fabs(reedRoute.high-edgeRoute.high)>
        .01*std::max(reedRoute.high,edgeRoute.high));
    // Turning breath energy OFF doesn't erase pressure waves or resonances.
    const auto releaseTail=listen(g,.85f,.5f,.9f,true);
    assert(releaseTail.tail>1e-10);
    // The source cannot be left on while V is zero.
    const auto windNoV=listen(g,0.f,.5f);
    assert(windNoV.energy<1e-11);
    // Positive link delays and no backwards time travel.
    assert(!g.compatible(bore,reed));
    std::printf("PASS universal flow v4: V=0 %.8g, V soft/strong %.6g/%.6g, X movement %.6g/%.6g, late %.0f samples, hybrid reed/edge %.6g/%.6g, tail %.8g\n",
      silent.energy,soft.energy,strong.energy,entryStart.energy,
      entryMoved.energy,double(late.onset-fast.onset),
      reedRoute.high,edgeRoute.high,releaseTail.tail);
}
