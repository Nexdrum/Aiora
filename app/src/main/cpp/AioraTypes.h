#pragma once
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

struct Envelope {
    float attack{0.008f};
    float decay{0.12f};
    float sustain{0.85f};
    float release{0.25f};
};

struct Operator {
    bool enabled{true};
    Wave wave{Wave::Sine};
    float ratio{1.0f};
    float detuneCents{0.0f};
    float level{0.0f};
    Envelope env{0.005f, 0.1f, 0.8f, 0.15f};
    std::array<float,16> harm{};
    std::array<float,16> harmMute{};
    bool hasHarm{false};
    bool hasHarmMute{false};
};

struct Filter {
    FilterType type{FilterType::Lowpass};
    float cutoff{6500.0f};
    float resonance{0.7f};
    float envAmount{0.0f};
    Envelope env{0.01f,0.15f,0.7f,0.2f};
};

struct Lfo {
    float rate{5.0f};
    float amount{0.0f};
    float attack{0.0f};
    float velocitySensitivity{0.0f};
    LfoTarget target{LfoTarget::None};
};

struct Fx {
    float distortion{0.0f};
    float delay{0.18f};
    float delayTime{0.32f};
    float delayFeedback{0.32f};
    float reverb{0.25f};
};

struct ModSlot {
    ModTarget target{ModTarget::None};
    float min{0.0f};
    float max{1.0f};
};

struct Patch {
    std::string name{"Spectrachord Init"};
    std::array<Operator,6> ops{};
    std::array<std::array<float,6>,6> matrix{};
    Filter filter{};
    Envelope amp{};
    float velocityAmp{0.0f};
    float velocityFilter{0.0f};
    std::array<ModSlot,4> modSlots{};
    uint8_t modSlotCount{0};
    Lfo lfo{};
    Fx fx{};
    float unison{0.0f};
    float glide{0.0f};
    float octave{0.0f};
    float volume{0.8f};
    int32_t nexdrumLow{-1};
    int32_t nexdrumHigh{-1};
    int32_t fundamentalMidi{-1};
};

/** Audio-thread copy of Patch: fixed-size and trivially copyable, with no std::string. */
struct DspPatch {
    std::array<Operator,6> ops{};
    std::array<std::array<float,6>,6> matrix{};
    Filter filter{};
    Envelope amp{};
    float velocityAmp{0.0f};
    float velocityFilter{0.0f};
    std::array<ModSlot,4> modSlots{};
    uint8_t modSlotCount{0};
    Lfo lfo{};
    Fx fx{};
    float unison{0.0f};
    float glide{0.0f};
    float octave{0.0f};
    float volume{0.8f};
};
static_assert(std::is_trivially_copyable_v<DspPatch>);

inline DspPatch toDspPatch(const Patch& p) noexcept {
    DspPatch d;
    d.ops=p.ops; d.matrix=p.matrix; d.filter=p.filter; d.amp=p.amp;
    d.velocityAmp=p.velocityAmp; d.velocityFilter=p.velocityFilter;
    d.modSlots=p.modSlots; d.modSlotCount=p.modSlotCount; d.lfo=p.lfo; d.fx=p.fx;
    d.unison=p.unison; d.glide=p.glide; d.octave=p.octave; d.volume=p.volume;
    return d;
}

struct CurvePoint { float step{0.0f}, value{0.0f}; bool free{false}; };
struct Note {
    int32_t midi{62};
    float startStep{0.0f};
    float lengthSteps{1.0f};
    std::vector<CurvePoint> bend;
    std::vector<CurvePoint> velocity;
    std::vector<CurvePoint> mod;
};

struct DrumPad {
    int32_t centerMidi{62};
    int32_t lowMidi{62};
    int32_t highMidi{62};
    std::string icon{"kick"};
    Patch patch{};
    float volume{1.0f};
    float pan{0.0f};
};

struct Track {
    std::string name;
    bool drums{false};
    bool mute{false};
    bool solo{false};
    float volume{0.8f};
    float pan{0.0f};
    Patch patch{};
    std::vector<DrumPad> pads;
    int32_t selectedPad{0};
    int32_t drumZoneLow{-1};
    int32_t drumZoneHigh{-1};
    std::vector<Note> notes;
};

struct Project {
    float bpm{112.0f};
    int32_t beats{4};
    int32_t divisions{4};
    bool dozenal{false};
    float masterVolume{0.9f};
    float masterReverb{0.0f};
    std::vector<Track> tracks;
};

} // namespace aiora
