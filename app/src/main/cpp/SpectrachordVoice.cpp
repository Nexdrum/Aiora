#include "SpectrachordVoice.h"
#include <algorithm>
#include <cmath>

namespace aiora {
namespace {
constexpr float kPi=3.14159265358979323846f;
constexpr float kTwoPi=6.28318530717958647692f;
constexpr float kEps=1.0e-6f;
float secondsToSamples(float seconds,float sr) noexcept { return std::max(1.0f,seconds*sr); }
}

void SpectrachordVoice::EnvState::reset(const Envelope& e,float sr) noexcept {
    shape=e;stage=EnvStage::Attack;value=0;releaseStart=0;pos=0;
    stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,e.attack)+0.001f,sr));
}
void SpectrachordVoice::EnvState::noteOff(float sr) noexcept {
    if(stage==EnvStage::Off||stage==EnvStage::Release)return;
    releaseStart=value;stage=EnvStage::Release;pos=0;
    stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,shape.release),sr));
}
float SpectrachordVoice::EnvState::next(float sr) noexcept {
    switch(stage){
        case EnvStage::Off:return 0;
        case EnvStage::Attack:{
            value=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            if(pos>=stageSamples){value=1;stage=EnvStage::Decay;pos=0;stageSamples=static_cast<uint32_t>(secondsToSamples(std::max(0.0005f,shape.decay)+0.001f,sr));}
            break;
        }
        case EnvStage::Decay:{
            const float t=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            value=1+(shape.sustain-1)*std::min(1.0f,t);
            if(pos>=stageSamples){value=shape.sustain;stage=EnvStage::Sustain;}
            break;
        }
        case EnvStage::Sustain:value=shape.sustain;break;
        case EnvStage::Release:{
            const float t=static_cast<float>(++pos)/static_cast<float>(std::max(1u,stageSamples));
            value=releaseStart*(1-std::min(1.0f,t));
            if(pos>=stageSamples||value<=0.0001f){value=0;stage=EnvStage::Off;}
            break;
        }
    }
    return value;
}

void SpectrachordVoice::PointEnvState::reset(const OperatorCurve& c) noexcept {
    curve=c;age=0;releaseAge=0;releaseStart=0;value=0;releasing=false;done=false;
}
void SpectrachordVoice::PointEnvState::noteOff() noexcept {
    if(releasing||done||!curve.enabled||!curve.count)return;
    releaseStart=value;releaseAge=0;releasing=true;
}
float SpectrachordVoice::PointEnvState::next(float sr) noexcept {
    if(done||!curve.enabled||!curve.count)return 0;
    const auto* pts=curve.points.data();
    const int last=static_cast<int>(curve.count)-1;
    const int pivot=std::clamp(static_cast<int>(curve.releaseIndex),0,last);
    const float dt=1.0f/std::max(8000.0f,sr);
    if(releasing){
        const float total=pts[last].time-pts[pivot].time;
        if(total<=0||releaseAge>=total){done=true;value=0;return 0;}
        // Release is relative to note-off: the current value replaces the
        // drawn release start point, and the remaining segment timings survive.
        OperatorPoint start=pts[pivot];start.time=0;start.value=releaseStart;
        value=releaseStart;
        for(int i=pivot+1;i<=last;++i){
            const float endT=std::max(0.0f,pts[i].time-pts[pivot].time);
            if(releaseAge<=endT){
                OperatorPoint end=pts[i];end.time=endT;
                const float beginT=i==pivot+1?0.0f:pts[i-1].time-pts[pivot].time;
                if(i>pivot+1){start=pts[i-1];start.time=beginT;}
                OperatorPoint segment[2]{start,end};
                value=operatorCurveValue(segment,2,releaseAge,releaseStart);
                break;
            }
        }
        releaseAge+=dt;
    }else{
        const float limit=curve.sustain?pts[pivot].time:pts[last].time;
        value=operatorCurveValue(pts,curve.count,std::min(age,limit),0);
        if(!curve.sustain&&age>=pts[last].time){value=0;done=true;}
        age+=dt;
    }
    return value;
}
void SpectrachordVoice::Biquad::configure(FilterType type,float freq,float q,float sr) noexcept {
    freq=std::clamp(freq,20.0f,sr*0.45f);q=std::max(0.05f,q);
    const float w0=kTwoPi*freq/sr,c=std::cos(w0),s=std::sin(w0),alpha=s/(2*q);
    float nb0,nb1,nb2,na0,na1,na2;
    if(type==FilterType::Highpass){nb0=(1+c)*0.5f;nb1=-(1+c);nb2=(1+c)*0.5f;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    else if(type==FilterType::Bandpass){nb0=alpha;nb1=0;nb2=-alpha;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    else{nb0=(1-c)*0.5f;nb1=1-c;nb2=(1-c)*0.5f;na0=1+alpha;na1=-2*c;na2=1-alpha;}
    const float inv=1/std::max(kEps,na0);b0=nb0*inv;b1=nb1*inv;b2=nb2*inv;a1=na1*inv;a2=na2*inv;
}
void SpectrachordVoice::Biquad::configurePeak(float freq,float q,float gainDb,float sr) noexcept {
    // RBJ Audio EQ Cookbook peaking EQ, with a0 normalization.
    freq=std::clamp(freq,20.0f,sr*.45f);
    q=std::clamp(q,.3f,12.0f);
    const float a=std::pow(10.0f,std::clamp(gainDb,-18.0f,18.0f)/40.0f);
    const float w0=kTwoPi*freq/sr,c=std::cos(w0),alpha=std::sin(w0)/(2.0f*q);
    const float na0=1.0f+alpha/a,inv=1.0f/na0;
    b0=(1.0f+alpha*a)*inv;
    b1=(-2.0f*c)*inv;
    b2=(1.0f-alpha*a)*inv;
    a1=(-2.0f*c)*inv;
    a2=(1.0f-alpha/a)*inv;
}
float SpectrachordVoice::Biquad::process(float x) noexcept {const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}

float SpectrachordVoice::midiHz(float midi) noexcept {return 440*std::pow(2.0f,(midi-69)/12.0f);}
float SpectrachordVoice::clamp01(float v) noexcept {return std::clamp(v,0.0f,1.0f);}
float SpectrachordVoice::curveValue(const DspCurve& c,float step,float fallback) noexcept {
    if(c.count==0)return fallback;
    if(c.count==1||step<=c.points[0].step)return c.points[0].value;
    const auto* end=c.points+c.count;
    const auto* b=std::lower_bound(c.points+1,end,step,
        [](const DspCurvePoint& p,float t){return p.step<t;});
    if(b==end)return c.points[c.count-1].value;
    const auto& a=*(b-1);
    const float k=(step-a.step)/std::max(1.0e-6f,b->step-a.step);
    return curveSegment(k,a.value,b->value,b->midpoint,b->handleActive,b->handleValue);
}
float SpectrachordVoice::curveMean(const DspCurve& c,float fallback) noexcept {
    if(c.count==0)return fallback;float sum=0;for(size_t i=0;i<c.count;++i)sum+=c.points[i].value;return sum/static_cast<float>(c.count);
}
void SpectrachordVoice::prepare(float sr) noexcept {sampleRate_=std::max(8000.0f,sr);}

float SpectrachordVoice::mappedValue(ModTarget target,float m,float fallback) const noexcept {
    for(size_t i=0;patch_.modSlots&&i<patch_.modSlotCount&&i<patch_.modSlots->size();++i){const auto&s=(*patch_.modSlots)[i];if(s.target==target)return s.min+(s.max-s.min)*clamp01(m);}return fallback;
}
bool SpectrachordVoice::velocityOwns(ModTarget target) const noexcept {
    for(size_t i=0;patch_.velocitySlots&&i<patch_.velocitySlotCount&&i<patch_.velocitySlots->size();++i)
        if((*patch_.velocitySlots)[i].target==target)return true;
    return false;
}
float SpectrachordVoice::slotValue(ModTarget target,float v,float m,float fallback) const noexcept {
    // Existing M routes retain priority if malformed input assigns both sources.
    if(modActive_ && patch_.modSlots)
        for(size_t i=0;i<patch_.modSlotCount&&i<patch_.modSlots->size();++i)
            if((*patch_.modSlots)[i].target==target)return mappedValue(target,m,fallback);
    if(patch_.velocitySlots)
        for(size_t i=0;i<patch_.velocitySlotCount&&i<patch_.velocitySlots->size();++i){
            const auto& s=(*patch_.velocitySlots)[i];
            if(s.target==target)return s.min+(s.max-s.min)*clamp01(v);
        }
    return fallback;
}

void SpectrachordVoice::start(int32_t id,const DspPatch& patch,int midi,float pressure,uint64_t gateSamples,float samplesPerStep,const VoiceAutomation& automation) noexcept {
    id_=id;patch_=patch;midi_=midi;velocity_=clamp01(pressure);active_=true;automation_=automation;
    gateSamples_=gateSamples;autoRelease_=gateSamples>0;elapsedSamples_=0;samplesPerStep_=std::max(1.0f,samplesPerStep);
    modActive_=automation_.mod.count>0;velocityCurveActive_=automation_.velocity.count>0;
    const float m0=curveValue(automation_.mod,0,0),mEnd=automation_.mod.count?automation_.mod.points[automation_.mod.count-1].value:0,mMean=curveMean(automation_.mod,0);
    const float v0=clamp01(curveValue(automation_.velocity,0,velocity_));
    const float vMean=clamp01(curveMean(automation_.velocity,velocity_));
    const float vEnd=automation_.velocity.count?clamp01(automation_.velocity.points[automation_.velocity.count-1].value):velocity_;
    modValue_=m0;timpaniRingGain_=1.0f;
    fixedUnison_=clamp01(slotValue(ModTarget::Unison,vMean,mMean,patch_.unison));
    fixedFilterEnvAmount_=clamp01(slotValue(ModTarget::FilterEnv,vMean,mMean,patch_.filter.envAmount));
    rng_^=static_cast<uint32_t>(id*747796405u+midi*2891336453u);phase_={};lastOp_={};lfoPhase_=0;lfoAge_=0;filter_.reset();peak_.reset();peakReady_=false;
    // Every operator can select Hiss. Its pitch-responsive noise filter
    // has independent state; ordinary Noise always remains raw white noise.
    for(auto& pair:hissState_)pair.fill(0.0f);
    cymbalTailGain_=1.0f;
    // Strong crash hits retain chaotic energy longer than light taps,
    // but the mixed metallic tail remains short and controlled.
    const float crashTailSeconds=.19f+.14f*std::pow(v0,1.4f);
    cymbalTailMultiplier_=patch_.cymbalModel
        ?std::exp(-1.0f/(sampleRate_*(patch_.cymbalModel==2?.34f:crashTailSeconds)))
        :1.0f;
    platePhase_.fill(0);plateGain_.fill(0);plateDecay_.fill(1);
    if(patch_.cymbalModel){
        // Flexing metal plates produce unrelated modal frequencies and decay times.
        constexpr std::array<float,12> crashStrength{{
            .065f,.057f,.069f,.050f,.056f,.040f,.043f,.033f,.026f,.022f,.016f,.012f}};
        constexpr std::array<float,12> rideStrength{{
            .066f,.085f,.095f,.087f,.074f,.051f,.034f,.021f,.014f,.009f,.007f,.004f}};
        const bool ride=patch_.cymbalModel==2;
        for(size_t i=0;i<plateGain_.size();++i){
            rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;
            platePhase_[i]=static_cast<float>(rng_&65535u)*(kTwoPi/65536.0f);
            const float variation=static_cast<float>((rng_>>16)&255u)/255.0f;
            // A light stick tap predominantly excites low, bell-like modes.
            // Harder strikes progressively excite the upper modes as well.
            const float band=static_cast<float>(i)/static_cast<float>(plateGain_.size()-1);
            const float low=ride?(.65f+.35f*v0):(.48f+.52f*v0);
            const float high=ride?(.35f+.65f*v0):(.20f+.80f*v0);
            const float excitation=low+(high-low)*band;
            const float strength=ride?rideStrength[i]:crashStrength[i];
            plateGain_[i]=strength*(0.82f+0.36f*variation)*excitation;
            // Close-miked/mix-gated voicing: low metal modes linger briefly
            // but upper ringing is suppressed rapidly, especially in the crash.
            const float seconds=(ride?1.25f:.72f)/
                (1.0f+(ride?0.13f:0.18f)*static_cast<float>(i));
            plateDecay_[i]=std::exp(-1.0f/(sampleRate_*seconds));
        }
    }
    anyCustomVolume_=false;
    for(size_t i=0;i<6;++i){
        opVolume_[i].reset(patch_.ops[i].volumeCurve);
        opPitch_[i].reset(patch_.ops[i].pitchCurve);
        anyCustomVolume_|=patch_.ops[i].enabled&&patch_.ops[i].volumeCurve.enabled&&patch_.ops[i].volumeCurve.count>1;
        Envelope shape=patch_.ops[i].env;
        // Per-operator decay links are the editable replacement for the
        // old crash-only velocity envelope. Sample at note-on as with ADSR.
        const auto target=static_cast<ModTarget>(static_cast<int>(ModTarget::Op1Decay)+static_cast<int>(i));
        shape.decay=std::max(.005f,slotValue(target,v0,mMean,shape.decay));
        opEnv_[i].reset(shape,sampleRate_);
    }
    Envelope a=patch_.amp,f=patch_.filter.env;
    a.attack=std::max(0.0005f,slotValue(ModTarget::AmpAttack,v0,m0,a.attack));
    a.decay=std::max(0.0005f,slotValue(ModTarget::AmpDecay,vMean,mMean,a.decay));
    a.sustain=clamp01(slotValue(ModTarget::AmpSustain,vMean,mMean,a.sustain));
    a.release=std::max(0.0005f,slotValue(ModTarget::AmpRelease,vEnd,mEnd,a.release));
    f.attack=std::max(0.0005f,slotValue(ModTarget::FilterAttack,v0,m0,f.attack));
    f.decay=std::max(0.0005f,slotValue(ModTarget::FilterDecay,vMean,mMean,f.decay));
    f.sustain=clamp01(slotValue(ModTarget::FilterSustain,vMean,mMean,f.sustain));
    f.release=std::max(0.0005f,slotValue(ModTarget::FilterRelease,vEnd,mEnd,f.release));
    ampEnv_.reset(a,sampleRate_);filterEnv_.reset(f,sampleRate_);
}
void SpectrachordVoice::release() noexcept {
    if(!active_)return;
    autoRelease_=false;
    for(size_t i=0;i<6;++i){opEnv_[i].noteOff(sampleRate_);opVolume_[i].noteOff();opPitch_[i].noteOff();}
    ampEnv_.noteOff(sampleRate_);filterEnv_.noteOff(sampleRate_);
}
void SpectrachordVoice::kill() noexcept {active_=false;id_=-1;autoRelease_=false;for(auto&e:opEnv_)e.stage=EnvStage::Off;ampEnv_.stage=EnvStage::Off;}

float SpectrachordVoice::noise() noexcept {uint32_t x=rng_;x^=x<<13;x^=x>>17;x^=x<<5;rng_=x;return static_cast<float>(x)/2147483648.0f-1.0f;}
float SpectrachordVoice::hissSample(size_t index,int unisonVoice,float pitchHz) noexcept {
    // Lowpass-shaped white noise: pitch, ratio, semitone, bend and detune
    // set the spectral bandwidth. It works on ANY patch, not cymbals only.
    // The coefficient is algebraic for cheap, continuous pitch response.
    const float cutoff=std::clamp(pitchHz*8.5f,100.0f,sampleRate_*0.42f);
    const float omega=kTwoPi*cutoff;
    const float alpha=omega/(omega+sampleRate_);
    float& filtered=hissState_[index][static_cast<size_t>(unisonVoice)];
    filtered+=alpha*(noise()-filtered);
    return filtered;
}
float SpectrachordVoice::opLevel(size_t i,float v,float m) const noexcept {
    static constexpr std::array<ModTarget,6>T{ModTarget::Op1,ModTarget::Op2,ModTarget::Op3,ModTarget::Op4,ModTarget::Op5,ModTarget::Op6};
    return std::max(0.0f,slotValue(T[i],v,m,patch_.ops[i].level));
}
float SpectrachordVoice::morphValue(size_t i,float v,float m) const noexcept {
    static constexpr std::array<ModTarget,6>T{ModTarget::Morph1,ModTarget::Morph2,ModTarget::Morph3,ModTarget::Morph4,ModTarget::Morph5,ModTarget::Morph6};
    return clamp01(slotValue(T[i],v,m,0));
}
float SpectrachordVoice::waveSample(const Operator& op,float phase,float morph) noexcept {
    if(op.wave==Wave::Noise)return noise();
    if(op.wave==Wave::Saw)return phase/kPi-1;
    if(op.wave==Wave::Square)return phase<kPi?1.0f:-1.0f;
    if(op.wave==Wave::Triangle)return 2*std::fabs(2*(phase/kTwoPi)-1)-1;
    if(op.wave==Wave::Custom&&op.hasHarm){float sum=0,norm=0;for(size_t i=0;i<16;++i){const float h=op.harm[i]*(1-morph)+(op.hasHarmMute?op.harmMute[i]:op.harm[i])*morph;sum+=h*std::sin(phase*static_cast<float>(i+1));norm+=std::fabs(h);}return norm>1?sum/norm:sum;}
    return std::sin(phase);
}

float SpectrachordVoice::render() noexcept {
    if(!active_)return 0;
    if(autoRelease_&&elapsedSamples_>=gateSamples_)release();
    const float step=static_cast<float>(elapsedSamples_)/samplesPerStep_;
    if(modActive_)modValue_=clamp01(curveValue(automation_.mod,step,modValue_));
    const float bend=curveValue(automation_.bend,step,0);
    const float velNow=velocityCurveActive_?clamp01(curveValue(automation_.velocity,step,velocity_)):1.0f;
    const float velocityGain=velocityCurveActive_?velNow*velNow:1.0f;
    const DspPatch&P=patch_;const float pressure=velocity_;
    const float lvlScale=1-P.velocityAmp+P.velocityAmp*(0.15f+0.85f*pressure*pressure);
    const float cutScale=velocityOwns(ModTarget::Cutoff)?1.0f:1-P.velocityFilter+P.velocityFilter*(0.4f+0.6f*pressure);
    const float m=modValue_;
    const float expressionV=velocityCurveActive_?velNow:velocity_;
    // Smooth post-strike loss of metallic energy brings cymbals beneath
    // a dense metal mix; M-controlled chokes still act independently.
    if(P.cymbalModel && elapsedSamples_>static_cast<uint64_t>(sampleRate_*.065f))
        cymbalTailGain_*=cymbalTailMultiplier_;
    // First ~65 ms: M sets mallet attack spectral hardness. Thereafter
    // the SAME M line represents player-applied felt/head damping.
    // Damping is cumulative and irreversible, as with a real drumhead.
    if(P.timpaniMembrane && modActive_ && elapsedSamples_>static_cast<uint32_t>(sampleRate_*.065f)){
        const float damping=std::max(0.0f,m);
        timpaniRingGain_*=std::exp(-damping*10.0f/sampleRate_);
    }

    const float lfo=std::sin(lfoPhase_);lfoPhase_+=kTwoPi*std::max(0.05f,slotValue(ModTarget::LfoRate,expressionV,m,P.lfo.rate))/sampleRate_;if(lfoPhase_>=kTwoPi)lfoPhase_-=kTwoPi;
    lfoAge_+=1/sampleRate_;const float lfoFade=P.lfo.attack<=0?1:std::min(1.0f,lfoAge_/P.lfo.attack);const float lfoVel=(1-P.lfo.velocitySensitivity)+P.lfo.velocitySensitivity*pressure;
    const float lfoAmount=std::max(0.0f,slotValue(ModTarget::LfoAmount,expressionV,m,P.lfo.amount))*lfoFade*lfoVel;const float pitchLfoCents=P.lfo.target==LfoTarget::Pitch?lfo*lfoAmount*60:0;

    const float baseMidi=static_cast<float>(midi_)+P.octave*12+bend;const float unison=clamp01(fixedUnison_);const int uniCount=unison>0.0001f?2:1;
    // A brief beater-driven membrane sweep settles at the unchanged note fundamental.
    const float pitchDrop=(P.drumPitchDrop>0.0f&&P.drumPitchDropSeconds>0.0f)
        ?P.drumPitchDrop*std::exp(-static_cast<float>(elapsedSamples_)/(sampleRate_*P.drumPitchDropSeconds)):0.0f;
    std::array<float,6>now{};
    for(size_t i=0;i<6;++i){
        const auto&op=P.ops[i];if(!op.enabled){opEnv_[i].next(sampleRate_);continue;}
        const float oldEnv=opEnv_[i].next(sampleRate_);
        const float env=op.volumeCurve.enabled&&op.volumeCurve.count>1
            ?opVolume_[i].next(sampleRate_):oldEnv;
        const float pitchEnv=op.pitchCurve.enabled&&op.pitchCurve.count>1
            ?opPitch_[i].next(sampleRate_):0.0f;
        float response=1.0f;
        const auto&expr=op.expressionCurve;
        if(expr.enabled&&expr.count>1){
            const bool supplied=expr.source==0?velocityCurveActive_||velocity_>=0:modActive_;
            if(supplied){
                const float in=expr.source==0?expressionV:m;
                response=std::clamp(operatorCurveValue(expr.points.data(),expr.count,
                    std::clamp(in,0.0f,1.0f),1),0.0f,1.0f);
            }
        }
        const float level=opLevel(i,expressionV,m)*env*response;
        if(level<=0.000001f)continue;
        const float semitoneScale=std::pow(2.0f,op.semitoneOffset/12.0f);
        float fmHz=0;
        // Bend has no artificial semitone ceiling. The physical output
        // oscillator must still remain finite and below Nyquist.
        const float unboundedBase=midiHz(baseMidi+(i<2?pitchDrop:0.0f)+pitchEnv)*op.ratio*semitoneScale;
        const float maxHz=sampleRate_*0.49f;
        const float baseF=std::isfinite(unboundedBase)
            ?std::clamp(unboundedBase,0.001f,maxHz):maxHz;
        const float fmScale=std::max(0.0f,slotValue(ModTarget::Fm,expressionV,m,1));
        for(size_t mod=0;mod<6;++mod)
            fmHz+=lastOp_[mod]*P.matrix[mod][i]*baseF*2.5f*fmScale;
        float opSum=0;
        for(int u=0;u<uniCount;++u){
            const float uniCents=u?unison*12:0;
            const float cents=op.detuneCents+uniCents+pitchLfoCents;
            const float unboundedHz=(baseF+fmHz)*std::pow(2.0f,cents/1200.0f);
            const float f=std::isfinite(unboundedHz)
                ?std::clamp(unboundedHz,0.001f,maxHz):maxHz;
            float&ph=phase_[i][u];
            opSum+=op.wave==Wave::Hiss
                ?hissSample(i,u,f)
                :waveSample(op,ph,morphValue(i,expressionV,m));
            ph+=kTwoPi*f/sampleRate_;
            if(ph>=kTwoPi)ph-=kTwoPi;
        }
        now[i]=opSum/static_cast<float>(uniCount)*level;
    }
    lastOp_=now;float sum=0;for(float v:now)sum+=v;
    if(P.cymbalModel){
        // Independent inharmonic modes add the evolving ring missing from
        // filtered white-noise cymbal approximations.
        constexpr std::array<float,12> crashRatios{{
            .41f,.67f,1.03f,1.48f,2.09f,2.91f,3.79f,4.67f,5.83f,7.21f,8.64f,10.2f}};
        constexpr std::array<float,12> rideRatios{{
            .31f,.52f,.81f,1.14f,1.60f,2.17f,2.79f,3.46f,4.38f,5.30f,6.92f,8.40f}};
        const auto& ratios=P.cymbalModel==2?rideRatios:crashRatios;
        const float fundamental=midiHz(baseMidi);
        const float nyquist=sampleRate_*0.48f;
        float plate=0.0f;
        for(size_t i=0;i<ratios.size();++i){
            const float frequency=fundamental*ratios[i];
            if(frequency<nyquist){
                plate+=std::sin(platePhase_[i])*plateGain_[i];
                platePhase_[i]+=kTwoPi*frequency/sampleRate_;
                if(platePhase_[i]>=kTwoPi)platePhase_[i]-=kTwoPi;
            }
            plateGain_[i]*=plateDecay_[i];
        }
        sum+=plate*(P.cymbalModel==2?0.94f:1.0f);
    }
    // A separate, optional bell EQ runs once per voice, BEFORE the primary
    // subtractive filter. Pitch tracking uses the known MIDI fundamental,
    // including pitch bend; no audio pitch detection is necessary.
    if(P.peak.enabled){
        const float center=P.peak.followNote
            ?midiHz(baseMidi)*std::clamp(slotValue(ModTarget::PeakRatio,expressionV,m,P.peak.ratio),.5f,8.0f)
            :slotValue(ModTarget::PeakFrequency,expressionV,m,P.peak.frequency);
        const float targetCenter=std::clamp(center,20.0f,sampleRate_*.45f);
        const float targetQ=std::clamp(slotValue(ModTarget::PeakQ,expressionV,m,P.peak.q),.3f,12.0f);
        const float targetGain=std::clamp(slotValue(ModTarget::PeakGain,expressionV,m,P.peak.gainDb),-18.0f,18.0f);
        if(!peakReady_){
            peakCenterSmoothed_=targetCenter;peakQSmoothed_=targetQ;
            peakGainSmoothed_=targetGain;peakReady_=true;
        }else{
            // Four-millisecond coefficient motion avoids zipper noise when
            // M or a live pitch bend moves the resonant center.
            const float t=std::min(1.0f,1.0f/(sampleRate_*.004f));
            peakCenterSmoothed_+=(targetCenter-peakCenterSmoothed_)*t;
            peakQSmoothed_+=(targetQ-peakQSmoothed_)*t;
            peakGainSmoothed_+=(targetGain-peakGainSmoothed_)*t;
        }
        peak_.configurePeak(peakCenterSmoothed_,peakQSmoothed_,peakGainSmoothed_,sampleRate_);
        sum=peak_.process(sum);
    }
    const float fEnv=filterEnv_.next(sampleRate_);float cutoff=slotValue(ModTarget::Cutoff,expressionV,m,P.filter.cutoff)*cutScale+fEnv*fixedFilterEnvAmount_*8000;
    if(P.lfo.target==LfoTarget::Filter)cutoff+=lfo*lfoAmount*3000;const float q=std::max(0.05f,slotValue(ModTarget::Resonance,expressionV,m,P.filter.resonance));filter_.configure(P.filter.type,cutoff,q,sampleRate_);const float filtered=filter_.process(sum);
    const float ampLegacy=ampEnv_.next(sampleRate_)*lvlScale;
    if(!anyCustomVolume_&&ampEnv_.done()){active_=false;return 0;}
    bool anyOperatorAlive=false;
    for(size_t i=0;i<6;++i){
        if(!P.ops[i].enabled||P.ops[i].level<=0)continue;
        anyOperatorAlive|=(P.ops[i].volumeCurve.enabled&&P.ops[i].volumeCurve.count>1)
            ?!opVolume_[i].done:!opEnv_[i].done();
    }
    if(anyCustomVolume_&&!anyOperatorAlive){active_=false;return 0;}
    const float amp=anyCustomVolume_?1.0f:ampLegacy;
    float gain=std::max(0.0f,slotValue(ModTarget::Volume,expressionV,m,1));if(P.lfo.target==LfoTarget::Amp)gain*=std::max(0.0f,1+lfo*lfoAmount*0.5f);
    ++elapsedSamples_;return filtered*amp*gain*P.volume*velocityGain*(P.timpaniMembrane?timpaniRingGain_:1.0f)*(P.cymbalModel?cymbalTailGain_:1.0f);
}

} // namespace aiora
