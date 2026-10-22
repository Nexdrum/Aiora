#include "FxProcessor.h"
#include <algorithm>
#include <cmath>

namespace aiora {
namespace {
constexpr float pi=3.14159265358979323846f;
constexpr std::array<float,4> combMs{29.7f,37.1f,41.1f,43.7f};
constexpr std::array<float,2> allMs{5.0f,1.7f};
constexpr std::array<float,kPrismaBands> prismaHz{{
    31,47,70,106,160,240,360,540,810,1200,1800,2700,4050,6100,9200,14000
}};
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
    if(allTypes||type==FxType::Recurra){
        const size_t size=static_cast<size_t>(sr*std::max(1.2f,maxDelay))+2;
        if(delay.size()<size)delay.resize(size,0.0f);
    }
    if(allTypes||type==FxType::Vastrel){
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
    if(module.type==FxType::Embera){
        // Float quantization is the entire bitcrush effect; at zero the
        // original saturation transfer function is bit-for-bit unchanged.
        const float amount=std::clamp(p[3],0.0f,1.0f);
        crushEnabled=amount>0.0f;
        crushLevels=std::exp2(16.0f-14.0f*amount)-1.0f;
    }
    if(module.type==FxType::Prisma){
        for(int channel=0;channel<2;++channel)
            for(size_t i=0;i<kPrismaBands;++i)
                eq[channel][i].peak(prismaHz[i],1.65f,p[i],sr);
    }
    if(module.type==FxType::Ampere){
        const float osRate=sr*2.0f;
        ampTightPole=pole(p[2],osRate);
        ampMidLowPole=pole(520.0f,osRate);
        ampMidHighPole=pole(2700.0f,osRate);
        ampPickPole=pole(2100.0f,osRate);
        ampInterPole=pole(4900.0f,osRate);
        ampGrowlLowPole=pole(440.0f,osRate);
        ampGrowlHighPole=pole(145.0f,osRate);
        ampDcPole=pole(16.0f,sr);
        // Fast response tracks the actual audio while allowing the
        // synth's own attack and release to remain audible.
        ampAttackPole=pole(900.0f,sr);
        ampReleasePole=pole(160.0f,sr);
        for(int ch=0;ch<2;++ch){
            eq[ch][0].peak(850.0f,0.85f,p[3]*.35f,sr);
            eq[ch][1].peak(3000.0f,0.9f,p[4]*.55f,sr);
        }
        // The two algorithms never share a Stage, so this proven
        // two-times anti-aliasing FIR can use the same per-slot buffers.
        constexpr int taps=33,half=16;
        float total=0.0f;
        for(int i=0;i<taps;++i){
            const int k=i-half;
            constexpr float fc=.235f;
            const float sinc=k==0?2.0f*fc:
                std::sin(2.0f*pi*fc*k)/(pi*k);
            const float window=.42f-.5f*std::cos(2.0f*pi*i/(taps-1))+
                .08f*std::cos(4.0f*pi*i/(taps-1));
            coreFir[i]=sinc*window;total+=coreFir[i];
        }
        for(auto& tap:coreFir)tap/=total;
    }
    if(module.type==FxType::Cabeus){
        cabLowPole=pole(p[3],sr);
        const int model=std::clamp(static_cast<int>(std::round(p[0])),0,2);
        const float mic=std::clamp(p[1],0.0f,1.0f);
        const float distance=std::clamp(p[2],0.0f,1.0f);
        const float cut=(model==0?5800.0f:model==1?6900.0f:7800.0f)*
            (1.0f-.38f*mic)*(1.0f-.22f*distance);
        cabHighPole=pole(std::min(p[4],cut),sr);
        // Procedural speaker impulse: three cascaded minimum-delay poles
        // followed by cabinet-dependent early acoustic reflections.
        // Runtime processing is fixed 64-tap convolution, allocation free.
        const float a=std::exp(-2.0f*pi*std::clamp(cut,1000.0f,sr*.44f)/sr);
        std::array<float,64> lowpass{};
        float z1=0,z2=0,z3=0;
        for(size_t n=0;n<lowpass.size();++n){
            const float in=n==0?1.0f:0.0f;
            z1+=(1.0f-a)*(in-z1);
            z2+=(1.0f-a)*(z1-z2);
            z3+=(1.0f-a)*(z2-z3);
            lowpass[n]=z3;
        }
        const int d1=model==0?5:model==1?7:9;
        const int d2=model==0?13:model==1?17:22;
        const int d3=model==0?23:model==1?27:35;
        float sum=0;
        for(int n=0;n<64;++n){
            float v=lowpass[static_cast<size_t>(n)];
            if(n>=d1)v-=.28f*lowpass[static_cast<size_t>(n-d1)];
            if(n>=d2)v+=.21f*lowpass[static_cast<size_t>(n-d2)];
            if(n>=d3)v-=.10f*lowpass[static_cast<size_t>(n-d3)];
            cabImpulse[static_cast<size_t>(n)]=v;sum+=v;
        }
        if(std::fabs(sum)>1.0e-6f)
            for(auto& tap:cabImpulse)tap/=sum;
        const float resonance=std::clamp(p[5],0.0f,1.0f);
        const float bodyHz=model==0?180.0f:model==1?145.0f:115.0f;
        const float bodyGain=(resonance-.5f)*11.0f+(1.0f-distance)*2.5f;
        const float biteGain=(1.0f-mic)*3.5f-mic*3.5f;
        for(int ch=0;ch<2;++ch){
            eq[ch][0].peak(bodyHz,.80f,bodyGain,sr);
            eq[ch][1].peak(model==2?1950.0f:2350.0f,1.15f,biteGain,sr);
            eq[ch][2].peak(model==0?4100.0f:3650.0f,1.3f,-2.0f-2.0f*mic,sr);
        }
    }
    if(module.type==FxType::Vastrel){
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
    for(auto& h:coreHistory)h.fill(0.0f);
    corePosition.fill(0);
    corePrevious.fill(0.0f);
    ampEnvelope.fill(0.0f);ampTightLow.fill(0.0f);
    ampMidLow.fill(0.0f);ampMidHigh.fill(0.0f);
    ampPickLow.fill(0.0f);ampInter.fill(0.0f);
    ampGrowlLow.fill(0.0f);ampGrowlHigh.fill(0.0f);
    ampDc.fill(0.0f);
    for(auto& h:cabHistory)h.fill(0.0f);
    cabPosition.fill(0);cabLow.fill(0.0f);cabHigh.fill(0.0f);
}
std::array<float,2> FxProcessor::Stage::process(float l,float r,float sr) noexcept {
    const auto& p=module.params;
    switch(module.type){
        case FxType::Embera:{
            const float drive=1.0f+std::clamp(p[1],0.0f,10.0f)*8.0f;
            const auto saturate=[&](float x){
                float y=std::tanh(x*p[0]*drive);
                if(crushEnabled){
                    // Uniform quantization of a signed full-scale wave.
                    // Slider 0: bypass; slider 1: approximately 2-bit.
                    const float normalized=(std::clamp(y,-1.0f,1.0f)+1.0f)*.5f;
                    const float quantized=std::floor(normalized*crushLevels+.5f)/
                        crushLevels;
                    y=2.0f*quantized-1.0f;
                }
                return y*p[2];
            };
            return {saturate(l),saturate(r)};
        }
        case FxType::Ampere:{
            // Musical dynamics come from the audio amplitude itself; the
            // operator envelopes and note gates are never inspected here.
            const auto amplifier=[&](float input,int ch){
                const float x=input*p[0];
                const float level=std::fabs(x);
                const float tracking=level>ampEnvelope[ch]?
                    ampAttackPole:ampReleasePole;
                ampEnvelope[ch]+=tracking*(level-ampEnvelope[ch]);
                const float env=ampEnvelope[ch];
                // Bounded adaptive drive keeps quieter resonance gritty,
                // but never normalizes notes to a common output level.
                const float compensation=std::clamp(
                    std::pow(.10f/(env+.025f),.28f),.75f,2.15f);
                const float drive=(1.4f+1.55f*p[1])*compensation;
                const float dynamicGain=.44f+.56f*std::sqrt(
                    env/(env+.085f));
                float output=0.0f;
                for(int sub=0;sub<2;++sub){
                    const float sample=sub==0?.5f*(corePrevious[ch]+x):x;
                    ampTightLow[ch]+=ampTightPole*(sample-ampTightLow[ch]);
                    const float tight=sample-ampTightLow[ch];
                    ampMidLow[ch]+=ampMidLowPole*(tight-ampMidLow[ch]);
                    ampMidHigh[ch]+=ampMidHighPole*(tight-ampMidHigh[ch]);
                    const float midBand=ampMidHigh[ch]-ampMidLow[ch];
                    const float presence=tight-ampMidHigh[ch];
                    const float pre=tight+
                        (db(p[3]) - 1.0f)*midBand*.77f+
                        (db(p[4]) - 1.0f)*presence*.58f;
                    ampPickLow[ch]+=ampPickPole*(pre-ampPickLow[ch]);
                    const float pick=pre-ampPickLow[ch];
                    const float first=std::tanh((pre+.12f*pick)*drive+.045f)-
                        std::tanh(.045f);
                    ampInter[ch]+=ampInterPole*(first-ampInter[ch]);
                    // Asymmetric saturator after a frequency-dependent first
                    // stage, not a hard peak limiter.
                    const float secondDrive=(ampInter[ch]+.11f*pick)*
                        (1.0f+.20f*p[1]);
                    const float second=.72f*(std::tanh(secondDrive*1.55f+.035f)-
                        std::tanh(.035f))+
                        .28f*std::tanh(secondDrive*.7f);
                    ampGrowlLow[ch]+=ampGrowlLowPole*(sample-ampGrowlLow[ch]);
                    ampGrowlHigh[ch]+=ampGrowlHighPole*(sample-ampGrowlHigh[ch]);
                    const float growl=ampGrowlLow[ch]-ampGrowlHigh[ch];
                    // The low-mid growl is input-fed only. No free-running
                    // feedback and no artificial release after a cut.
                    const float definition=std::clamp(p[5],0.0f,1.0f);
                    const float articulate=std::tanh(pre*(2.0f+p[1]*.4f));
                    const float shaped=(second*(1.0f-.32f*definition)+
                        articulate*(.32f*definition)+.095f*growl)*
                        dynamicGain;
                    auto& history=coreHistory[ch];
                    const size_t at=corePosition[ch];
                    history[at]=shaped;
                    corePosition[ch]=(at+1)%history.size();
                    if(sub==1){
                        size_t index=corePosition[ch];
                        for(size_t tap=0;tap<coreFir.size();++tap){
                            index=(index+history.size()-1)%history.size();
                            output+=history[index]*coreFir[tap];
                        }
                    }
                }
                corePrevious[ch]=x;
                ampDc[ch]+=ampDcPole*(output-ampDc[ch]);
                float y=output-ampDc[ch];
                for(int i=0;i<2;++i)y=eq[ch][i].process(y);
                if(p[6]>.0001f){
                    // Downward expander: silent input suppresses residual
                    // fuzz, without forcing musical notes to a fixed level.
                    const float threshold=.00004f*std::pow(220.0f,p[6]);
                    const float t=std::clamp(env/threshold,0.0f,1.0f);
                    y*=t*t*(3.0f-2.0f*t);
                }
                return y*p[7];
            };
            return {amplifier(l,0),amplifier(r,1)};
        }
        case FxType::Cabeus:{
            const auto cabinet=[&](float x,int ch){
                auto& history=cabHistory[ch];
                history[cabPosition[ch]]=x;
                size_t at=cabPosition[ch];
                float wet=0.0f;
                for(size_t i=0;i<cabImpulse.size();++i){
                    wet+=history[at]*cabImpulse[i];
                    at=(at+history.size()-1)%history.size();
                }
                cabPosition[ch]=(cabPosition[ch]+1)%history.size();
                cabLow[ch]+=cabLowPole*(wet-cabLow[ch]);
                wet-=cabLow[ch];
                cabHigh[ch]+=cabHighPole*(wet-cabHigh[ch]);
                wet=cabHigh[ch];
                for(int i=0;i<3;++i)wet=eq[ch][i].process(wet);
                const float mix=std::clamp(p[6],0.0f,1.0f);
                return (x*(1.0f-mix)+wet*mix)*p[7];
            };
            return {cabinet(l,0),cabinet(r,1)};
        }
        case FxType::Prisma:{
            for(size_t i=0;i<kPrismaBands;++i){
                l=eq[0][i].process(l);r=eq[1][i].process(r);
            }
            return {l*p[kPrismaBands],r*p[kPrismaBands]};
        }
        case FxType::Recurra:{
            if(delay.empty())return {l,r};
            const size_t len=std::clamp<size_t>(static_cast<size_t>(std::clamp(p[1],.03f,1.0f)*sr),1,delay.size()-1);
            const size_t read=(delayWrite+delay.size()-len)%delay.size();
            const float d=delay[read];
            delay[delayWrite]=(l+r)*.5f+d*std::clamp(p[2],0.0f,.85f);
            delayWrite=(delayWrite+1)%delay.size();
            return {l+d*.3f*p[0],r+d*.3f*p[0]};
        }
        case FxType::Vastrel:{
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
        for(auto& stage:stages_)stage.prepare(sampleRate_,maxDelaySeconds_,FxType::Recurra,true);
    set(fx_);reset();
}
void FxProcessor::set(const Fx& fx){
    fx_=fx;
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
