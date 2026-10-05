#include "AiHeuristic.h"

#include "FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace aiora {
namespace {

std::string lower(std::string_view text){
    std::string out(text);
    std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool has(const std::string& d,std::string_view token){
    return d.find(token)!=std::string::npos;
}

template<size_t N>
bool any(const std::string& d,const std::array<std::string_view,N>& words){
    for(auto word:words)if(has(d,word))return true;
    return false;
}

void disableAll(Patch& p){
    for(auto& op:p.ops){op.enabled=false;op.level=0.0f;}
}

void op(
    Patch& p,int index,Wave wave,float ratio,float level,
    float attack,float decay,float sustain,float release,
    float detune=0.0f){
    if(index<0||index>=static_cast<int>(p.ops.size()))return;
    auto& o=p.ops[static_cast<size_t>(index)];
    o.enabled=true;o.wave=wave;o.ratio=ratio;o.detuneCents=detune;o.level=level;
    o.env={attack,decay,sustain,release};
}

void setHarm(Patch& p,int index,const std::array<float,16>& harm){
    auto& o=p.ops[static_cast<size_t>(index)];
    o.harm=harm;o.hasHarm=true;
}

void defaultMovement(Patch& p){
    const float cutoff=std::clamp(p.filter.cutoff,40.0f,18000.0f);
    const float release=std::clamp(p.amp.release,0.02f,2.0f);
    p.modSlotCount=3;
    p.modSlots[0]={ModTarget::Cutoff,cutoff,std::max(150.0f,std::round(cutoff*0.4f))};
    p.modSlots[1]={ModTarget::AmpRelease,release,std::min(0.08f,release*0.3f)};
    p.modSlots[2]={ModTarget::Volume,1.0f,0.7f};
    p.modSlots[3]={};
}

Patch drumPatch(const std::string& d,std::string name){
    Patch p=makeDefaultPatch(name.c_str());
    disableAll(p);
    const bool kick=any(d,std::array<std::string_view,4>{"kick","808","bass drum","sub drum"});
    const bool snare=has(d,"snare");
    const bool hat=has(d,"hihat")||has(d,"hi-hat")||has(d,"hat");
    const bool crash=has(d,"crash")||has(d,"cymbal");
    const bool ride=has(d,"ride");
    const bool floorTom=has(d,"floortom")||has(d,"floor tom");
    const bool tom=has(d,"tom");
    const bool bongo=has(d,"bongo");
    const bool conga=has(d,"conga");
    const bool clap=has(d,"clap");
    const bool shaker=has(d,"shaker");
    const bool cowbell=has(d,"cowbell");

    if(kick){
        op(p,0,Wave::Sine,1.0f,1.0f,.002f,.25f,0,.12f);
        op(p,4,Wave::Sine,.5f,.8f,.002f,.22f,0,.12f);
        p.filter={FilterType::Lowpass,700,1,0,{.005f,.1f,.5f,.12f}};
        p.amp={.002f,.3f,0,.15f};p.volume=.9f;
    }else if(hat){
        op(p,0,Wave::Noise,1,.5f,.001f,.05f,0,.05f);
        p.filter={FilterType::Highpass,7000,.7f,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,.06f,0,.06f};p.volume=.7f;
    }else if(crash||ride){
        op(p,0,Wave::Noise,1,ride?.45f:.5f,.001f,ride?1.2f:.8f,0,ride?1.2f:.9f);
        op(p,1,Wave::Square,ride?5.0f:6.0f,ride?.15f:.12f,.001f,ride?1.0f:.6f,0,ride?1.0f:.7f);
        p.filter={FilterType::Highpass,ride?4500.0f:5000.0f,.7f,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,ride?1.2f:.9f,0,ride?1.3f:1.0f};p.volume=ride?.7f:.75f;p.fx.reverb=ride?.3f:.35f;
    }else if(floorTom||tom||bongo||conga){
        const float ratio2=(bongo||conga)?2.0f:1.5f;
        const float decay=floorTom?.4f:conga?.3f:bongo?.2f:.3f;
        op(p,0,Wave::Sine,1,.9f,.001f,decay,0,decay*.75f);
        op(p,1,Wave::Sine,ratio2,.3f,.001f,decay*.8f,0,decay*.6f);
        const float cutoff=floorTom?900.0f:tom?1200.0f:bongo?800.0f:600.0f;
        p.filter={(bongo||conga)?FilterType::Bandpass:FilterType::Lowpass,cutoff,1.2f,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,decay,0,decay*.8f};p.volume=.85f;
    }else if(clap){
        op(p,0,Wave::Noise,1,.7f,.001f,.15f,0,.15f);
        p.filter={FilterType::Bandpass,1500,1.5f,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,.16f,0,.16f};p.volume=.8f;
    }else if(shaker){
        op(p,0,Wave::Noise,1,.4f,.001f,.09f,0,.09f);
        p.filter={FilterType::Highpass,6000,.7f,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,.09f,0,.09f};p.volume=.65f;
    }else if(cowbell){
        op(p,0,Wave::Square,1,.6f,.001f,.25f,0,.2f);
        op(p,1,Wave::Square,1.48f,.4f,.001f,.22f,0,.18f);
        p.filter={FilterType::Bandpass,1200,2,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,.25f,0,.22f};p.volume=.8f;
    }else{
        op(p,0,Wave::Triangle,1,.55f,.001f,.15f,0,.12f);
        op(p,2,Wave::Noise,1,.6f,.001f,.12f,0,.1f);
        p.filter={FilterType::Bandpass,1800,1,0,{.005f,.1f,.5f,.12f}};
        p.amp={.001f,.18f,0,.14f};p.volume=.85f;
    }
    p.velocityAmp=.5f;p.velocityFilter=.3f;p.unison=0;p.fx.delay=0;p.fx.delayFeedback=0;
    defaultMovement(p);
    return p;
}

} // namespace

Patch heuristicPatch(std::string_view description){
    const std::string d=lower(description);
    std::string name="AI "+std::string(description.substr(0,std::min<size_t>(24,description.size())));

    const bool drum=any(d,std::array<std::string_view,16>{
        "drum","kick","snare","hat","cymbal","percussion","nexdrum","808",
        "breakbeat","tom","conga","bongo","clap","shaker","cowbell","ride"});
    if(drum){
        const bool specific=any(d,std::array<std::string_view,13>{
            "kick","snare","hat","tom","crash","ride","clap","shaker",
            "cowbell","bongo","conga","cymbal","808"});
        if(specific)return drumPatch(d,std::move(name));
        Patch q=makeFactoryPatch(FactoryPreset::Nexdrum);q.name=std::move(name);return q;
    }

    Patch p=makeDefaultPatch(name.c_str());
    disableAll(p);

    const bool bowed=any(d,std::array<std::string_view,7>{"violin","viola","cello","string","bow","erhu","fiddle"});
    const bool brass=any(d,std::array<std::string_view,5>{"trumpet","trombone","horn","brass","sax"});
    const bool flute=any(d,std::array<std::string_view,8>{"flute","clarinet","oboe","woodwind","recorder","whistle","shakuhachi","ney"});
    const bool bell=any(d,std::array<std::string_view,10>{"bell","ep","rhodes","wurl","piano","clav","vibra","marimba","kalimba","pluck"});
    const bool bass=any(d,std::array<std::string_view,4>{"bass","808","sub","tuba"});
    const bool lead=any(d,std::array<std::string_view,6>{"lead","saw","supersaw","trance","acid","303"});
    const bool pad=any(d,std::array<std::string_view,7>{"pad","choir","dream","ambient","drone","space","cosmic"});
    const bool sitar=any(d,std::array<std::string_view,7>{"sitar","guitar","banjo","koto","oud","harp","nylon"});

    if(bowed){
        op(p,0,Wave::Custom,1,.85f,.09f,.3f,.9f,.45f);
        setHarm(p,0,{1,.55f,.38f,.28f,.2f,.15f,.11f,.085f,.06f,.05f,.04f,.03f,.025f,.02f,.015f,.01f});
        op(p,1,Wave::Saw,1.004f,.5f,.09f,.3f,.9f,.45f);
        op(p,2,Wave::Sine,2.003f,.22f,.06f,.3f,.8f,.4f);
        p.matrix[2][0]=.18f;
        p.filter={FilterType::Lowpass,2600,1.4f,.18f,{.12f,.35f,.7f,.4f}};
        p.amp={.08f,.3f,.92f,.5f};p.lfo={5.6f,.3f,.4f,.5f,LfoTarget::Pitch};p.unison=.55f;
    }else if(brass){
        op(p,0,Wave::Saw,1,.85f,.05f,.25f,.85f,.3f);
        op(p,1,Wave::Saw,.5f,.5f,.05f,.25f,.85f,.3f);
        p.filter={FilterType::Lowpass,2800,2,.45f,{.06f,.25f,.55f,.3f}};
        p.amp={.05f,.25f,.85f,.3f};p.unison=.4f;
    }else if(flute){
        op(p,0,Wave::Custom,1,.9f,.04f,.2f,.9f,.3f);
        setHarm(p,0,{1,.25f,.08f,.03f,.015f,.008f,.005f,.003f,.002f,.001f,0,0,0,0,0,0});
        op(p,1,Wave::Sine,2,.12f,.04f,.2f,.7f,.3f);
        p.filter={FilterType::Lowpass,3200,.7f,.1f,{.05f,.2f,.8f,.3f}};
        p.amp={.04f,.2f,.9f,.32f};p.lfo={4.8f,.22f,0,0,LfoTarget::Pitch};
    }else if(bass){
        op(p,0,Wave::Sine,1,1,.004f,.1f,1,.16f);
        op(p,1,Wave::Saw,1,.5f,.004f,.12f,.8f,.18f);
        p.filter={FilterType::Lowpass,700,2,.25f,{.005f,.15f,.5f,.18f}};
        p.amp={.004f,.1f,1,.18f};
    }else if(lead){
        op(p,0,Wave::Saw,1,.8f,.004f,.12f,.8f,.2f);
        op(p,1,Wave::Saw,1.007f,.8f,.004f,.12f,.8f,.2f);
        op(p,3,Wave::Square,.5f,.4f,.004f,.12f,.8f,.2f);
        p.filter={FilterType::Lowpass,3800,4,.3f,{.005f,.18f,.5f,.2f}};
        p.amp={.004f,.12f,.82f,.22f};p.unison=.7f;p.fx.distortion=.2f;p.fx.delay=.2f;
    }else if(pad){
        op(p,0,Wave::Saw,1,.5f,.6f,.8f,.9f,1.2f);
        op(p,1,Wave::Saw,1.006f,.5f,.6f,.8f,.9f,1.2f);
        op(p,2,Wave::Sine,.5f,.4f,.5f,.8f,.9f,1.2f);
        p.filter={FilterType::Lowpass,1800,.8f,.15f,{.5f,.8f,.7f,1}};
        p.amp={.55f,.8f,.9f,1.3f};p.unison=1;p.fx.reverb=.45f;p.lfo={.4f,.3f,0,0,LfoTarget::Filter};
    }else if(bell||sitar){
        op(p,0,Wave::Sine,1,.9f,.003f,.5f,.35f,.6f);
        op(p,1,Wave::Sine,sitar?2.99f:3.5f,.5f,.003f,.35f,.15f,.5f);
        op(p,2,Wave::Sine,7,.18f,.003f,.25f,.08f,.4f);
        p.matrix[1][0]=.5f;p.matrix[2][0]=.25f;
        p.filter={FilterType::Lowpass,6000,.7f,0,{.01f,.2f,.9f,.4f}};
        p.amp={.003f,.45f,.35f,.65f};p.fx.delay=.22f;p.fx.reverb=.35f;
    }else{
        op(p,0,Wave::Saw,1,.7f,.01f,.2f,.8f,.3f);
        op(p,1,Wave::Sine,2,.3f,.01f,.2f,.6f,.3f);
        p.matrix[1][0]=.3f;p.filter.cutoff=4000;
    }

    if(any(d,std::array<std::string_view,4>{"muted","soft","dark","mellow"}))
        p.filter.cutoff=std::min(p.filter.cutoff,1600.0f);
    if(any(d,std::array<std::string_view,4>{"bright","harsh","acid","sharp"})){
        p.filter.cutoff=std::max(p.filter.cutoff,5000.0f);
        p.filter.resonance=std::max(p.filter.resonance,5.0f);
    }
    if(any(d,std::array<std::string_view,6>{"space","cosmic","alien","ghost","monster","dragon"})){
        p.matrix[1][0]=.7f;
        op(p,3,Wave::Sine,1.7f,.5f,.005f,.1f,.8f,.15f);
        p.matrix[3][0]=.5f;p.fx.reverb=.55f;p.fx.delay=.3f;
        p.lfo={.6f,.5f,0,0,LfoTarget::Pitch};
    }

    defaultMovement(p);
    return p;
}

} // namespace aiora
