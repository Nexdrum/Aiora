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
    if(module.type==FxType::MetalCore){
        const float osRate=sr*2.0f;
        coreTightPole=pole(p[2],osRate);
        coreInterPole=pole(5100.0f,osRate);
        corePickPole=pole(1700.0f,osRate);
        coreSubPole=pole(56.0f,osRate);
        coreBodyPole=pole(210.0f,osRate);
        coreDcPole=pole(22.0f,sr);
        // Blackman-windowed 33-tap decimation filter, centered below
        // the 1x Nyquist limit to reduce hard-clip aliasing.
        constexpr int taps=33,half=(taps-1)/2;
        constexpr float fc=.235f;
        float total=0.0f;
        for(int i=0;i<taps;++i){
            const int k=i-half;
            const float sinc=k==0?2.0f*fc:
                std::sin(2.0f*pi*fc*k)/(pi*k);
            const float window=.42f-.5f*std::cos(2.0f*pi*i/(taps-1))+
                .08f*std::cos(4.0f*pi*i/(taps-1));
            coreFir[i]=sinc*window;
            total+=coreFir[i];
        }
        for(auto& tap:coreFir)tap/=total;
        constexpr float frequencies[4]={125.0f,850.0f,2700.0f,5150.0f};
        const float gains[4]={(p[3]-5.0f)*1.05f,p[4],p[5],p[6]};
        for(int ch=0;ch<2;++ch)
            for(int i=0;i<4;++i)
                eq[ch][i].peak(frequencies[i],.84f,gains[i],sr);
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
    for(auto& h:coreHistory)h.fill(0.0f);
    corePosition.fill(0);
    corePrevious.fill(0.0f);coreTightLow.fill(0.0f);
    coreInter.fill(0.0f);corePickLow.fill(0.0f);
    coreSubLow.fill(0.0f);coreBassBody.fill(0.0f);
    coreDc.fill(0.0f);
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
        case FxType::MetalCore:{
            const auto pedal=[&](float input,int ch){
                const float gain=1.0f+p[1]*3.1f;
                // Input level governs EVERY branch, including post-clip punch.
                const float inputSample=input*p[0];
                float output=0.0f;
                for(int subStep=0;subStep<2;++subStep){
                    const float sample=subStep==0?
                        .5f*(corePrevious[ch]+inputSample):inputSample;
                    // Keep bass out of both clipping stages, then mix a
                    // short low-pitched impact back in after saturation.
                    coreSubLow[ch]+=coreSubPole*(sample-coreSubLow[ch]);
                    const float subFree=sample-coreSubLow[ch];
                    coreBassBody[ch]+=coreBodyPole*(subFree-coreBassBody[ch]);
                    const float bass=coreBassBody[ch];
                    coreTightLow[ch]+=coreTightPole*(sample-coreTightLow[ch]);
                    const float tight=sample-coreTightLow[ch];
                    corePickLow[ch]+=corePickPole*(tight-corePickLow[ch]);
                    const float attack=tight-corePickLow[ch];
                    const float first=std::tanh(
                        (tight+.18f*attack)*gain);
                    coreInter[ch]+=coreInterPole*(first-coreInter[ch]);
                    const float drive=(coreInter[ch]+.16f*attack)*
                        (1.0f+p[1]*.9f);
                    // Asymmetric hard clipping creates dense metal
                    // harmonics; the soft blend retains pick nuance.
                    const float hard=std::clamp(drive+.055f,-.82f,.69f)-.055f;
                    const float soft=std::tanh(drive*1.35f)/1.35f;
                    const float core=std::tanh((hard*.79f+soft*.21f)*
                        (1.15f+.13f*p[1]));
                    const float punch=std::tanh(bass*3.2f)*
                        (.055f+.031f*p[3]);
                    const float pick=.055f*std::tanh(attack*3.0f);
                    const float shaped=.72f*core+punch+pick;
                    auto& history=coreHistory[ch];
                    const size_t at=corePosition[ch];
                    history[at]=shaped;
                    corePosition[ch]=(at+1)%history.size();
                    if(subStep==1){
                        size_t index=corePosition[ch];
                        for(size_t tap=0;tap<coreFir.size();++tap){
                            index=(index+history.size()-1)%history.size();
                            output+=history[index]*coreFir[tap];
                        }
                    }
                }
                corePrevious[ch]=inputSample;
                coreDc[ch]+=coreDcPole*(output-coreDc[ch]);
                float y=output-coreDc[ch];
                for(int i=0;i<4;++i)y=eq[ch][i].process(y);
                return y*p[7];
            };
            return {pedal(l,0),pedal(r,1)};
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
