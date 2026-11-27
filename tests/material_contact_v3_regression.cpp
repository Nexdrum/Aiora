#include "MaterialTimeVoice.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace aiora::physical;

struct Response {double early{},after{},later{},peak{};};
Response measure(const FlowGraph& g,float beginningM,float laterM,float transitionS=0.15f){
    MaterialTimeVoice v;
    v.prepare(48000.f);
    Blueprint patch;
    patch.graph=g;
    v.start(patch,57,.85f,beginningM);
    Response out;
    constexpr int sr=48000;
    for(int i=0;i<sr;++i){
        if(i==static_cast<int>(transitionS*sr))v.setExpression(.85f,laterM);
        const float x=v.render();
        assert(std::isfinite(x)&&std::fabs(x)<=1.00001f);
        if(i<sr*.13f)out.early+=x*x;
        if(i>=sr*.26f&&i<sr*.58f)out.after+=x*x;
        if(i>=sr*.58f)out.later+=x*x;
        out.peak=std::max(out.peak,static_cast<double>(std::fabs(x)));
    }
    return out;
}
int main(){
    // User-reported case: a string template has TWO outgoing paths
    // (direct and resonator). Both must respond to a physical touch.
    FlowGraph g=templateFlowGraph(FlowTemplate::String);
    const int s=g.first(FlowKind::String);
    const int body=g.first(FlowKind::Body);
    const int out=g.first(FlowKind::Output);
    assert(s>=0&&body>=0&&out>=0);
    assert(g.linked(s,out)&&g.linked(s,body));
    const int contact=g.addNode(FlowKind::Damper);
    assert(contact>=0);
    g.nodes[contact].y=.54f;
    g.nodes[contact].material=Material::Felt;
    g.nodes[contact].coupling=1.f;
    g.nodes[contact].position=.30f;
    assert(g.connect(s,contact));
    // Contact only has a side attachment, no audio output path.
    const auto noContact=measure(g,0.f,0.f);
    const auto applyMidNote=measure(g,0.f,1.f);
    assert(noContact.early>1.e-12);
    assert(applyMidNote.early>1.e-12);
    // Early output is identical before the M gesture.
    assert(std::fabs(applyMidNote.early-noContact.early)<
        std::max(1.e-10,noContact.early*.0001));
    // M must drain energy stored in the string, and this should be audible
    // even if there is an existing direct String -> Output bypass.
    assert(applyMidNote.after < noContact.after*.85);
    // A stationary pressure knob works even without an M curve.
    g.nodes[contact].restPressure=.80f;
    const auto resting=measure(g,0.f,0.f);
    assert(resting.after<noContact.after*.85);
    // Explicit user's five-stage chain works as well, with no bypasses.
    g.nodes[contact].restPressure=0.f;
    assert(g.disconnect(s,out));
    assert(g.disconnect(s,body));
    assert(g.connect(contact,body));
    assert(g.linked(0,s)&&g.linked(s,contact)&&
           g.linked(contact,body)&&g.linked(body,out));
    const auto chainDry=measure(g,0.f,0.f);
    const auto chainTouch=measure(g,0.f,1.f);
    assert(chainDry.after>1.e-14);
    assert(chainTouch.after<chainDry.after*.90);
    // Contact type and point along vibrating object change the response.
    g.nodes[contact].material=Material::Rubber;
    g.nodes[contact].position=.05f;
    const auto rubber=measure(g,1.f,1.f);
    g.nodes[contact].material=Material::Felt;
    g.nodes[contact].position=.5f;
    const auto felt=measure(g,1.f,1.f);
    assert(std::fabs(felt.after-rubber.after)>
        .005*std::max(felt.after,rubber.after));
    std::printf("PASS string contact: original %.9g, M changed %.9g; rest %.9g; explicit chain %.9g -> %.9g\n",
        noContact.after,applyMidNote.after,resting.after,
        chainDry.after,chainTouch.after);
}
