#include "SpectrachordVoice.h"
#include <algorithm>
#include <cmath>

namespace aiora {
namespace {
constexpr float kPi=3.14159265358979323846f;
constexpr float kTwoPi=6.28318530717958647692f;
constexpr float kEps=1.0e-6f;
float secondsToSamples(float seconds,float sr) noexcept { return std::max(1.0f, seconds * sr); }
}

void SpectrachordVoice::EnvState::reset(const Envelope& e, float sr) noexcept {
    shape=e; stage=EnvStage::Attack; value=0.0f; releaseStart=0.0f; pos=0;
    stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,e.attack)+0.001f,sr));
}
void SpectrachordVoice::EnvState::noteOff(float sr) noexcept {
    if(stage==EnvStage::Off||stage==EnvStage::Release)return;
    releaseStart=value;stage=EnvStage::Release;pos=0;
    stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,shape.release),sr));
}
float SpectrachordVoice::EnvState::next(float sr) noexcept {
    switch(stage){
        case EnvStage::Off:return 0.0f;
        case EnvStage::Attack:{
            value=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            if(pos>=stageSamples){value=1.0f;stage=EnvStage::Decay;pos=0;stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,shape.decay)+0.001f,sr));}
            break;
        }
        case EnvStage::Decay:{
            const float t=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            value=1.0f+(shape.sustain-1.0f)*std::min(1.0f,t);
            if(pos>=stageSamples){value=shape.sustain;stage=EnvStage::Sustain;}
            break;
        }
        case EnvStage::Sustain:value=shape.sustain;break;
        case EnvStage::Release:{
            const float t=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            value=releaseStart*(1.0f-std::min(1.0f,t));
            if(pos>=stageSamples||value<=0.0001f){value=0.0f;stage=EnvStage::Off;}
            break;
        }
    }
    return value;
}

void SpectrachordVoice::Biquad::configure(FilterType type,float freq,float q,float sr) noexcept {
    freq=std::clamp(freq,20.0f,sr*0.45f);q=std::max(0.05f,q);
    const float w0=kTwoPi*freq/sr,c=std::cos(w0),s=std::sin(w0),alpha=s/(2.0f*q);
    float nb0,nb1,nb2,na0,na1,na2;
    if(type==FilterType::Highpass){nb0=(1+c)*0.5f;nb1=-(1+c);nb2=(1+c)*0.5f;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    else if(type==FilterType::Bandpass){nb0=alpha;nb1=0;nb2=-alpha;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    else {nb0=(1-c)*0.5f;nb1=1-c;nb2=(1-c)*0.5f;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    const float inv=1.0f/std::max(kEps,na0);b0=nb0*inv;b1=nb1*inv;b2=nb2*inv;a1=na1*inv;a2=na2*inv;
}
float SpectrachordVoice::Biquad::process(float x) noexcept {
    const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;
}

float SpectrachordVoice::midiHz(float midi) noexcept { return 440.0f*std::pow(2.0f,(midi-69.0f)/12.0f); }
float SpectrachordVoice::clamp01(float v) noexcept { return std::clamp(v,0.0f,1.0f); }
void SpectrachordVoice::prepare(float sampleRate) noexcept { sampleRate_=std::max(8000.0f,sampleRate); }

void SpectrachordVoice::start(int32_t id,const Patch& patch,int midi,float velocity) noexcept {
    id_=id;patch_=patch;midi_=midi;velocity_=clamp01(velocity);active_=true;
    rng_^=static_cast<uint32_t>(id*747796405u + midi*2891336453u);
    phase_={};lastOp_={};lfoPhase_=0;lfoAge_=0;modValue_=0;filter_.reset();
    for(size_t i=0;i<6;++i)opEnv_[i].reset(patch_.ops[i].env,sampleRate_);
    ampEnv_.reset(patch_.amp,sampleRate_);filterEnv_.reset(patch_.filter.env,sampleRate_);
}
void SpectrachordVoice::release() noexcept {
    if(!active_)return;
    for(auto& e:opEnv_) e.noteOff(sampleRate_);
    ampEnv_.noteOff(sampleRate_);
    filterEnv_.noteOff(sampleRate_);
}
void SpectrachordVoice::kill() noexcept { active_=false;id_=-1;for(auto& e:opEnv_)e.stage=EnvStage::Off;ampEnv_.stage=EnvStage::Off; }

float SpectrachordVoice::noise() noexcept {
    uint32_t x=rng_;x^=x<<13;x^=x>>17;x^=x<<5;rng_=x;return (static_cast<float>(x)/2147483648.0f)-1.0f;
}

float SpectrachordVoice::slotValue(ModTarget target,float m,float fallback) const noexcept {
    for(size_t i=0;i<patch_.modSlotCount&&i<patch_.modSlots.size();++i){const auto&s=patch_.modSlots[i];if(s.target==target)return s.min+(s.max-s.min)*clamp01(m);}return fallback;
}
float SpectrachordVoice::opLevel(size_t index,float m) const noexcept {
    static constexpr std::array<ModTarget,6> T{ModTarget::Op1,ModTarget::Op2,ModTarget::Op3,ModTarget::Op4,ModTarget::Op5,ModTarget::Op6};
    return std::max(0.0f,slotValue(T[index],m,patch_.ops[index].level));
}
float SpectrachordVoice::morphValue(size_t index,float m) const noexcept {
    static constexpr std::array<ModTarget,6> T{ModTarget::Morph1,ModTarget::Morph2,ModTarget::Morph3,ModTarget::Morph4,ModTarget::Morph5,ModTarget::Morph6};
    return clamp01(slotValue(T[index],m,0.0f));
}

float SpectrachordVoice::waveSample(const Operator& op,float phase,float morph) noexcept {
    if(op.wave==Wave::Noise)return noise();
    if(op.wave==Wave::Saw)return phase/kPi-1.0f;
    if(op.wave==Wave::Square)return phase<kPi?1.0f:-1.0f;
    if(op.wave==Wave::Triangle)return 2.0f*std::fabs(2.0f*(phase/kTwoPi)-1.0f)-1.0f;
    if(op.wave==Wave::Custom&&op.hasHarm){
        float sum=0.0f,norm=0.0f;
        for(size_t i=0;i<16;++i){const float h=op.harm[i]*(1.0f-morph)+(op.hasHarmMute?op.harmMute[i]:op.harm[i])*morph;sum+=h*std::sin(phase*static_cast<float>(i+1));norm+=std::fabs(h);}
        return norm>1.0f?sum/norm:sum;
    }
    return std::sin(phase);
}

float SpectrachordVoice::render() noexcept {
    if(!active_)return 0.0f;
    const float pressure=velocity_;
    const float lvlScale=1.0f-patch_.velocityAmp+patch_.velocityAmp*(0.15f+0.85f*pressure*pressure);
    const float cutScale=1.0f-patch_.velocityFilter+patch_.velocityFilter*(0.4f+0.6f*pressure);
    const float m=modValue_;

    const float lfo=std::sin(lfoPhase_);
    lfoPhase_+=kTwoPi*std::max(0.05f,slotValue(ModTarget::LfoRate,m,patch_.lfo.rate))/sampleRate_;
    if(lfoPhase_>=kTwoPi)lfoPhase_-=kTwoPi;
    lfoAge_+=1.0f/sampleRate_;
    const float lfoFade=patch_.lfo.attack<=0?1.0f:std::min(1.0f,lfoAge_/patch_.lfo.attack);
    const float lfoVel=(1.0f-patch_.lfo.velocitySensitivity)+patch_.lfo.velocitySensitivity*pressure;
    const float lfoAmount=std::max(0.0f,slotValue(ModTarget::LfoAmount,m,patch_.lfo.amount))*lfoFade*lfoVel;
    const float pitchLfoCents=patch_.lfo.target==LfoTarget::Pitch?lfo*lfoAmount*60.0f:0.0f;

    const float baseMidi=static_cast<float>(midi_)+patch_.octave*12.0f;
    const float unison=clamp01(slotValue(ModTarget::Unison,m,patch_.unison));
    const int uniCount=unison>0.0001f?2:1;
    std::array<float,6> now{};
    for(size_t i=0;i<6;++i){
        const auto& op=patch_.ops[i];
        if(!op.enabled||op.level<=0.001f){opEnv_[i].next(sampleRate_);continue;}
        const float env=opEnv_[i].next(sampleRate_);
        const float level=opLevel(i,m)*env;
        float fmHz=0.0f;
        const float baseF=std::max(20.0f,midiHz(baseMidi)*op.ratio);
        const float fmScale=std::max(0.0f,slotValue(ModTarget::Fm,m,1.0f));
        for(size_t mod=0;mod<6;++mod)fmHz+=lastOp_[mod]*patch_.matrix[mod][i]*baseF*2.5f*fmScale;
        float opSum=0.0f;
        for(int u=0;u<uniCount;++u){
            const float uniCents=u?unison*12.0f:0.0f;
            const float cents=op.detuneCents+uniCents+pitchLfoCents;
            const float f=std::max(20.0f,(baseF+fmHz)*std::pow(2.0f,cents/1200.0f));
            float& ph=phase_[i][u];
            opSum+=waveSample(op,ph,morphValue(i,m));
            ph+=kTwoPi*f/sampleRate_;while(ph>=kTwoPi)ph-=kTwoPi;while(ph<0)ph+=kTwoPi;
        }
        now[i]=(opSum/static_cast<float>(uniCount))*level;
    }
    lastOp_=now;
    float sum=0.0f;for(float v:now)sum+=v;

    const float fEnv=filterEnv_.next(sampleRate_);
    const float envAmt=clamp01(slotValue(ModTarget::FilterEnv,m,patch_.filter.envAmount));
    float cutoff=slotValue(ModTarget::Cutoff,m,patch_.filter.cutoff)*cutScale + fEnv*envAmt*8000.0f;
    if(patch_.lfo.target==LfoTarget::Filter)cutoff+=lfo*lfoAmount*3000.0f;
    const float q=std::max(0.05f,slotValue(ModTarget::Resonance,m,patch_.filter.resonance));
    filter_.configure(patch_.filter.type,cutoff,q,sampleRate_);
    float filtered=filter_.process(sum);

    const float amp=ampEnv_.next(sampleRate_)*lvlScale;
    if(ampEnv_.done()){active_=false;return 0.0f;}
    float gain=std::max(0.0f,slotValue(ModTarget::Volume,m,1.0f));
    if(patch_.lfo.target==LfoTarget::Amp)gain*=std::max(0.0f,1.0f+lfo*lfoAmount*0.5f);
    return filtered*amp*gain*patch_.volume;
}

} // namespace aiora
