#include "SpectrachordVoice.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <bit>
#include <mutex>
#include <unordered_map>

namespace aiora {
namespace {
constexpr float kPi=3.14159265358979323846f;
constexpr float kTwoPi=6.28318530717958647692f;
constexpr float kEps=1.0e-6f;
float secondsToSamples(float seconds,float sr) noexcept { return std::max(1.0f,seconds*sr); }

// Iterative inverse FFT: build one oscillator cycle from the 128-partial
// magnitude/phase spectrum. This only executes when a voice starts, never
// in the per-sample oscillator loop. No allocations or plan creation.
void inverseFft(std::array<std::complex<float>,kWaveTableSamples>& values) noexcept {
    constexpr size_t N=kWaveTableSamples;
    for(size_t i=1,j=0;i<N;++i){
        size_t bit=N>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j)std::swap(values[i],values[j]);
    }
    for(size_t span=2;span<=N;span<<=1){
        const float angle=kTwoPi/static_cast<float>(span);
        const std::complex<float> rotation(std::cos(angle),std::sin(angle));
        for(size_t start=0;start<N;start+=span){
            std::complex<float> step(1.0f,0.0f);
            for(size_t j=0;j<span/2;++j){
                const auto a=values[start+j];
                const auto b=values[start+j+span/2]*step;
                values[start+j]=a+b;
                values[start+j+span/2]=a-b;
                step*=rotation;
            }
        }
    }
}

void buildSpectralTables(SpectralWaveTables& tables,
    const HarmonicBank& amplitudes,const HarmonicBank& phases) noexcept {
    std::array<float,kSynthHarmonics> amp{},phase{};
    extrapolateHarmonics(amplitudes,phases,amp,phase);
    float normalization=0.0f;
    for(float value:amp)normalization+=std::fabs(value);
    normalization=std::max(1.0f,normalization);
    constexpr size_t N=kWaveTableSamples;
    constexpr float halfN=static_cast<float>(N)*0.5f;
    constexpr float invN=1.0f/static_cast<float>(N);
    for(size_t level=0;level<kWaveTableLevels;++level){
        std::array<std::complex<float>,N> spectrum{};
        const size_t limit=kSynthHarmonics>>level;
        for(size_t n=1;n<=limit;++n){
            const float amount=amp[n-1]/normalization;
            if(amount<=0.0f)continue;
            const float angle=kTwoPi*phase[n-1];
            const std::complex<float> value(
                halfN*amount*std::sin(angle),
                -halfN*amount*std::cos(angle));
            spectrum[n]=value;
            spectrum[N-n]=std::conj(value);
        }
        inverseFft(spectrum);
        for(size_t sample=0;sample<N;++sample)
            tables[level][sample]=spectrum[sample].real()*invN;
    }
}
// Cache tables by *actual* authored amplitudes and phases, rather than by
// patch name or note. This preserves exact morph-bank and preset semantics.
struct SpectralKey {
    std::array<uint32_t,kEditableHarmonics*2> bits{};
    bool operator==(const SpectralKey&) const noexcept = default;
};
struct SpectralKeyHash {
    size_t operator()(const SpectralKey& key) const noexcept {
        size_t h=static_cast<size_t>(1469598103934665603ull);
        for(uint32_t b:key.bits){
            h^=static_cast<size_t>(b);
            h*=static_cast<size_t>(1099511628211ull);
        }
        return h;
    }
};
struct TableCache {
    std::mutex mutex;
    std::unordered_map<SpectralKey,SharedSpectralBank,SpectralKeyHash> banks{};
};
TableCache& tableCache(){
    static TableCache cache;
    return cache;
}
SharedSpectralBank cachedSpectralBank(const HarmonicBank& amplitudes,
                                     const HarmonicBank& phases){
    SpectralKey key;
    for(size_t i=0;i<kEditableHarmonics;++i){
        key.bits[i]=std::bit_cast<uint32_t>(amplitudes[i]);
        key.bits[i+kEditableHarmonics]=std::bit_cast<uint32_t>(phases[i]);
    }
    auto& cache=tableCache();
    std::scoped_lock lock(cache.mutex);
    if(const auto it=cache.banks.find(key);it!=cache.banks.end())
        return it->second;
    // Keep several MB of tables available across note releases. The last
    // owner of a playing voice's bank must not disappear on the audio thread:
    // only evict cache-only entries, with use_count() == 1.
    constexpr size_t kCachedBankLimit=192;
    if(cache.banks.size()>=kCachedBankLimit){
        for(auto it=cache.banks.begin();
            it!=cache.banks.end()&&cache.banks.size()>=kCachedBankLimit;){
            if(it->second.use_count()==1)it=cache.banks.erase(it);
            else ++it;
        }
    }
    auto writable=std::make_shared<SpectralWaveTables>();
    buildSpectralTables(*writable,amplitudes,phases);
    const SharedSpectralBank prepared=writable;
    cache.banks.emplace(std::move(key),prepared);
    return prepared;
}
SharedSpectralBank preparedTonalBank(Wave wave,
                                    const HarmonicBank& amplitudes,
                                    const HarmonicBank& phases,
                                    bool customAuthored){
    if(isTextureWave(wave))return {};
    return cachedSpectralBank(
        wave==Wave::Custom&&customAuthored?amplitudes:presetWaveHarmonics(wave),
        wave==Wave::Custom&&customAuthored?phases:presetWavePhases(wave));
}
}

void prepareDspPatchWaveforms(DspPatch& patch){
    for(size_t i=0;i<patch.ops.size();++i){
        const auto& op=patch.ops[i];
        if(!op.enabled)continue;
        auto& primary=patch.spectralBanks.primary[i];
        auto& alternate=patch.spectralBanks.alternate[i];
        primary=preparedTonalBank(op.wave,op.harm,op.harmPhase,op.hasHarm);
        const Wave alt=effectiveWaveMorph(op);
        if(alt==op.wave&&!(alt==Wave::Custom&&op.hasHarmMute))
            alternate=primary;
        else
            alternate=preparedTonalBank(alt,op.harmMute,op.harmMutePhase,op.hasHarmMute);
    }
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
void SpectrachordVoice::Biquad::configureLowpass(
    float freq,float q,float sr) noexcept {
    freq=std::clamp(freq,20.0f,sr*.45f);
    q=std::max(.05f,q);
    const float w0=kTwoPi*freq/sr;
    const float c=std::cos(w0),sn=std::sin(w0),alpha=sn/(2*q);
    const float inv=1.0f/(1+alpha);
    b0=(1-c)*.5f*inv;b1=(1-c)*inv;b2=b0;
    a1=-2*c*inv;a2=(1-alpha)*inv;
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

void SpectrachordVoice::start(int32_t id,const DspPatch& patch,int midi,float pressure,uint64_t gateSamples,float samplesPerStep,const VoiceAutomation& automation) noexcept {
    id_=id;patch_=patch;midi_=midi;
    keyMapValue_=normalizedKeyMap(midi,patch.keyMapLow,patch.keyMapHigh);
    velocity_=clamp01(pressure);liveGain_=1.0f;active_=true;automation_=automation;
    gateSamples_=gateSamples;autoRelease_=gateSamples>0;elapsedSamples_=0;samplesPerStep_=std::max(1.0f,samplesPerStep);
    // M is always supplied; an unedited line is explicitly M=0, never
    // a request to bypass an operator or FX modulation route.
    modActive_=true;velocityCurveActive_=automation_.velocity.count>0;
    modValue_=curveValue(automation_.mod,0,0);
    masterAmpTimeline_.reset(patch_.masterAmpCurve,false);
    fixedUnison_=clamp01(patch_.unison);
    if(patch_.unisonCurve.enabled&&patch_.unisonCurve.count>=2){
        unisonTimeline_.reset(patch_.unisonCurve,true);
        unisonSmoothed_=std::clamp(patch_.unisonCurve.points[0].value,0.0f,1.0f);
    }else unisonSmoothed_=fixedUnison_;
    rng_^=static_cast<uint32_t>(id*747796405u+midi*2891336453u);phase_={};lastOp_={};lfoPhase_=0;lfoAge_=0;peak_.reset();peakReady_=false;
    // Tables were generated once off the audio callback during patch
    // preparation. Here only per-note phase state is initialized.
    for(size_t i=0;i<6;++i){
        const auto& o=patch_.ops[i];
        if(!o.enabled||!o.randomPhase||o.randomPhaseDegrees<=0.0f||
           isTextureWave(o.wave))continue;
        if(rng_==0)rng_=0x91e10da5u;
        for(int u=0;u<2;++u){
            rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;
            // +/-degrees about the previous zero-phase start. Wrap negatives
            // into [0,2pi), preserving a shared base phase for custom partials.
            const float unit=static_cast<float>(rng_>>8)/16777216.0f;
            float angle=(unit*2.0f-1.0f)*
                (std::clamp(o.randomPhaseDegrees,0.0f,180.0f)*
                 (kTwoPi/360.0f));
            if(angle<0.0f)angle+=kTwoPi;
            phase_[i][u]=angle;
        }
    }
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
    textureState_={};
    for(size_t i=0;i<6;++i){
        opVolume_[i].reset(patch_.ops[i].volumeCurve);
        opPitch_[i].reset(patch_.ops[i].pitchCurve);
        opMorphVolume_[i].reset(patch_.ops[i].volumeMorphCurve);
        opMorphPitch_[i].reset(patch_.ops[i].pitchMorphCurve);
    }
}
void SpectrachordVoice::release() noexcept {
    if(!active_)return;
    autoRelease_=false;
    for(size_t i=0;i<6;++i){
        opVolume_[i].noteOff();opPitch_[i].noteOff();
        opMorphVolume_[i].noteOff();opMorphPitch_[i].noteOff();
    }
    if(patch_.filter.evolving.configured)
        for(auto& env:filterTimeline_)env.noteOff();
    if(patch_.unisonCurve.enabled&&patch_.unisonCurve.count>=2)
        unisonTimeline_.noteOff();
    if(patch_.masterAmpCurve.enabled&&patch_.masterAmpCurve.count>=2)
        masterAmpTimeline_.noteOff();
}
void SpectrachordVoice::kill() noexcept {active_=false;id_=-1;autoRelease_=false;}

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
float SpectrachordVoice::textureSample(
    Wave wave,const TextureParams& params,bool pitched,size_t index,
    int unisonVoice,bool alternate,float pitchHz,float oscillatorPhase) noexcept {
    if(wave==Wave::Hiss)return hissSample(index,unisonVoice,pitchHz);
    if(wave==Wave::Noise){
        if(pitched)return hissSample(index,unisonVoice,pitchHz);
        return noise(); // preserve historical Noise exactly
    }
    auto& state=textureState_[index][static_cast<size_t>(unisonVoice)]
                              [alternate?1u:0u];
    const bool custom=wave==Wave::TextureCustom;
    const float density=custom?params.density:0.55f;
    const float size=custom?params.size:0.38f;
    const float color=custom?params.color:0.60f;
    const float resonance=custom?params.resonance:0.5f;
    const float chaos=custom?params.chaos:0.45f;
    const int source=custom?static_cast<int>(params.source):
        (wave==Wave::Grain?1:wave==Wave::Rasp?3:2);
    const float base=pitched?pitchHz:115.0f;
    // Pitched follows the full operator pitch path. Random uses a
    // note-independent rate, but still has randomized internal events.
    const float rate=std::clamp(base*(0.18f+density*2.0f),
                                5.0f,sampleRate_*0.35f);
    const float r=noise();
    if(source==0){ // colored stochastic excitation
        const float alpha=std::clamp(
            (pitched?base*(1.0f+color*7.5f):120.0f+color*18000.0f)/
                sampleRate_,0.002f,.48f);
        state.filtered+=alpha*(r-state.filtered);
        return state.filtered*1.8f;
    }
    if(source==1){ // sample-and-hold grains, with adjustable smoothing
        const float grainHz=std::clamp(rate*(0.5f+4.0f*(1.0f-size)),
                                       4.0f,sampleRate_*.40f);
        state.phase+=grainHz/sampleRate_;
        if(state.phase>=1.0f){
            state.phase-=std::floor(state.phase);
            state.held=noise();
        }
        const float smoothing=0.002f+(1.0f-chaos)*size*.35f;
        state.filtered+=smoothing*(state.held-state.filtered);
        return state.held*(1.0f-size*.68f)+state.filtered*size*.68f;
    }
    if(source==2){ // impulse-driven metallic resonators
        state.phase+=rate/sampleRate_;
        if(state.phase>=1.0f){
            state.phase-=std::floor(state.phase);
            state.ring+=(0.45f+chaos*.55f)*r;
        }
        const float ringHz=std::clamp(base*(1.0f+resonance*.6f),
                                      1.0f,sampleRate_*.37f);
        const float decay=1.0f-(0.00035f+0.012f*(1.0f-size));
        state.ring=std::clamp(state.ring*decay,-1.0f,1.0f);
        // Inharmonic beating defines the Clang family.
        state.color+=ringHz*6.28318530718f/sampleRate_;
        if(state.color>=6.28318530718f)state.color-=6.28318530718f;
        const float metal=std::sin(state.color)+
            .47f*std::sin(state.color*1.41421356f)+
            .24f*std::sin(state.color*2.7182818f);
        return state.ring*metal*.57f;
    }
    // Stick/slip friction: coherent fundamental with random abrasion.
    const float frictionPhase=pitched?oscillatorPhase:
        (state.phase+=rate/sampleRate_*6.28318530718f);
    if(!pitched&&state.phase>=6.28318530718f)
        state.phase-=6.28318530718f;
    const float edge=std::sin(frictionPhase+
        chaos*.8f*r+resonance*.5f*std::sin(frictionPhase*3.0f));
    state.filtered+=(.03f+size*.18f)*(r-state.filtered);
    return std::clamp(edge*(.7f+.3f*resonance)+
                      state.filtered*chaos*.55f,-1.0f,1.0f);
}
float SpectrachordVoice::waveSample(
    Wave wave,const SpectralWaveTables& tables,float phase,float frequency) noexcept {
    if(wave==Wave::Noise)return noise();
    // Pick the highest available harmonic-resolution table whose highest
    // partial remains below Nyquist at the actual post-FM frequency.
    const float permitted=(sampleRate_*0.48f)/std::max(0.001f,frequency);
    if(permitted<1.0f)return 0.0f;
    size_t level=0;
    while(level+1<kWaveTableLevels&&
          static_cast<float>(kSynthHarmonics>>level)>permitted)
        ++level;
    const auto& cycle=tables[level];
    const float position=phase*(static_cast<float>(kWaveTableSamples)/kTwoPi);
    const size_t index=std::min(static_cast<size_t>(position),kWaveTableSamples-1);
    const float fraction=std::clamp(position-static_cast<float>(index),0.0f,1.0f);
    const float a=cycle[index],b=cycle[(index+1)&(kWaveTableSamples-1)];
    return a+(b-a)*fraction;
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
    const float m=modValue_;
    const float expressionV=velocityCurveActive_?velNow:velocity_;
    const float lfo=std::sin(lfoPhase_);lfoPhase_+=kTwoPi*std::max(0.05f,P.lfo.rate)/sampleRate_;if(lfoPhase_>=kTwoPi)lfoPhase_-=kTwoPi;
    lfoAge_+=1/sampleRate_;const float lfoFade=P.lfo.attack<=0?1:std::min(1.0f,lfoAge_/P.lfo.attack);const float lfoVel=(1-P.lfo.velocitySensitivity)+P.lfo.velocitySensitivity*pressure;
    const float lfoAmount=std::max(0.0f,P.lfo.amount)*lfoFade*lfoVel;const float pitchLfoCents=P.lfo.target==LfoTarget::Pitch?lfo*lfoAmount*60:0;

    const bool dynamicUnison=P.unisonCurve.enabled&&P.unisonCurve.count>=2;
    const float unisonTarget=dynamicUnison?
        clamp01(unisonTimeline_.next(sampleRate_)):clamp01(fixedUnison_);
    const float smoothing=1.0f-std::exp(-1.0f/(sampleRate_*.004f));
    unisonSmoothed_+=(unisonTarget-unisonSmoothed_)*smoothing;
    const float baseMidi=static_cast<float>(midi_)+P.octave*12+bend;
    const float unison=clamp01(unisonSmoothed_);
    const int uniCount=unison>0.0001f?2:1;
    // A brief beater-driven membrane sweep settles at the unchanged note fundamental.
    float sum=0.0f;
    for(size_t i=0;i<6;++i){
        const auto&op=P.ops[i];if(!op.enabled)continue;
        const float primaryVol=opVolume_[i].next(sampleRate_);
        const float primaryPitch=op.pitchCurve.enabled&&op.pitchCurve.count>1
            ?opPitch_[i].next(sampleRate_):0.0f;
        // All four envelope timelines advance independently. A change to M
        // never retriggers either envelope; only the mix at this instant moves.
        const float secondaryVol=op.volumeMorphCurve.enabled&&op.volumeMorphCurve.count>1
            ?opMorphVolume_[i].next(sampleRate_):primaryVol;
        const float secondaryPitch=op.pitchMorphCurve.enabled&&op.pitchMorphCurve.count>1
            ?opMorphPitch_[i].next(sampleRate_):primaryPitch;
        std::array<float,5> totals{};
        std::array<int,5> counts{};
        if(op.modChannelsConfigured){
            for(const auto& channel:op.modChannels){
                if(!channel.enabled)continue;
                const int dest=static_cast<int>(channel.destination);
                if(dest<0||dest>=5)continue;
                const auto& c=channel.response;
                const float input=c.source==2?keyMapValue_:(c.source==1?m:expressionV);
                const float output=c.enabled&&c.count>=2?
                    operatorCurveValue(c.points.data(),c.count,clamp01(input),input):clamp01(input);
                totals[static_cast<size_t>(dest)]+=clamp01(output);
                ++counts[static_cast<size_t>(dest)];
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
        // The fifth, visible operator destination owns waveform morphing.
        // The retired patch-level MORPH1..6 routes remain stored but no
        // longer change waveform content invisibly.
        const float waveMorph=clamp01(routed(4,0.0f));
        const float env=primaryVol+(secondaryVol-primaryVol)*volMorph;
        const float pitchEnv=(primaryPitch+(secondaryPitch-primaryPitch)*pitchMorph)*pitchDepth;
        const float level=op.level*env*response;
        if(level<=0.000001f)continue;
        const float semitoneScale=std::pow(2.0f,op.semitoneOffset/12.0f);
        // The former six-by-six FM cross-routing has been removed.
        // All operator phase evolution now follows its authored pitch curve.
        // Bend remains unrestricted except for physical oscillator limits.
        const float unboundedBase=midiHz(baseMidi+pitchEnv)*op.ratio*semitoneScale;
        const float maxHz=sampleRate_*0.49f;
        const float baseF=std::isfinite(unboundedBase)
            ?std::clamp(unboundedBase,0.001f,maxHz):maxHz;
        float opSum=0;
        for(int u=0;u<uniCount;++u){
            const float uniCents=u?unison*12:0;
            const float cents=op.detuneCents+uniCents+pitchLfoCents;
            const float unboundedHz=baseF*std::pow(2.0f,cents/1200.0f);
            const float f=std::isfinite(unboundedHz)
                ?std::clamp(unboundedHz,0.001f,maxHz):maxHz;
            float&ph=phase_[i][u];
            const auto bankSample=[&](Wave wave,
                const SharedSpectralBank& tables,
                const TextureParams& params,bool pitched,bool alternate)->float {
                if(isTextureWave(wave))
                    return textureSample(wave,params,pitched,i,u,alternate,f,ph);
                return tables?waveSample(wave,*tables,ph,f):0.0f;
            };
            float sample=bankSample(op.wave,P.spectralBanks.primary[i],
                                    op.texture,op.texturePitched,false);
            if(waveMorph>0.0f){
                const Wave alternate=effectiveWaveMorph(op);
                const bool distinct=alternate!=op.wave||
                    (alternate==Wave::Custom&&op.hasHarmMute)||
                    (isTextureWave(alternate)&&
                     (op.textureMorphPitched!=op.texturePitched||
                      alternate==Wave::TextureCustom));
                if(distinct){
                    const float alternateSample=bankSample(
                        alternate,P.spectralBanks.alternate[i],
                        op.waveMorphConfigured?op.textureMorph:op.texture,
                        op.waveMorphConfigured?op.textureMorphPitched:op.texturePitched,
                        true);
                    sample+=(alternateSample-sample)*waveMorph;
                }
            }
            opSum+=dynamicUnison&&u==1?sample*unison:sample;
            ph+=kTwoPi*f/sampleRate_;
            if(ph>=kTwoPi)ph-=kTwoPi;
        }
        // A localized 1 ms smoothstep removes the potential discontinuity
        // of a random nonzero phase at note-on. Existing operators retain
        // bit-identical onset behavior and their authored envelope shapes.
        if(op.randomPhase&&op.randomPhaseDegrees>0.0f&&
           !isTextureWave(op.wave)){
            const float t=std::clamp(
                static_cast<float>(elapsedSamples_)/
                std::max(1.0f,sampleRate_*.001f),0.0f,1.0f);
            opSum*=t*t*(3.0f-2.0f*t);
        }
        sum+=opSum/(dynamicUnison?1.0f+unison:static_cast<float>(uniCount))*level;
    }
    // A separate, optional bell EQ runs once per voice, BEFORE the primary
    // subtractive filter. Pitch tracking uses the known MIDI fundamental,
    // including pitch bend; no audio pitch detection is necessary.
    if(P.peak.enabled&&!P.filter.evolving.configured){
        const float center=P.peak.followNote
            ?midiHz(baseMidi)*std::clamp(P.peak.ratio,.5f,8.0f)
            :P.peak.frequency;
        const float targetCenter=std::clamp(center,20.0f,sampleRate_*.45f);
        const float targetQ=std::clamp(P.peak.q,.3f,12.0f);
        const float targetGain=std::clamp(P.peak.gainDb,-18.0f,18.0f);
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
            const float input=clamp01(route.response.source==2?keyMapValue_:
                (route.response.source==1?m:expressionV));
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
                        b.configureLowpass(cutoff,.70710678f,sampleRate_);
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
    }
    bool anyOperatorAlive=false;
    for(size_t i=0;i<6;++i){
        if(!P.ops[i].enabled||P.ops[i].level<=0)continue;
        const bool primaryCustom=P.ops[i].volumeCurve.enabled&&P.ops[i].volumeCurve.count>1;
        const bool altCustom=P.ops[i].volumeMorphCurve.enabled&&P.ops[i].volumeMorphCurve.count>1;
        anyOperatorAlive|=!opVolume_[i].done||
            (altCustom&&!opMorphVolume_[i].done);
    }
    const float master=clamp01(masterAmpTimeline_.next(sampleRate_));
    if(masterAmpTimeline_.done||!anyOperatorAlive){active_=false;return 0;}
    float ampMod=1.0f,modSum=0.0f;
    int modCount=0;
    for(const auto& route:P.ampMods){
        if(!route.enabled||route.count<2)continue;
        const float input=route.source==2?keyMapValue_:
            (route.source==1?m:expressionV);
        modSum+=clamp01(operatorCurveValue(
            route.points.data(),route.count,clamp01(input),input));
        ++modCount;
    }
    if(modCount>0)ampMod=modSum/static_cast<float>(modCount);
    const float gain=P.lfo.target==LfoTarget::Amp?
        std::max(0.0f,1+lfo*lfoAmount*0.5f):1.0f;
    ++elapsedSamples_;
    return filtered*master*ampMod*gain*P.volume*velocityGain*liveGain_;
}

} // namespace aiora
