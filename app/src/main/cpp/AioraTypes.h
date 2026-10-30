#pragma once
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace aiora {

// Retain Hiss=6 as a legacy serialized/runtime value. New selectors never
// expose it; old patches migrate Hiss to Noise with pitched mode enabled.
enum class Wave : uint8_t {
    Sine, Saw, Square, Triangle, Custom, Noise, Hiss, Grain, Rasp, Clang, TextureCustom
};
inline bool isTextureWave(Wave w) noexcept {
    return w==Wave::Noise||w==Wave::Hiss||w==Wave::Grain||
        w==Wave::Rasp||w==Wave::Clang||w==Wave::TextureCustom;
}
inline constexpr std::array<Wave,5> kTonalWaves{
    Wave::Sine,Wave::Saw,Wave::Square,Wave::Triangle,Wave::Custom};
inline constexpr std::array<const char*,5> kTonalLabels{
    "Sine","Sawtooth","Square","Triangle","Custom"};
inline constexpr std::array<Wave,5> kTextureWaves{
    Wave::Noise,Wave::Grain,Wave::Rasp,Wave::Clang,Wave::TextureCustom};
inline constexpr std::array<const char*,5> kTextureLabels{
    "Noise","Grain","Rasp","Clang","Custom"};
// Single source of truth for selectable operator waveforms and their menu names.
// 64 authored harmonics per bank, extended to 128 during wavetable construction.
constexpr size_t kEditableHarmonics=64;
constexpr size_t kSynthHarmonics=128;
using HarmonicBank=std::array<float,kEditableHarmonics>;
// One immutable bank is shared by any voices using identical harmonic data.
// Expression, frequency and morph mixing remain calculated every sample.
constexpr size_t kWaveTableSamples=512;
constexpr size_t kWaveTableLevels=8;
using SpectralWaveTables=std::array<
    std::array<float,kWaveTableSamples>,kWaveTableLevels>;
using SharedSpectralBank=std::shared_ptr<const SpectralWaveTables>;
struct PreparedSpectralBanks {
    std::array<SharedSpectralBank,6> primary{};
    std::array<SharedSpectralBank,6> alternate{};
};
inline HarmonicBank presetWaveHarmonics(Wave wave) noexcept {
    HarmonicBank partials{};
    for(size_t i=0;i<partials.size();++i){
        const float n=static_cast<float>(i+1);
        if(wave==Wave::Sine)partials[i]=i==0?1.0f:0.0f;
        else if(wave==Wave::Saw)partials[i]=1.0f/n;
        else if(wave==Wave::Square)partials[i]=i%2==0?1.0f/n:0.0f;
        else if(wave==Wave::Triangle)partials[i]=i%2==0?1.0f/(n*n):0.0f;
        else if(wave==Wave::Custom)partials[i]=1.0f/n;
    }
    return partials;
}
// Phase is expressed in cycle fractions: 0=0 degrees, .25=90, .5=180.
// Triangle's alternating cosine partials retain the correct relative phase.
inline HarmonicBank presetWavePhases(Wave wave) noexcept {
    HarmonicBank phases{};
    if(wave==Wave::Triangle){
        for(size_t i=0;i<phases.size();i+=2)
            phases[i]=(i%4==0)?0.25f:0.75f;
    }
    return phases;
}
// Extend only harmonics 65..128. Doubled frequencies copy their source
// phase and halve amplitude, subject to the 1/n upper-amplitude ceiling.
// Gaps use linear amplitude and shortest-arc circular phase interpolation.
inline void extrapolateHarmonics(const HarmonicBank& amplitude,
    const HarmonicBank& phase,std::array<float,kSynthHarmonics>& outAmp,
    std::array<float,kSynthHarmonics>& outPhase) noexcept {
    for(size_t i=0;i<kEditableHarmonics;++i){
        outAmp[i]=std::clamp(amplitude[i],0.0f,1.0f);
        outPhase[i]=std::clamp(phase[i],0.0f,1.0f);
    }
    for(size_t source=33;source<=64;++source){
        const size_t dest=source*2;
        outAmp[dest-1]=std::min(outAmp[source-1]*0.5f,1.0f/static_cast<float>(dest));
        outPhase[dest-1]=outPhase[source-1];
    }
    const auto midpoint=[](float a,float b) noexcept {
        float delta=b-a;
        if(delta>0.5f)delta-=1.0f;
        else if(delta<-0.5f)delta+=1.0f;
        const float value=a+delta*0.5f;
        return value-std::floor(value);
    };
    // 65 is bridged from the edited 64 to the generated 66.
    for(size_t n=65;n<128;n+=2){
        outAmp[n-1]=std::min(
            0.5f*(outAmp[n-2]+outAmp[n]),
            1.0f/static_cast<float>(n));
        outPhase[n-1]=midpoint(outPhase[n-2],outPhase[n]);
    }
}
inline constexpr std::array<const char*,11> kWaveformLabels{
    "sine","sawtooth","square","triangle","custom","noise","hiss",
    "grain","rasp","clang","texture custom"
};
static_assert(kWaveformLabels.size()==static_cast<size_t>(Wave::TextureCustom)+1);

enum class LfoTarget : uint8_t { None, Pitch, Filter, Amp };
// Fixed-capacity curves: no allocation during synthesis or note scheduling.
// Time envelopes use seconds; response curves use input fractions (0..1).
constexpr size_t kMaxOperatorPoints=24;
struct OperatorPoint {
    float time{0},value{0},midpoint{0.5f},handleValue{0};
    bool free{false},handleActive{false};
};
struct OperatorCurve {
    std::array<OperatorPoint,kMaxOperatorPoints> points{};
    uint8_t count{0},releaseIndex{0},decayIndex{1};
    bool enabled{false},sustain{true};
};
struct OperatorResponse {
    std::array<OperatorPoint,kMaxOperatorPoints> points{};
    uint8_t count{0};
    uint8_t source{0}; // 0=V, 1=M, 2=K (played key)
    bool enabled{false};
};
enum class OperatorModDestination : uint8_t {
    Vol=0, Pitch=1, Morph=2, PitchMorph=3, WaveMorph=4
};
struct OperatorModChannel {
    OperatorResponse response{};
    OperatorModDestination destination{OperatorModDestination::Vol};
    bool enabled{false};
};
inline OperatorModChannel initialOperatorModChannel() noexcept {
    OperatorModChannel c;
    c.response.enabled=true;c.response.count=2;
    c.response.points[0]={0,0};c.response.points[1]={1,1};
    return c;
}
// Use the same monotone X-handle interpolation as the piano-roll B/V/M curves.
inline float curveSegment(float fraction,float a,float b,float q,bool shaped,float handleValue) noexcept;
// Interpolates warped/curved segments without allocating on the audio thread.
inline float operatorCurveValue(const OperatorPoint* points,size_t count,float x,float fallback=0) noexcept {
    if(!count)return fallback;
    if(x<=points[0].time)return points[0].value;
    for(size_t i=1;i<count;++i){
        if(x<=points[i].time){
            const auto&a=points[i-1];const auto&b=points[i];
            const float t=std::clamp((x-a.time)/std::max(.000001f,b.time-a.time),0.0f,1.0f);
            return curveSegment(t,a.value,b.value,b.midpoint,b.handleActive,b.handleValue);
        }
    }
    return points[count-1].value;
}
// A neutral, editable note-on/release envelope for new operators.
inline OperatorCurve initialOperatorVolume() noexcept {
    OperatorCurve c; c.enabled=true;c.sustain=true;c.count=4;
    c.decayIndex=1;c.releaseIndex=2;
    c.points[0]={0,0};c.points[1]={.005f,1};
    c.points[2]={.125f,.85f};c.points[3]={.375f,0};
    return c;
}
// Unity master lane: global amplitude is neutral until explicitly drawn.
// Operator envelopes, not a hidden master ADSR, determine instrument decay.
inline OperatorCurve initialMasterAmpCurve() noexcept {
    OperatorCurve c;c.enabled=true;c.sustain=true;c.count=2;
    c.decayIndex=0;c.releaseIndex=0;
    c.points[0]={0,1};c.points[1]={120,1};
    return c;
}
inline OperatorCurve initialOperatorPitch() noexcept {
    OperatorCurve c;c.enabled=true;c.sustain=true;c.count=4;c.decayIndex=1;c.releaseIndex=2;
    c.points[0]={0,0};c.points[1]={.02f,0};c.points[2]={.1f,0};
    c.points[3]={.25f,0};
    return c;
}
inline OperatorResponse initialOperatorResponse() noexcept {
    OperatorResponse c;c.enabled=true;c.count=2;c.points[0]={0,0};c.points[1]={1,1};return c;
}
struct TextureParams {
    // 0 noise, 1 grains, 2 impulses, 3 friction. All normalized 0..1.
    uint8_t source{1};
    float density{0.50f},size{0.35f},color{0.60f};
    float resonance{0.45f},chaos{0.30f};
};
struct Operator {
    bool enabled{true};
    // Optional phase variation at note-on; disabled by default.
    bool randomPhase{false};
    // Symmetric note-on variation in degrees: 10 = -10..+10, 180 = full circle.
    float randomPhaseDegrees{10.0f};
    Wave wave{Wave::Sine};
    Wave waveMorph{Wave::Sine};
    bool waveMorphConfigured{false};
    bool texturePitched{false};
    bool textureMorphPitched{false};
    TextureParams texture{},textureMorph{};
    float ratio{1.0f};
    float semitoneOffset{0.0f};
    float detuneCents{0.0f};
    float level{0.0f};
    OperatorCurve volumeCurve{initialOperatorVolume()},pitchCurve{};
    OperatorCurve volumeMorphCurve{},pitchMorphCurve{};
    std::array<OperatorModChannel,6> modChannels{};
    bool modChannelsConfigured{false};
    HarmonicBank harm{},harmMute{};
    HarmonicBank harmPhase{},harmMutePhase{};
    bool hasHarm{false},hasHarmMute{false};
};
// Unconfigured alternate banks follow the primary waveform.
inline Wave effectiveWaveMorph(const Operator& op) noexcept {
    if(op.waveMorphConfigured)return op.waveMorph;
    if(op.wave==Wave::Custom&&op.hasHarmMute)return Wave::Custom;
    return op.wave;
}
// Evolving filter uses six synchronized envelopes: frequency, gain and Q,
// each with primary / morph variants. Values are normalized 0..1; amount
// uses 0.5 as neutral. Q points carry a type and an adjustable transition
// start fraction before a subsequent node with a different type.
enum class FilterShape : uint8_t { LowShelf=0, Bell=1, HighShelf=2 };
struct FilterLane {
    OperatorCurve curve{};
    std::array<FilterShape,kMaxOperatorPoints> shape{};
    std::array<float,kMaxOperatorPoints> transition{};
};
inline FilterLane initialFilterLane(int lane) noexcept {
    FilterLane l;
    // Two equal-valued endpoints mean one draggable static line.
    // The first inserted point converts it to a timed envelope.
    l.curve.enabled=true;l.curve.sustain=true;l.curve.count=2;
    l.curve.decayIndex=0;l.curve.releaseIndex=0;
    const float v=lane%3==0?0.53f:0.5f;
    l.curve.points[0]={0,v};l.curve.points[1]={120.0f,v};
    l.shape.fill(FilterShape::Bell);
    l.transition.fill(.65f);
    return l;
}
struct FilterModChannel {
    OperatorResponse response{};
    std::array<FilterShape,kMaxOperatorPoints> shape{};
    std::array<float,kMaxOperatorPoints> transition{};
    uint8_t destination{0}; // F, A, Q, F Morph, A Morph, Q Morph
    bool enabled{false};
};
inline FilterModChannel initialFilterModChannel() noexcept {
    FilterModChannel m;
    m.response=initialOperatorResponse();
    m.shape.fill(FilterShape::Bell);m.transition.fill(.65f);
    return m;
}
// Magnitude response drawn against relative octaves (-4 .. +4).
// The new response replaces, rather than cascades with, its legacy shape.
struct FilterSlope {
    bool enabled{false};
    OperatorResponse profile{};
};
inline FilterSlope initialFilterSlope(int shape) noexcept {
    FilterSlope f;f.profile.enabled=true;f.profile.count=5;
    constexpr std::array<float,5> x{{0,.25f,.5f,.75f,1}};
    for(int i=0;i<5;++i) {
        const float y=shape==0?(i<2?0:(i==2?.5f:1)):
            shape==2?(i<2?1:(i==2?.5f:0)):
            (i==2?0:1);
        f.profile.points[i]={x[i],y};
    }
    return f;
}
struct EvolvingFilter {
    bool configured{false};
    std::array<FilterLane,6> lanes{};
    std::array<FilterModChannel,6> mods{};
    bool keyTrack{false};
    std::array<FilterSlope,3> slopes{};
};
inline EvolvingFilter initialEvolvingFilter() noexcept {
    EvolvingFilter e;e.configured=true;
    for(int i=0;i<6;++i)e.lanes[i]=initialFilterLane(i);
    for(auto& mod:e.mods)mod=initialFilterModChannel();
    for(int i=0;i<3;++i)e.slopes[i]=initialFilterSlope(i);
    return e;
}
struct Filter { EvolvingFilter evolving{initialEvolvingFilter()}; };
// Optional per-voice bell EQ before the main filter. Center follows the played
// fundamental times ratio, or stays at frequency when followNote is false.
struct PeakFilter {
    bool enabled{false},followNote{true};
    float ratio{4.0f},frequency{220.0f},q{1.0f},gainDb{0.0f};
};
struct Lfo { float rate{5},amount{0},attack{0},velocitySensitivity{0}; LfoTarget target{LfoTarget::None}; };
// Keep live FX identifiers stable in saved modern projects. Retired IDs are
// intentionally unassigned and rejected during decoding.
enum class FxType : uint8_t {
    Embera=0, Prisma=3, Recurra=4, Vastrel=5, Ampere=7, Cabeus=8,
    Pulsar=9, Binary=10
};
inline constexpr std::array<FxType,8> kAvailableFxTypes{{
    FxType::Embera,FxType::Recurra,FxType::Vastrel,
    FxType::Prisma,FxType::Ampere,FxType::Cabeus,
    FxType::Pulsar,FxType::Binary
}};
inline bool isAvailableFxType(FxType type) noexcept {
    for(const auto allowed:kAvailableFxTypes)
        if(allowed==type)return true;
    return false;
}
inline int fxTypeMenuIndex(FxType type) noexcept {
    for(size_t i=0;i<kAvailableFxTypes.size();++i)
        if(kAvailableFxTypes[i]==type)return static_cast<int>(i);
    return 0;
}
constexpr size_t kMaxFxStages=16;
constexpr size_t kPrismaBands=16;
constexpr size_t kMaxFxParams=kPrismaBands+1;
struct FxModule {
    FxType type{FxType::Embera};
    bool enabled{true};
    std::array<float,kMaxFxParams> params{};
};
struct FxParamDesc { const char* name;float lo,hi,defaultValue; };
inline const char* fxTypeName(FxType type) noexcept {
    switch(type){
        case FxType::Embera:return "Embera";
        case FxType::Recurra:return "Recurra";
        case FxType::Vastrel:return "Vastrel";
        case FxType::Prisma:return "Prisma";
        case FxType::Ampere:return "Ampere";
        case FxType::Cabeus:return "Cabeus";
        case FxType::Pulsar:return "Pulsar";
        case FxType::Binary:return "Binary";
    }
    return "Effect";
}
inline size_t fxParamCount(FxType type) noexcept {
    switch(type){
        case FxType::Embera:return 4;
        case FxType::Prisma:return kPrismaBands+1;
        case FxType::Recurra:return 3;
        case FxType::Vastrel:return 3;
        case FxType::Ampere:return 8;
        case FxType::Cabeus:return 8;
        case FxType::Pulsar:return 5;
        case FxType::Binary:return 4;
    }
    return 0;
}
inline FxParamDesc fxParamDesc(FxType type,size_t p) noexcept {
    static constexpr FxParamDesc embera[]={
        {"Input",0,4,1},{"Drive",0,10,3},
        {"Output",0,4,1},{"Bitcrush",0,1,0}
    };
    // Ampere retains its approved dynamics and voicing unchanged.
    static constexpr FxParamDesc ampere[]={{"Input",0,4,.5f},{"Drive",0,10,7.0f},{"Tight Hz",40,240,105},{"Mid dB",-12,12,-3},{"Bite dB",-12,12,3},{"Definition",0,1,.32f},{"Soft Gate",0,1,0},{"Output",0,4,.6f}};
    // Cabeus remains the approved modelled speaker cabinet.
    static constexpr FxParamDesc cabeus[]={{"Cab Model",0,2,2},{"Mic Position",0,1,.35f},{"Mic Distance",0,1,.15f},{"Low Cut Hz",40,200,75},{"High Cut Hz",2500,9500,6800},{"Resonance",0,1,.55f},{"Mix",0,1,1},{"Output",0,4,1}};
    // 16 logarithmically spaced parametric bands, each ±12 dB, plus output.
    static constexpr FxParamDesc prisma[]={
        {"31 Hz dB",-12,12,0},{"47 Hz dB",-12,12,0},
        {"70 Hz dB",-12,12,0},{"106 Hz dB",-12,12,0},
        {"160 Hz dB",-12,12,0},{"240 Hz dB",-12,12,0},
        {"360 Hz dB",-12,12,0},{"540 Hz dB",-12,12,0},
        {"810 Hz dB",-12,12,0},{"1.2k dB",-12,12,0},
        {"1.8k dB",-12,12,0},{"2.7k dB",-12,12,0},
        {"4.05k dB",-12,12,0},{"6.1k dB",-12,12,0},
        {"9.2k dB",-12,12,0},{"14k dB",-12,12,0},
        {"Output",0,2,1}
    };
    static constexpr FxParamDesc pulsar[]={
        {"Rate Hz",.1f,20.0f,5.0f},
        {"Amount",0.0f,2.0f,0.0f},
        {"Attack s",0.0f,2.0f,0.0f},
        {"Response",0.0f,1.0f,0.0f},
        {"Target",0.0f,3.0f,1.0f}
    };
    static constexpr FxParamDesc binary[]={
        {"Detune",0.0f,1.0f,.25f},
        {"Rate Hz",0.05f,8.0f,.6f},
        {"Spread",0.0f,1.0f,.75f},
        {"Mix",0.0f,1.0f,.35f}
    };
    static constexpr FxParamDesc recurra[]={{"Mix",0,1,.18f},{"Time s",.03f,1,.32f},{"Feedback",0,.85f,.32f}};
    static constexpr FxParamDesc vastrel[]={{"Mix",0,1,.25f},{"Decay s",.2f,12,.8f},{"Room",.5f,2.5f,1}};
    if(p>=fxParamCount(type))return {"Parameter",0,1,0};
    switch(type){
        case FxType::Embera:return embera[p];
        case FxType::Prisma:return prisma[p];
        case FxType::Recurra:return recurra[p];
        case FxType::Vastrel:return vastrel[p];
        case FxType::Ampere:return ampere[p];
        case FxType::Cabeus:return cabeus[p];
        case FxType::Pulsar:return pulsar[p];
        case FxType::Binary:return binary[p];
    }
    return {"Parameter",0,1,0};
}
inline FxModule makeFxModule(FxType type) noexcept {
    FxModule m;m.type=type;
    for(size_t i=0;i<fxParamCount(type);++i)
        m.params[i]=fxParamDesc(type,i).defaultValue;
    return m;
}
struct Fx {
    // The FX rack is the sole source of processing state for a patch or master.
    std::array<FxModule,kMaxFxStages> chain{};
    uint8_t count{0};
};
inline OperatorCurve initialUnisonCurve(float base=0.0f) noexcept {
    OperatorCurve c;c.enabled=true;c.sustain=true;c.count=2;
    c.releaseIndex=0;c.decayIndex=0;
    c.points[0]={0,std::clamp(base,0.0f,1.0f)};
    c.points[1]={120.0f,std::clamp(base,0.0f,1.0f)};
    return c;
}
struct Patch {
    std::string name{"Spectrachord Init"};
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; PeakFilter peak{}; Lfo lfo{}; Fx fx{};
    float unison{0},octave{0},volume{0.8f};
    OperatorCurve unisonCurve{};
    OperatorCurve masterAmpCurve{initialMasterAmpCurve()};
    std::array<OperatorResponse,3> ampMods{};
    // Configurable played-note range for Key Map responses (K=0..100%).
    int32_t keyMapLow{0},keyMapHigh{127};
    int32_t nexdrumLow{-1},nexdrumHigh{-1},fundamentalMidi{-1};
};

/** DSP-ready patch with immutable wave tables shared across voices. */
struct DspPatch {
    int32_t keyMapLow{0},keyMapHigh{127};
    PreparedSpectralBanks spectralBanks{};
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; PeakFilter peak{}; Lfo lfo{}; Fx fx{};
    float unison{0},octave{0},volume{0.8f};
    OperatorCurve unisonCurve{};
    OperatorCurve masterAmpCurve{initialMasterAmpCurve()};
    std::array<OperatorResponse,3> ampMods{};
};


inline float normalizedKeyMap(int midi,int low,int high) noexcept {
    return std::clamp((static_cast<float>(midi)-static_cast<float>(low)) /
        static_cast<float>(std::max(1,high-low)),0.0f,1.0f);
}

// Only call off the audio thread when preparing a patch or song snapshot.
void prepareDspPatchWaveforms(DspPatch& patch);

inline DspPatch toDspPatch(const Patch& p) {
    DspPatch d;
    d.ops=p.ops;d.matrix=p.matrix;d.filter=p.filter;d.peak=p.peak;
    d.keyMapLow=p.keyMapLow;d.keyMapHigh=p.keyMapHigh;
    d.lfo=p.lfo;d.fx=p.fx;d.unison=p.unison;
    d.unisonCurve=p.unisonCurve;d.masterAmpCurve=p.masterAmpCurve;
    d.ampMods=p.ampMods;
    d.octave=p.octave;d.volume=p.volume;
    prepareDspPatchWaveforms(d);
    return d;
}
// q=0.5 preserves linear interpolation; q<0.5 puts the half-value
// earlier and q>0.5 later. Rational interpolation is audio-thread safe.
inline float curveWarp(float fraction,float q) noexcept {
    const float t=std::clamp(fraction,0.0f,1.0f);
    const float center=std::clamp(q,0.05f,0.95f);
    const float ratio=center/(1.0f-center);
    const float denominator=t+(1.0f-t)*ratio;
    return denominator>0.0f?t/denominator:t;
}
// New two-dimensional X control passes through its handle. Legacy segments
// (including saved q-only curves) preserve the original interpolation.
// Shape-preserving cubic Hermite interpolation for the X control.
// It is C1 at the handle, monotone, and continuous even when the X reaches
// either endpoint value. The control passes through (q,handleValue).
inline float curveSegment(float fraction,float a,float b,float q,
                         bool shaped,float handleValue) noexcept {
    const float t=std::clamp(fraction,0.0f,1.0f);
    // Untouched and legacy q-only curves retain exactly their old behavior.
    if(!shaped)return a+(b-a)*curveWarp(t,q);
    if(std::fabs(b-a)<=1.0e-5f)return a;
    const float position=std::clamp(q,0.05f,0.95f);
    const float target=std::clamp(handleValue,std::min(a,b),std::max(a,b));
    const float h=std::clamp((target-a)/(b-a),0.0f,1.0f);
    const float left=(h/position);
    const float right=(1.0f-h)/(1.0f-position);
    const float sum=left+right;
    // Non-negative shared tangent, bounded by the secants: no overshoot
    // and no forced flattening at an interior X.
    const float slopeA=2.0f*left*left/sum;
    const float slopeX=2.0f*left*right/sum;
    const float slopeB=2.0f*right*right/sum;

    const auto hermite=[](float u,float v0,float v1,
                          float tangent0,float tangent1) noexcept {
        const float u2=u*u;
        const float u3=u2*u;
        return (2.0f*u3-3.0f*u2+1.0f)*v0+
               (u3-2.0f*u2+u)*tangent0+
               (-2.0f*u3+3.0f*u2)*v1+
               (u3-u2)*tangent1;
    };
    float normalized=0.0f;
    if(t<=position){
        const float u=t/position;
        normalized=hermite(u,0.0f,h,
            slopeA*position,slopeX*position);
    }else{
        const float remaining=1.0f-position;
        const float u=(t-position)/remaining;
        normalized=hermite(u,h,1.0f,
            slopeX*remaining,slopeB*remaining);
    }
    return a+(b-a)*std::clamp(normalized,0.0f,1.0f);
}
struct DspCurvePoint {
    float step{0},value{0},midpoint{0.5f};
    float handleValue{0.0f};
    bool handleActive{false};
};
// Shared, immutable-after-construction storage lets voices use any number of points
// without allocating/copying the curve on the real-time callback.
struct DspCurve {
    std::shared_ptr<std::vector<DspCurvePoint>> storage{};
    const DspCurvePoint* points{nullptr};
    size_t count{0};
};
struct VoiceAutomation { DspCurve bend{},velocity{},mod{}; };


struct CurvePoint {
    float step{0.0f}, value{0.0f};
    bool free{false};
    float midpoint{0.5f}; // Incoming segment X timing (0..1)
    float handleValue{0.0f}; // Actual V/Bend/M value at the X
    bool handleActive{false}; // False keeps legacy linear/warped sound
};
struct Note {
    int32_t midi{62}; float startStep{0},lengthSteps{1};
    std::vector<CurvePoint> bend; std::vector<CurvePoint> velocity; std::vector<CurvePoint> mod;
};
struct DrumPad {
    int32_t centerMidi{62},lowMidi{62},highMidi{62}; std::string icon{"kick"}; Patch patch{}; float volume{1},pan{0}; std::string id{}; std::string name{};
};
struct Track {
    std::string name; bool drums{false},mute{false},solo{false}; float volume{0.8f},pan{0}; Patch patch{};
    std::vector<DrumPad> pads; int32_t selectedPad{0},drumZoneLow{-1},drumZoneHigh{-1}; std::vector<Note> notes;
};
struct Project {
    std::string name{"Untitled Song"};
    float bpm{112}; int32_t beats{4},divisions{4}; bool dozenal{false};
    float masterVolume{0.9f};
    Fx masterFx{};
    std::vector<Track> tracks;
};

} // namespace aiora
