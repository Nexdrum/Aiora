#include "FxProcessor.h"
#include <algorithm>
#include <cmath>

namespace aiora {

float FxProcessor::Comb::process(float x) noexcept {
    if(buf.empty())return 0.0f;
    const float y=buf[pos];
    store=y*(1.0f-damp)+store*damp;
    buf[pos]=x+store*feedback;
    pos=(pos+1)%buf.size();
    return y;
}
void FxProcessor::Comb::reset() noexcept { std::fill(buf.begin(),buf.end(),0.0f);pos=0;store=0; }
float FxProcessor::Allpass::process(float x) noexcept {
    if(buf.empty())return x;
    const float b=buf[pos];
    const float y=-x+b;
    buf[pos]=x+b*feedback;
    pos=(pos+1)%buf.size();
    return y;
}
void FxProcessor::Allpass::reset() noexcept { std::fill(buf.begin(),buf.end(),0.0f);pos=0; }

void FxProcessor::prepare(float sampleRate,float maxDelaySeconds) {
    sampleRate_=std::max(8000.0f,sampleRate);
    delay_.assign(static_cast<size_t>(sampleRate_*std::max(1.2f,maxDelaySeconds))+2,0.0f);
    const std::array<float,4> combMs{29.7f,37.1f,41.1f,43.7f};
    const std::array<float,2> allMs{5.0f,1.7f};
    for(size_t i=0;i<4;++i){
        combL_[i].buf.assign(static_cast<size_t>(sampleRate_*combMs[i]/1000.0f),0.0f);
        combR_[i].buf.assign(static_cast<size_t>(sampleRate_*(combMs[i]+1.3f)/1000.0f),0.0f);
    }
    for(size_t i=0;i<2;++i){
        allL_[i].buf.assign(static_cast<size_t>(sampleRate_*allMs[i]/1000.0f),0.0f);
        allR_[i].buf.assign(static_cast<size_t>(sampleRate_*(allMs[i]+0.7f)/1000.0f),0.0f);
    }
    reset();
}
void FxProcessor::reset() noexcept {
    std::fill(delay_.begin(),delay_.end(),0.0f);delayWrite_=0;
    for(auto& c:combL_) c.reset();
    for(auto& c:combR_) c.reset();
    for(auto& a:allL_) a.reset();
    for(auto& a:allR_) a.reset();
}

std::array<float,2> FxProcessor::process(float input) noexcept {
    return processStereo(input,input);
}

std::array<float,2> FxProcessor::processStereo(float left,float right) noexcept {
    float dryL=left,dryR=right;
    if(fx_.distortion>0.005f){
        const float drive=1.0f+fx_.distortion*8.0f;
        dryL=std::tanh(dryL*drive);
        dryR=std::tanh(dryR*drive);
    }
    const float mono=(dryL+dryR)*0.5f;

    float d=0.0f;
    if(!delay_.empty()&&fx_.delay>0.005f){
        const float sec=std::clamp(fx_.delayTime,0.03f,1.0f);
        const size_t delaySamples=std::clamp<size_t>(static_cast<size_t>(sec*sampleRate_),1,delay_.size()-1);
        const size_t rp=(delayWrite_+delay_.size()-delaySamples)%delay_.size();
        d=delay_[rp];
        delay_[delayWrite_]=mono+d*std::clamp(fx_.delayFeedback,0.0f,0.85f);
        delayWrite_=(delayWrite_+1)%delay_.size();
    }else if(!delay_.empty()){
        delay_[delayWrite_]=mono;delayWrite_=(delayWrite_+1)%delay_.size();
    }
    const float delayWet=d*(0.3f*std::clamp(fx_.delay,0.0f,1.0f));

    float rl=0.0f,rr=0.0f;
    if(fx_.reverb>0.005f){
        for(auto& c:combL_)rl+=c.process(mono*0.25f);
        for(auto& c:combR_)rr+=c.process(mono*0.25f);
        for(auto& a:allL_)rl=a.process(rl);
        for(auto& a:allR_)rr=a.process(rr);
        const float rw=0.48f*std::clamp(fx_.reverb,0.0f,1.0f);
        rl*=rw;rr*=rw;
    }
    return {dryL+delayWet+rl,dryR+delayWet+rr};
}

} // namespace aiora
