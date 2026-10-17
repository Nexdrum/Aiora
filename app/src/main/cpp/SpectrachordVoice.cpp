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

// Morphing a filter type means CROSSFADE, not running two effects
// serially or summing two full-volume outputs.
std::array<float,3> evolvingShapeWeights(
    const std::array<FilterShape,kMaxOperatorPoints>& shapes,
    const std::array<float,kMaxOperatorPoints>& transitions,
    const OperatorPoint* points,int count,float position) noexcept {
    std::array<float,3> weights{};
    if(count<1){weights[1]=1;return weights;}
    const auto shapeIndex=[&](int i){
        return std::clamp(static_cast<int>(shapes[static_cast<size_t>(i)]),0,2);
    };
    if(position<=points[0].time){
        weights[shapeIndex(0)]=1;return weights;
    }
    for(int i=1;i<count;++i){
        if(position<=points[i].time){
            const int a=shapeIndex(i-1),b=shapeIndex(i);
            if(a==b){weights[a]=1;return weights;}
            const float begin=points[i-1].time+
                (points[i].time-points[i-1].time)*
                std::clamp(transitions[static_cast<size_t>(i)],0.0f,.95f);
            const float f=std::clamp((position-begin)/
                std::max(.000001f,points[i].time-begin),0.0f,1.0f);
            weights[a]=1-f;weights[b]=f;return weights;
        }
    }
    weights[shapeIndex(count-1)]=1;return weights;
}
float evolvingFrequency(float norm) noexcept {
    return 40.0f*std::pow(450.0f,std::clamp(norm,0.0f,1.0f));
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

void SpectrachordVoice::PointEnvState::reset(const OperatorCurve& c,bool retainFinal) noexcept {
    curve=c;age=0;releaseAge=0;releaseStart=0;value=0;releasing=false;done=false;
    holdLast=retainFinal;
}
void SpectrachordVoice::PointEnvState::noteOff() noexcept {
    // Note-off always begins release, whether sustain is infinite
    // or the timeline is still advancing toward its timed release gate.
    if(releasing||done||!curve.enabled||!curve.count)return;
    releaseStart=value;releaseAge=0;releasing=true;
}
float SpectrachordVoice::PointEnvState::next(float sr) noexcept {
    if(done||!curve.enabled||!curve.count)
        return holdLast&&curve.count?curve.points[curve.count-1].value:0.0f;
    const auto* pts=curve.points.data();
    const int last=static_cast<int>(curve.count)-1;
    const int pivot=std::clamp(static_cast<int>(curve.releaseIndex),0,last);
    const float dt=1.0f/std::max(8000.0f,sr);
    // Infinite sustain pauses the timeline at the S marker. Dynamic
    // sustain follows the authored shape up to R, then automatically
    // enters the very same release used by an early note-off.
    if(!releasing&&!curve.sustain&&age>=pts[pivot].time){
        value=operatorCurveValue(pts,curve.count,pts[pivot].time,0);
        noteOff();
    }
    if(releasing){
        const float total=pts[last].time-pts[pivot].time;
        if(total<=0||releaseAge>=total){
            done=true;value=holdLast?pts[last].value:0;
            return value;
        }
        // Evaluate the complete authored release curve (including X
        // handles and intermediate points), then smoothly offset it
        // from its authored start to this note's actual release value.
        // The offset fades to zero at the terminal point: early
        // note-off cannot jump toward a later node's absolute value.
        const float authoredTime=pts[pivot].time+releaseAge;
        const float authored=operatorCurveValue(pts+pivot,
            static_cast<size_t>(last-pivot+1),authoredTime,pts[pivot].value);
        const float remaining=1.0f-releaseAge/total;
        value=authored+(releaseStart-pts[pivot].value)*remaining;
        releaseAge+=dt;
    }else{
        value=operatorCurveValue(pts,curve.count,
            std::min(age,pts[pivot].time),0);
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
// RBJ bell/low-shelf/high-shelf filters. Q means bell bandwidth for Bell,
// or slope for the two shelving types. All three paths use the same sampled
// input and are crossfaded with normalized weights; they never cascade.
void SpectrachordVoice::Biquad::configureEvolving(
    FilterShape shape,float freq,float qNorm,float gainDb,float sr) noexcept {
    freq=std::clamp(freq,20.0f,sr*.45f);
    qNorm=std::clamp(qNorm,0.0f,1.0f);
    gainDb=std::clamp(gainDb,-24.0f,24.0f);
    const float A=std::pow(10.0f,gainDb/40.0f);
    const float w=kTwoPi*freq/sr,c=std::cos(w),sn=std::sin(w);
    float nb0=1,nb1=0,nb2=0,na0=1,na1=0,na2=0;
    if(shape==FilterShape::Bell){
        const float Q=.3f*std::pow(40.0f,qNorm);
        const float alpha=sn/(2.0f*Q);
        nb0=1+alpha*A;nb1=-2*c;nb2=1-alpha*A;
        na0=1+alpha/A;na1=-2*c;na2=1-alpha/A;
    }else{
        const float S=.15f+.85f*qNorm;
        const float alpha=sn*.5f*std::sqrt(std::max(.0001f,(A+1/A)*(1/S-1)+2));
        const float root=2*std::sqrt(A)*alpha;
        if(shape==FilterShape::LowShelf){
            nb0=A*((A+1)-(A-1)*c+root);
            nb1=2*A*((A-1)-(A+1)*c);
            nb2=A*((A+1)-(A-1)*c-root);
            na0=(A+1)+(A-1)*c+root;
            na1=-2*((A-1)+(A+1)*c);
            na2=(A+1)+(A-1)*c-root;
        }else{
            nb0=A*((A+1)+(A-1)*c+root);
            nb1=-2*A*((A-1)+(A+1)*c);
            nb2=A*((A+1)+(A-1)*c-root);
            na0=(A+1)-(A-1)*c+root;
            na1=2*((A-1)-(A+1)*c);
            na2=(A+1)-(A-1)*c-root;
        }
    }
    const float inv=1.0f/std::max(1e-9f,na0);
    b0=nb0*inv;b1=nb1*inv;b2=nb2*inv;
    a1=na1*inv;a2=na2*inv;
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
    if(patch_.modSlots)
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
    // M is always supplied; an unedited line is explicitly M=0, never
    // a request to bypass an operator or FX modulation route.
    modActive_=true;velocityCurveActive_=automation_.velocity.count>0;
    const float m0=curveValue(automation_.mod,0,0),mEnd=automation_.mod.count?automation_.mod.points[automation_.mod.count-1].value:0,mMean=curveMean(automation_.mod,0);
    const float v0=clamp01(curveValue(automation_.velocity,0,velocity_));
    const float vMean=clamp01(curveMean(automation_.velocity,velocity_));
    const float vEnd=automation_.velocity.count?clamp01(automation_.velocity.points[automation_.velocity.count-1].value):velocity_;
    modValue_=m0;timpaniRingGain_=1.0f;
    if(patch_.masterAmpCurve.enabled&&patch_.masterAmpCurve.count>=2)
        masterAmpTimeline_.reset(patch_.masterAmpCurve,false);
    fixedUnison_=clamp01(slotValue(ModTarget::Unison,vMean,mMean,patch_.unison));
    if(patch_.unisonCurve.enabled&&patch_.unisonCurve.count>=2){
        unisonTimeline_.reset(patch_.unisonCurve,true);
        unisonSmoothed_=std::clamp(patch_.unisonCurve.points[0].value,0.0f,1.0f);
    }else unisonSmoothed_=fixedUnison_;
    fixedFilterEnvAmount_=clamp01(slotValue(ModTarget::FilterEnv,vMean,mMean,patch_.filter.envAmount));
    rng_^=static_cast<uint32_t>(id*747796405u+midi*2891336453u);phase_={};lastOp_={};lfoPhase_=0;lfoAge_=0;filter_.reset();peak_.reset();peakReady_=false;
    if(patch_.filter.evolving.configured){
        for(int k=0;k<6;++k)
            filterTimeline_[k].reset(patch_.filter.evolving.lanes[k].curve,true);
        for(auto& b:evolvingFilterBiquads_)b.reset();
        for(auto& bank:slopeLowpasses_)for(auto& b:bank)b.reset();
        slopeGainSmoothed_.fill(1.0f);slopeGainTarget_.fill(1.0f);
        slopeUpdateCounter_=0;
        filterFrequencySmoothed_=evolvingFrequency(patch_.filter.evolving.lanes[0].curve.points[0].value);
        filterGainSmoothed_=(patch_.filter.evolving.lanes[1].curve.points[0].value*2-1)*24;
        filterQSmoothed_=patch_.filter.evolving.lanes[2].curve.points[0].value;
        filterShapeSmoothed_={0,1,0};
    }
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
        opMorphVolume_[i].reset(patch_.ops[i].volumeMorphCurve);
        opMorphPitch_[i].reset(patch_.ops[i].pitchMorphCurve);
        anyCustomVolume_|=patch_.ops[i].enabled&&
            ((patch_.ops[i].volumeCurve.enabled&&patch_.ops[i].volumeCurve.count>1)||
            (patch_.ops[i].volumeMorphCurve.enabled&&patch_.ops[i].volumeMorphCurve.count>1));
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
    for(size_t i=0;i<6;++i){
        opEnv_[i].noteOff(sampleRate_);opVolume_[i].noteOff();opPitch_[i].noteOff();
        opMorphVolume_[i].noteOff();opMorphPitch_[i].noteOff();
    }
    if(patch_.filter.evolving.configured)
        for(auto& env:filterTimeline_)env.noteOff();
    if(patch_.unisonCurve.enabled&&patch_.unisonCurve.count>=2)
        unisonTimeline_.noteOff();
    if(patch_.masterAmpCurve.enabled&&patch_.masterAmpCurve.count>=2)
        masterAmpTimeline_.noteOff();
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

    const bool dynamicUnison=P.unisonCurve.enabled&&P.unisonCurve.count>=2;
    const float unisonTarget=dynamicUnison?
        clamp01(unisonTimeline_.next(sampleRate_)):clamp01(fixedUnison_);
    const float smoothing=1.0f-std::exp(-1.0f/(sampleRate_*.004f));
    unisonSmoothed_+=(unisonTarget-unisonSmoothed_)*smoothing;
    const float baseMidi=static_cast<float>(midi_)+P.octave*12+bend;
    const float unison=clamp01(unisonSmoothed_);
    const int uniCount=unison>0.0001f?2:1;
    // A brief beater-driven membrane sweep settles at the unchanged note fundamental.
    const float pitchDrop=(P.drumPitchDrop>0.0f&&P.drumPitchDropSeconds>0.0f)
        ?P.drumPitchDrop*std::exp(-static_cast<float>(elapsedSamples_)/(sampleRate_*P.drumPitchDropSeconds)):0.0f;
    std::array<float,6>now{};
    for(size_t i=0;i<6;++i){
        const auto&op=P.ops[i];if(!op.enabled){opEnv_[i].next(sampleRate_);continue;}
        const float oldEnv=opEnv_[i].next(sampleRate_);
        const float primaryVol=op.volumeCurve.enabled&&op.volumeCurve.count>1
            ?opVolume_[i].next(sampleRate_):oldEnv;
        const float primaryPitch=op.pitchCurve.enabled&&op.pitchCurve.count>1
            ?opPitch_[i].next(sampleRate_):0.0f;
        // All four envelope timelines advance independently. A change to M
        // never retriggers either envelope; only the mix at this instant moves.
        const float secondaryVol=op.volumeMorphCurve.enabled&&op.volumeMorphCurve.count>1
            ?opMorphVolume_[i].next(sampleRate_):primaryVol;
        const float secondaryPitch=op.pitchMorphCurve.enabled&&op.pitchMorphCurve.count>1
            ?opMorphPitch_[i].next(sampleRate_):primaryPitch;
        std::array<float,4> totals{};
        std::array<int,4> counts{};
        if(op.modChannelsConfigured){
            for(const auto& channel:op.modChannels){
                if(!channel.enabled)continue;
                const int dest=static_cast<int>(channel.destination);
                if(dest<0||dest>=4)continue;
                const auto& c=channel.response;
                const float input=c.source==1?m:expressionV;
                const float output=c.enabled&&c.count>=2?
                    operatorCurveValue(c.points.data(),c.count,clamp01(input),input):clamp01(input);
                totals[static_cast<size_t>(dest)]+=clamp01(output);
                ++counts[static_cast<size_t>(dest)];
            }
        }else{
            const auto& c=op.expressionCurve;
            if(c.enabled&&c.count>=2){
                const float input=c.source==1?m:expressionV;
                totals[0]=clamp01(operatorCurveValue(c.points.data(),c.count,clamp01(input),input));
                counts[0]=1;
            }
        }
        // Averaging prevents multiple channels on one destination from
        // multiplying its intensity; destinations with no route are neutral.
        const auto routed=[&](int dest,float neutral){
            return counts[dest]>0?totals[dest]/static_cast<float>(counts[dest]):neutral;
        };
        const float response=routed(0,1.0f);
        const float pitchDepth=routed(1,1.0f);
        const float volMorph=clamp01(routed(2,0.0f));
        const float pitchMorph=clamp01(routed(3,0.0f));
        const float env=primaryVol+(secondaryVol-primaryVol)*volMorph;
        const float pitchEnv=(primaryPitch+(secondaryPitch-primaryPitch)*pitchMorph)*pitchDepth;
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
            const float sample=op.wave==Wave::Hiss
                ?hissSample(i,u,f)
                :waveSample(op,ph,morphValue(i,expressionV,m));
            opSum+=dynamicUnison&&u==1?sample*unison:sample;
            ph+=kTwoPi*f/sampleRate_;
            if(ph>=kTwoPi)ph-=kTwoPi;
        }
        now[i]=opSum/(dynamicUnison?1.0f+unison:static_cast<float>(uniCount))*level;
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
    if(P.peak.enabled&&!P.filter.evolving.configured){
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
    float filtered=0;
    if(P.filter.evolving.configured){
        const auto& model=P.filter.evolving;
        std::array<float,6> timeline{};
        for(int i=0;i<6;++i)timeline[i]=clamp01(filterTimeline_[i].next(sampleRate_));
        std::array<float,6> modTotal{};
        std::array<int,6> modCount{};
        std::array<float,3> qDirectWeights{};
        for(const auto& route:model.mods){
            if(!route.enabled||route.destination>=6)continue;
            const float input=clamp01(route.response.source==1?m:expressionV);
            const auto& response=route.response;
            const float value=clamp01(response.enabled&&response.count>=2?
                operatorCurveValue(response.points.data(),response.count,input,input):input);
            modTotal[route.destination]+=value;
            ++modCount[route.destination];
            if(route.destination==2){
                const auto w=evolvingShapeWeights(route.shape,route.transition,
                    response.points.data(),response.count,input);
                for(int s=0;s<3;++s)qDirectWeights[s]+=w[s];
            }
        }
        const auto modded=[&](int dest,float fallback){
            return modCount[dest]?clamp01(modTotal[dest]/modCount[dest]):fallback;
        };
        const float freqNorm=modded(0,timeline[0]+
            (timeline[3]-timeline[0])*modded(3,0));
        const float gainNorm=modded(1,timeline[1]+
            (timeline[4]-timeline[1])*modded(4,0));
        const float qNorm=modded(2,timeline[2]+
            (timeline[5]-timeline[2])*modded(5,0));
        std::array<float,3> weights{};
        if(modCount[2]>0){
            for(int s=0;s<3;++s)weights[s]=qDirectWeights[s]/modCount[2];
        }else{
            const auto readQ=[&](int index){
                const auto& lane=model.lanes[index];
                const auto& state=filterTimeline_[index];
                const auto& curve=lane.curve;
                const float position=state.releasing?
                    curve.points[curve.releaseIndex].time+state.releaseAge:
                    std::min(state.age,curve.points[curve.releaseIndex].time);
                return evolvingShapeWeights(lane.shape,lane.transition,
                    curve.points.data(),curve.count,position);
            };
            const auto primary=readQ(2),alt=readQ(5);
            const float morph=modded(5,0);
            for(int s=0;s<3;++s)
                weights[s]=primary[s]*(1-morph)+alt[s]*morph;
        }
        const float smoothing=1.0f-std::exp(-1.0f/(sampleRate_*.004f));
        // Smoothing prevents live V/M jumps from clicking and also
        // brings dormant filter paths into transitions continuously.
        // Key Track owns the frequency destination, including its morph
        // and V/M routes, and follows the oscillator's bent MIDI note.
        const float targetHz=model.keyTrack?
            midiHz(baseMidi):evolvingFrequency(freqNorm);
        filterFrequencySmoothed_+=(targetHz-
            filterFrequencySmoothed_)*smoothing;
        filterGainSmoothed_+=((gainNorm*2-1)*24-filterGainSmoothed_)*smoothing;
        filterQSmoothed_+=(qNorm-filterQSmoothed_)*smoothing;
        float weightSum=0;
        for(int s=0;s<3;++s){
            filterShapeSmoothed_[s]+=(weights[s]-filterShapeSmoothed_[s])*smoothing;
            weightSum+=filterShapeSmoothed_[s];
        }
        const float freq=std::clamp(filterFrequencySmoothed_+
            (!model.keyTrack&&P.lfo.target==LfoTarget::Filter?
                lfo*lfoAmount*3000:0.0f),
            20.0f,sampleRate_*.45f);
        float customWeight=0.0f;
        for(int s=0;s<3;++s){
            const float weight=filterShapeSmoothed_[s]/
                std::max(.00001f,weightSum);
            evolvingFilterBiquads_[s].configureEvolving(
                static_cast<FilterShape>(s),freq,filterQSmoothed_,
                filterGainSmoothed_,sampleRate_);
            const float out=evolvingFilterBiquads_[s].process(sum);
            // Weighted alternatives, never effects connected in series.
            if(model.slopes[s].enabled)customWeight+=weight;
            else filtered+=out*weight;
        }
        const bool profilesEnabled=model.slopes[0].enabled||
            model.slopes[1].enabled||model.slopes[2].enabled;
        if(profilesEnabled){
            // Eight octave-spaced fourth-order lowpasses produce nine
            // complementary logarithmic bands. A weighted band sum gives
            // an actual magnitude-shaping DSP response; unity weights
            // telescope precisely back to the original dry input.
            // Coefficients and magnitude targets refresh every 32 samples,
            // while the gains themselves smooth every sample.
            if((slopeUpdateCounter_++ & 31u)==0){
                constexpr float edges[8]={-3.5f,-2.5f,-1.5f,-.5f,
                                            .5f,1.5f,2.5f,3.5f};
                for(int i=0;i<8;++i){
                    const float cutoff=std::clamp(
                        freq*std::exp2(edges[i]),20.0f,sampleRate_*.45f);
                    for(auto& b:slopeLowpasses_[i])
                        b.configure(FilterType::Lowpass,cutoff,.70710678f,sampleRate_);
                }
                const float gainDb=filterGainSmoothed_;
                const float depth=std::clamp(std::fabs(gainDb)/24.0f,0.0f,1.0f);
                const float bandwidth=1.65f-1.25f*filterQSmoothed_;
                for(int i=0;i<9;++i){
                    const float octaves=static_cast<float>(i)-4.0f;
                    const float position=std::clamp(
                        .5f+octaves/(8.0f*bandwidth),0.0f,1.0f);
                    float weightedMagnitude=0,normalizedWeight=0;
                    for(int s=0;s<3;++s){
                        if(!model.slopes[s].enabled)continue;
                        const auto& curve=model.slopes[s].profile;
                        const float value=std::clamp(operatorCurveValue(
                            curve.points.data(),curve.count,position,1.0f),
                            0.0f,1.0f);
                        const float mask=1.0f-value;
                        const float target=gainDb<0?
                            1.0f-depth*mask:
                            1.0f+depth*(15.84893f-1.0f)*mask;
                        const float weight=std::max(0.0f,filterShapeSmoothed_[s]);
                        weightedMagnitude+=weight*target;
                        normalizedWeight+=weight;
                    }
                    slopeGainTarget_[i]=normalizedWeight>1e-6f?
                        weightedMagnitude/normalizedWeight:1.0f;
                }
            }
            float lowpass[8]{};
            for(int i=0;i<8;++i){
                lowpass[i]=sum;
                for(auto& b:slopeLowpasses_[i])lowpass[i]=b.process(lowpass[i]);
            }
            const float gainSlew=1.0f-std::exp(-1.0f/(sampleRate_*.004f));
            for(int i=0;i<9;++i)
                slopeGainSmoothed_[i]+=(slopeGainTarget_[i]-
                    slopeGainSmoothed_[i])*gainSlew;
            float custom=sum*slopeGainSmoothed_[8];
            for(int i=0;i<8;++i)
                custom+=lowpass[i]*(slopeGainSmoothed_[i]-
                    slopeGainSmoothed_[i+1]);
            filtered+=custom*customWeight;
        }
    }else{
        const float fEnv=filterEnv_.next(sampleRate_);
        float cutoff=slotValue(ModTarget::Cutoff,expressionV,m,P.filter.cutoff)*
            cutScale+fEnv*fixedFilterEnvAmount_*8000;
        if(P.lfo.target==LfoTarget::Filter)cutoff+=lfo*lfoAmount*3000;
        const float q=std::max(0.05f,slotValue(
            ModTarget::Resonance,expressionV,m,P.filter.resonance));
        filter_.configure(P.filter.type,cutoff,q,sampleRate_);
        filtered=filter_.process(sum);
    }
    const float ampLegacy=ampEnv_.next(sampleRate_)*lvlScale;
    const bool masterEnabled=P.masterAmpCurve.enabled&&P.masterAmpCurve.count>=2;
    const float masterLevel=masterEnabled?
        clamp01(masterAmpTimeline_.next(sampleRate_))*lvlScale:1.0f;
    if((masterEnabled&&masterAmpTimeline_.done)||
       (!masterEnabled&&!anyCustomVolume_&&ampEnv_.done())){
        active_=false;return 0;
    }
    bool anyOperatorAlive=false;
    for(size_t i=0;i<6;++i){
        if(!P.ops[i].enabled||P.ops[i].level<=0)continue;
        const bool primaryCustom=P.ops[i].volumeCurve.enabled&&P.ops[i].volumeCurve.count>1;
        const bool altCustom=P.ops[i].volumeMorphCurve.enabled&&P.ops[i].volumeMorphCurve.count>1;
        anyOperatorAlive|=(primaryCustom?!opVolume_[i].done:!opEnv_[i].done())||
            (altCustom&&!opMorphVolume_[i].done);
    }
    if(anyCustomVolume_&&!anyOperatorAlive){active_=false;return 0;}
    const float amp=masterEnabled?masterLevel:(anyCustomVolume_?1.0f:ampLegacy);
    float gain=std::max(0.0f,slotValue(ModTarget::Volume,expressionV,m,1));if(P.lfo.target==LfoTarget::Amp)gain*=std::max(0.0f,1+lfo*lfoAmount*0.5f);
    ++elapsedSamples_;return filtered*amp*gain*P.volume*velocityGain*(P.timpaniMembrane?timpaniRingGain_:1.0f)*(P.cymbalModel?cymbalTailGain_:1.0f);
}

} // namespace aiora
