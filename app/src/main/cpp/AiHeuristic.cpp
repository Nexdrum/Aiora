#include "AiHeuristic.h"
#include "FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

namespace aiora {
namespace {
std::string lower(std::string_view s){
    std::string out(s);
    std::transform(out.begin(),out.end(),out.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return out;
}
bool has(const std::string& s,std::string_view term){
    return s.find(term)!=std::string::npos;
}
template<size_t N> bool any(const std::string& s,
                            const std::array<std::string_view,N>& terms){
    for(auto term:terms)if(has(s,term))return true;
    return false;
}
// Timelines are the only amplitude/decay source. No operator ADSRs.
void tone(Patch& p,int index,Wave wave,float ratio,float level,
          float attack,float decay,float sustain,float release){
    auto& o=p.ops[static_cast<size_t>(index)];
    o=Operator{};o.enabled=true;o.wave=wave;o.ratio=ratio;o.level=level;
    o.modChannelsConfigured=true;
    for(auto& c:o.modChannels)c.enabled=false;
    auto& curve=o.volumeCurve;
    curve.enabled=true;curve.sustain=sustain>.001f;
    curve.count=4;curve.decayIndex=1;curve.releaseIndex=2;
    curve.points[0]={0,0};
    curve.points[1]={std::max(.0005f,attack),1};
    curve.points[2]={curve.points[1].time+std::max(.005f,decay),
                     std::clamp(sustain,0.0f,1.0f)};
    curve.points[3]={curve.points[2].time+std::max(.02f,release),0};
}
void brightness(Patch& p,float hz){
    auto& model=p.filter.evolving;
    model=initialEvolvingFilter();
    const float normalized=std::clamp(
        std::log(std::clamp(hz,40.0f,18000.0f)/40.0f)/
        std::log(450.0f),0.0f,1.0f);
    for(auto& pt:model.lanes[0].curve.points)
        pt.value=normalized;
    model.lanes[2].shape.fill(FilterShape::HighShelf);
    // Neutral 0 dB high shelf. Other shaping is authored by the editor.
}
void activateVelocity(Patch& p,float offset){
    // V=0 always retains a genuine contact response; hard strikes grow.
    for(auto& op:p.ops){
        if(!op.enabled)continue;
        op.modChannelsConfigured=true;
        auto channel=initialOperatorModChannel();
        channel.enabled=true;channel.destination=OperatorModDestination::Vol;
        channel.response.source=0;
        channel.response.points[0]={0,offset};
        channel.response.points[1]={1,1};
        op.modChannels[0]=channel;
    }
}
void disableAll(Patch& p){
    for(auto& o:p.ops){o.enabled=false;o.level=0;}
}
Patch percussion(const std::string& desc,std::string name){
    Patch p=makeDefaultPatch(name.c_str());disableAll(p);
    if(has(desc,"kick")||has(desc,"808")){
        tone(p,0,Wave::Sine,1,.95f,.001f,.23f,0,.15f);
        tone(p,2,Wave::Noise,1,.35f,.001f,.015f,0,.015f);
        brightness(p,1500);
    }else if(has(desc,"snare")){
        tone(p,0,Wave::Sine,1,.42f,.001f,.18f,0,.09f);
        tone(p,1,Wave::Noise,1,.75f,.001f,.12f,0,.12f);
        brightness(p,7400);
    }else if(has(desc,"ride")||has(desc,"crash")||has(desc,"cymbal")){
        tone(p,0,Wave::Noise,1,.67f,.001f,.9f,0,.8f);
        tone(p,1,Wave::Hiss,2.2f,.42f,.002f,1.1f,0,1.0f);
        brightness(p,11000);
    }else if(has(desc,"hat")||has(desc,"shaker")){
        tone(p,0,Wave::Noise,1,.7f,.001f,.09f,0,.06f);
        brightness(p,14000);
    }else{
        tone(p,0,Wave::Sine,1,.8f,.002f,.34f,0,.28f);
        tone(p,2,Wave::Noise,1,.32f,.001f,.065f,0,.04f);
        brightness(p,4300);
    }
    activateVelocity(p,.12f);
    return p;
}
} // namespace

Patch heuristicPatch(std::string_view description){
    const std::string desc=lower(description);
    const std::string name="AI "+std::string(description.substr(
        0,std::min<size_t>(24,description.size())));
    if(any(desc,std::array<std::string_view,12>{{
        "drum","kick","snare","hat","cymbal","percussion",
        "tom","bongo","clap","shaker","ride","808"}}))
        return percussion(desc,name);
    Patch p=makeDefaultPatch(name.c_str());
    disableAll(p);
    if(any(desc,std::array<std::string_view,6>{{
        "bass","sub","tuba","fretless","tribass","low"}})){
        tone(p,0,Wave::Sine,1,.88f,.005f,.2f,.84f,.22f);
        tone(p,1,Wave::Saw,1,.29f,.004f,.15f,.6f,.2f);
        brightness(p,2400);
    }else if(any(desc,std::array<std::string_view,5>{{
        "bell","piano","pluck","marimba","kalimba"}})){
        tone(p,0,Wave::Sine,1,.85f,.002f,.62f,0,.55f);
        tone(p,1,Wave::Sine,3.7f,.30f,.002f,.19f,0,.22f);
        brightness(p,8500);
    }else if(any(desc,std::array<std::string_view,5>{{
        "pad","ambient","choir","dream","space"}})){
        tone(p,0,Wave::Saw,1,.62f,.5f,.7f,.86f,1.2f);
        tone(p,1,Wave::Saw,1.006f,.36f,.5f,.7f,.86f,1.2f);
        brightness(p,3200);
    }else if(any(desc,std::array<std::string_view,4>{{
        "flute","wind","whistle","woodwind"}})){
        tone(p,0,Wave::Sine,1,.87f,.045f,.14f,.88f,.33f);
        tone(p,1,Wave::Sine,2,.12f,.045f,.14f,.7f,.3f);
        brightness(p,4000);
    }else{
        tone(p,0,Wave::Saw,1,.8f,.012f,.19f,.7f,.27f);
        tone(p,1,Wave::Sine,2,.25f,.012f,.19f,.65f,.27f);
        brightness(p,6000);
    }
    activateVelocity(p,.18f);
    return p;
}
} // namespace aiora
