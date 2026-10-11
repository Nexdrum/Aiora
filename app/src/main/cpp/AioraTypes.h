#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
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
constexpr int kMaxModSlots = 64;

struct Patch {
    std::string name{"Spectrachord Init"};
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; Envelope amp{}; float velocityAmp{0},velocityFilter{0};
    std::array<ModSlot,kMaxModSlots> modSlots{}; uint8_t modSlotCount{0}; Lfo lfo{}; Fx fx{};
    float unison{0},glide{0},octave{0},volume{0.8f};
    int32_t nexdrumLow{-1},nexdrumHigh{-1},fundamentalMidi{-1};
    bool timpaniMembrane{false};
};

/** Audio-thread copy of Patch: fixed-size and trivially copyable, with no std::string. */
struct DspPatch {
    std::array<Operator,6> ops{}; std::array<std::array<float,6>,6> matrix{};
    Filter filter{}; Envelope amp{}; float velocityAmp{0},velocityFilter{0};
    std::array<ModSlot,kMaxModSlots> modSlots{}; uint8_t modSlotCount{0}; Lfo lfo{}; Fx fx{};
    float unison{0},glide{0},octave{0},volume{0.8f};
    bool timpaniMembrane{false};
};
static_assert(std::is_trivially_copyable_v<DspPatch>);

inline DspPatch toDspPatch(const Patch& p) noexcept {
    DspPatch d;d.ops=p.ops;d.matrix=p.matrix;d.filter=p.filter;d.amp=p.amp;
    d.velocityAmp=p.velocityAmp;d.velocityFilter=p.velocityFilter;d.modSlots=p.modSlots;d.modSlotCount=p.modSlotCount;
    d.lfo=p.lfo;d.fx=p.fx;d.unison=p.unison;d.glide=p.glide;d.octave=p.octave;d.volume=p.volume;
    d.timpaniMembrane=p.timpaniMembrane;return d;
}

constexpr size_t kDspCurvePoints = 26;
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
inline float curveSegment(float fraction,float a,float b,float q,
                         bool shaped,float handleValue) noexcept {
    const float t=std::clamp(fraction,0.0f,1.0f);
    if(!shaped)return a+(b-a)*curveWarp(t,q);
    // The X controls curvature *between* its anchors, never overshoot.
    // An equal-value pair has no change to shape or peak timing.
    if(std::fabs(b-a)<=1.0e-5f)return a;
    const float middle=std::clamp(q,0.05f,0.95f);
    const float peak=std::clamp(handleValue,std::min(a,b),std::max(a,b));
    const float u=t<=middle?t/middle:(t-middle)/(1.0f-middle);
    const float smooth=u*u*(3.0f-2.0f*u);
    return t<=middle?a+(peak-a)*smooth:
        peak+(b-peak)*smooth;
}
struct DspCurvePoint {
    float step{0},value{0},midpoint{0.5f};
    float handleValue{0.0f};
    bool handleActive{false};
};
struct DspCurve { std::array<DspCurvePoint,kDspCurvePoints> points{}; uint8_t count{0}; };
struct VoiceAutomation { DspCurve bend{},velocity{},mod{}; };
static_assert(std::is_trivially_copyable_v<VoiceAutomation>);

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
