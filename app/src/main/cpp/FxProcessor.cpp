#include "FxProcessor.h"
#include <algorithm>
#include <cmath>

namespace aiora {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr std::array<float,4> combMs{29.7f,37.1f,41.1f,43.7f};
constexpr std::array<float,2> allMs{5.0f,1.7f};
float pole(float hz,float sr) noexcept {
    return 1.0f-std::exp(-2.0f*pi*std::clamp(hz,10.0f,sr*.44f)/sr);
}
float db(float v) noexcept {return std::pow(10.0f,v/20.0f);}
}
float FxProcessor::Comb::process(float x) noexcept {
    if(length==0)return 0.0f;
    const float y=buf[pos];store=y*(1.0f-damp)+store*damp;
    buf[pos]=x+store*feedback;pos=(pos+1)%length;return y;
}
void FxProcessor::Comb::reset() noexcept {
    std::fill(buf.begin(),buf.end(),0.0f);pos=0;store=0;
}
float FxProcessor::Allpass::process(float x) noexcept {
    if(length==0)return x;
    const float b=buf[pos],y=-x+b;
    buf[pos]=x+b*feedback;pos=(pos+1)%length;return y;
}
void FxProcessor::Allpass::reset() noexcept {
    std::fill(buf.begin(),buf.end(),0.0f);pos=0;
}
void FxProcessor::Biquad::peak(float freq,float q,float gainDb,float sr) noexcept {
    freq=std::clamp(freq,20.0f,sr*.44f);
    const float a=std::pow(10.0f,gainDb/40.0f);
    const float w=2*pi*freq/sr,c=std::cos(w),alpha=std::sin(w)/(2*std::max(.2f,q));
    const float n=1+alpha/a;
    b0=(1+alpha*a)/n;b1=(-2*c)/n;b2=(1-alpha*a)/n;
    a1=(-2*c)/n;a2=(1-alpha/a)/n;
}
void FxProcessor::Stage::prepare(float sr,float maxDelay,FxType type,bool allTypes){
    if(allTypes||type==FxType::Delay){
        const size_t size=static_cast<size_t>(sr*std::max(1.2f,maxDelay))+2;
        if(delay.size()<size)delay.resize(size,0.0f);
    }
    if(allTypes||type==FxType::Reverb){
        for(size_t i=0;i<4;++i){
            const size_t a=static_cast<size_t>(sr*combMs[i]*2.5f/1000.0f)+2;
            const size_t b=static_cast<size_t>(sr*(combMs[i]+1.3f)*2.5f/1000.0f)+2;
            if(combL[i].buf.size()<a)combL[i].buf.resize(a,0.0f);
            if(combR[i].buf.size()<b)combR[i].buf.resize(b,0.0f);
        }
        for(size_t i=0;i<2;++i){
            const size_t a=static_cast<size_t>(sr*allMs[i]*2.5f/1000.0f)+2;
            const size_t b=static_cast<size_t>(sr*(allMs[i]+.7f)*2.5f/1000.0f)+2;
            if(allL[i].buf.size()<a)allL[i].buf.resize(a,0.0f);
            if(allR[i].buf.size()<b)allR[i].buf.resize(b,0.0f);
        }
    }
}
void FxProcessor::Stage::configure(float sr){
    const auto& p=module.params;
    if(module.type==FxType::BandEq){
        constexpr float f[5]={80,240,750,2200,6000};
        for(int channel=0;channel<2;++channel)
            for(int i=0;i<5;++i)eq[channel][i].peak(f[i],.95f,p[i],sr);
    }
    if(module.type==FxType::MetalAmp){
        cutoffA=pole(p[2],sr);cutoffB=pole(3700.0f,sr);
        constexpr float f[4]={120,850,3200,4900};
        for(int ch=0;ch<2;++ch)
            for(int i=0;i<4;++i)eq[ch][i].peak(f[i],.85f,p[3+i],sr);
    }
    if(module.type==FxType::Cabinet){
        cutoffA=pole(p[0],sr);cutoffB=pole(p[1],sr);
        for(int ch=0;ch<2;++ch){
            eq[ch][0].peak(145,.82f,(p[2]-.5f)*12.0f,sr);
            eq[ch][1].peak(1800,1.15f,(p[3]-.5f)*10.0f,sr);
            eq[ch][2].peak(3700,1.05f,-2.5f,sr);
        }
    }
    if(module.type==FxType::Reverb){
        const float room=std::clamp(p[2],.5f,2.5f);
        const float decay=std::clamp(p[1],.2f,12.0f);
        const auto conf=[&](Comb& c,float ms){
            if(c.buf.empty())return;
            const auto len=std::clamp<size_t>(static_cast<size_t>(sr*ms*room*.001f),1,c.buf.size());
            if(c.length!=len){c.length=len;c.pos%=len;}
            c.feedback=std::clamp(std::pow(.001f,(len/sr)/decay),0.0f,.995f);
        };
        const auto ap=[&](Allpass& a,float ms){
            if(a.buf.empty())return;
            const auto len=std::clamp<size_t>(static_cast<size_t>(sr*ms*room*.001f),1,a.buf.size());
            if(a.length!=len){a.length=len;a.pos%=len;}
        };
        for(size_t i=0;i<4;++i){conf(combL[i],combMs[i]);conf(combR[i],combMs[i]+1.3f);}
        for(size_t i=0;i<2;++i){ap(allL[i],allMs[i]);ap(allR[i],allMs[i]+.7f);}
    }
}
void FxProcessor::Stage::reset() noexcept {
    std::fill(delay.begin(),delay.end(),0.0f);delayWrite=0;
    for(auto& c:combL)c.reset();for(auto& c:combR)c.reset();
    for(auto& a:allL)a.reset();for(auto& a:allR)a.reset();
    for(auto& ch:eq)for(auto& q:ch)q.clear();
    low.fill(0);inter.fill(0);high.fill(0);
}
std::array<float,2> FxProcessor::Stage::process(float l,float r,float sr) noexcept {
    const auto& p=module.params;
    switch(module.type){
        case FxType::Saturation:{
            const float drive=1.0f+std::clamp(p[1],0.0f,10.0f)*8.0f;
            return {std::tanh(l*p[0]*drive)*p[2],std::tanh(r*p[0]*drive)*p[2]};
        }
        case FxType::MetalAmp:{
            const auto clip=[&](float x,int ch){
                low[ch]+=cutoffA*(x-low[ch]);
                const float tight=x-low[ch];
                const float first=std::tanh(tight*p[0]*(1.0f+p[1]*1.8f)+.09f)-std::tanh(.09f);
                inter[ch]+=cutoffB*(first-inter[ch]);
                const float second=std::tanh((inter[ch]+tight*.12f)*(1.0f+p[1]*1.1f));
                float y=second/(1.0f+p[1]*.13f);
                for(int i=0;i<4;++i)y=eq[ch][i].process(y);
                return y*p[7];
            };
            return {clip(l,0),clip(r,1)};
        }
        case FxType::Cabinet:{
            const auto cab=[&](float x,int ch){
                low[ch]+=cutoffA*(x-low[ch]);
                float y=x-low[ch];
                high[ch]+=cutoffB*(y-high[ch]);
                y=high[ch];
                for(int i=0;i<3;++i)y=eq[ch][i].process(y);
                return x*(1.0f-p[4])+y*p[4];
            };
            return {cab(l,0),cab(r,1)};
        }
        case FxType::BandEq:{
            for(int i=0;i<5;++i){l=eq[0][i].process(l);r=eq[1][i].process(r);}
            return {l*p[5],r*p[5]};
        }
        case FxType::Delay:{
            if(delay.empty())return {l,r};
            const size_t len=std::clamp<size_t>(static_cast<size_t>(std::clamp(p[1],.03f,1.0f)*sr),1,delay.size()-1);
            const size_t read=(delayWrite+delay.size()-len)%delay.size();
            const float d=delay[read];
            delay[delayWrite]=(l+r)*.5f+d*std::clamp(p[2],0.0f,.85f);
            delayWrite=(delayWrite+1)%delay.size();
            return {l+d*.3f*p[0],r+d*.3f*p[0]};
        }
        case FxType::Reverb:{
            const float mono=(l+r)*.5f;float a=0,b=0;
            for(auto& c:combL)a+=c.process(mono*.25f);
            for(auto& c:combR)b+=c.process(mono*.25f);
            for(auto& c:allL)a=c.process(a);
            for(auto& c:allR)b=c.process(b);
            const float wet=.48f*std::clamp(p[0],0.0f,1.0f);
            return {l+a*wet,r+b*wet};
        }
    }
    return {l,r};
}
void FxProcessor::prepare(float sampleRate,float maxDelaySeconds,bool realtimeChanges){
    sampleRate_=std::max(8000.0f,sampleRate);
    maxDelaySeconds_=std::max(1.2f,maxDelaySeconds);
    realtimeChanges_=realtimeChanges;
    if(realtimeChanges_)
        for(auto& stage:stages_)stage.prepare(sampleRate_,maxDelaySeconds_,FxType::Delay,true);
    set(fx_);reset();
}
void FxProcessor::set(const Fx& fx){
    fx_=fx;migrateLegacyFx(fx_);
    for(size_t i=0;i<fx_.count&&i<kMaxFxStages;++i){
        auto& stage=stages_[i];
        const auto type=fx_.chain[i].type;
        // Only normal (non-realtime) preparation may allocate buffers.
        if(!realtimeChanges_)stage.prepare(sampleRate_,maxDelaySeconds_,type,false);
        stage.module=fx_.chain[i];
        stage.configure(sampleRate_);
    }
}
void FxProcessor::reset() noexcept {
    for(auto& s:stages_)s.reset();
}
std::array<float,2> FxProcessor::processStereo(float l,float r) noexcept {
    for(size_t i=0;i<fx_.count&&i<kMaxFxStages;++i){
        if(!stages_[i].module.enabled)continue;
        const auto out=stages_[i].process(l,r,sampleRate_);
        l=out[0];r=out[1];
    }
    return {l,r};
}
} // namespace aiora
