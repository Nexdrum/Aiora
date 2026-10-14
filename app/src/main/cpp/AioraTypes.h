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

enum class Wave : uint8_t { Sine, Saw, Square, Triangle, Custom, Noise };
enum class FilterType : uint8_t { Lowpass, Highpass, Bandpass };
enum class LfoTarget : uint8_t { None, Pitch, Filter, Amp };
enum class ModTarget : uint8_t {
    None, Cutoff, Resonance, FilterEnv,
    AmpAttack, AmpDecay, AmpSustain, AmpRelease,
    FilterAttack, FilterDecay, FilterSustain, FilterRelease,
    Op1, Op2, Op3, Op4, Op5, Op6,
    Morph1, Morph2, Morph3, Morph4, Morph5, Morph6,
    Fm, LfoAmount, LfoRate, Unison, Volume
};

struct Envelope { float attack{0.008f}, decay{0.12f}, sustain{0.85f}, release{0.25f}; };
struct Operator {
    bool enabled{true};
    Wave wave{Wave::Sine};
    float ratio{1.0f};
    float semitoneOffset{0.0f};
    float detuneCents{0.0f};
    float level{0.0f};
    Envelope env{0.005f,0.1f,0.8f,0.15f};
    std::array<float,16> harm{}; std::array<float,16> harmMute{};
    bool hasHarm{false},hasHarmMute{false};
};
struct Filter { FilterType type{FilterType::Lowpass}; float cutoff{6500},resonance{0.7f},envAmount{0}; Envelope env{0.01f,0.15f,0.7f,0.2f}; };
struct Lfo { float rate{5},amount{0},attack{0},velocitySensitivity{0}; LfoTarget target{LfoTarget::None}; };
struct Fx { float distortion{0},delay{0.18f},delayTime{0.32f},delayFeedback{0.32f},reverb{0.25f}; };
struct ModSlot { ModTarget target{ModTarget::None}; float min{0},max{1}; };

struct Patch {
    std::string name{"Spectrachord Init"};
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; Envelope amp{}; float velocityAmp{0},velocityFilter{0};
    std::vector<ModSlot> modSlots{}; size_t modSlotCount{0}; Lfo lfo{}; Fx fx{};
    float unison{0},glide{0},octave{0},volume{0.8f};
    int32_t nexdrumLow{-1},nexdrumHigh{-1},fundamentalMidi{-1};
    bool timpaniMembrane{false};
    float drumPitchDrop{0.0f},drumPitchDropSeconds{0.0f};
    uint8_t cymbalModel{0}; // 0=normal, 1=crash, 2=ride
};

/** Audio-thread copy of Patch: fixed-size and trivially copyable, with no std::string. */
struct DspPatch {
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; Envelope amp{}; float velocityAmp{0},velocityFilter{0};
    std::shared_ptr<const std::vector<ModSlot>> modSlots{}; size_t modSlotCount{0}; Lfo lfo{}; Fx fx{};
    float unison{0},glide{0},octave{0},volume{0.8f};
    bool timpaniMembrane{false};
    float drumPitchDrop{0.0f},drumPitchDropSeconds{0.0f};
    uint8_t cymbalModel{0}; // 0=normal, 1=crash, 2=ride
};


inline DspPatch toDspPatch(const Patch& p) {
    DspPatch d;d.ops=p.ops;d.matrix=p.matrix;d.filter=p.filter;d.amp=p.amp;
    d.velocityAmp=p.velocityAmp;d.velocityFilter=p.velocityFilter;
    if(!p.modSlots.empty())d.modSlots=std::make_shared<const std::vector<ModSlot>>(p.modSlots);
    d.modSlotCount=p.modSlotCount;
    d.lfo=p.lfo;d.fx=p.fx;d.unison=p.unison;d.glide=p.glide;d.octave=p.octave;d.volume=p.volume;
    d.timpaniMembrane=p.timpaniMembrane;
    d.drumPitchDrop=p.drumPitchDrop;d.drumPitchDropSeconds=p.drumPitchDropSeconds;
    d.cymbalModel=p.cymbalModel;return d;
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
    float bpm{112}; int32_t beats{4},divisions{4}; bool dozenal{false}; float masterVolume{0.9f},masterReverb{0}; std::vector<Track> tracks;
};

} // namespace aiora
