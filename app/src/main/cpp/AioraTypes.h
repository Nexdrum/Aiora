#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace aiora {

enum class Wave : uint8_t { Sine, Saw, Square, Triangle, Custom, Noise };
enum class FilterType : uint8_t { Lowpass, Highpass, Bandpass };
enum class LfoTarget : uint8_t { None, Pitch, Filter, Amp };

struct Envelope { float attack{0.008f}, decay{0.12f}, sustain{0.85f}, release{0.25f}; };

struct Operator {
    bool enabled{true};
    Wave wave{Wave::Sine};
    float ratio{1.0f};
    float detuneCents{0.0f};
    float level{0.0f};
    Envelope env{};
    std::array<float,16> harm{};
    std::array<float,16> harmMute{};
};

struct Filter {
    FilterType type{FilterType::Lowpass};
    float cutoff{6500.0f};
    float resonance{0.7f};
    float envAmount{0.0f};
    Envelope env{0.01f,0.15f,0.7f,0.2f};
};

struct Lfo { float rate{5.0f}, amount{0.0f}, attack{0.0f}, velocitySensitivity{0.0f}; LfoTarget target{LfoTarget::None}; };
struct Fx { float distortion{0.0f}, delay{0.18f}, delayTime{0.32f}, delayFeedback{0.32f}, reverb{0.25f}; };
struct ModSlot { std::string target; float min{0.0f}, max{1.0f}; };

struct Patch {
    std::string name{"Spectrachord Init"};
    std::array<Operator,6> ops{};
    std::array<std::array<float,6>,6> matrix{};
    Filter filter{};
    Envelope amp{};
    float velocityAmp{0.0f}, velocityFilter{0.0f};
    std::array<ModSlot,4> modSlots{};
    uint8_t modSlotCount{0};
    Lfo lfo{};
    Fx fx{};
    float unison{0.0f}, glide{0.0f}, octave{0.0f}, volume{0.8f};
};

struct CurvePoint { float step{0.0f}, value{0.0f}; bool free{false}; };
struct Note {
    int32_t midi{62};
    float startStep{0.0f}, lengthSteps{1.0f};
    std::vector<CurvePoint> bend;
    std::vector<CurvePoint> velocity;
    std::vector<CurvePoint> mod;
};

struct DrumPad {
    int32_t centerMidi{62}, lowMidi{62}, highMidi{62};
    std::string icon{"kick"};
    Patch patch{};
    float volume{1.0f}, pan{0.0f};
};

struct Track {
    std::string name;
    bool drums{false}, mute{false}, solo{false};
    float volume{0.8f}, pan{0.0f};
    Patch patch{};
    std::vector<DrumPad> pads;
    std::vector<Note> notes;
};

struct Project {
    float bpm{112.0f};
    int32_t beats{4}, divisions{4};
    bool dozenal{false};
    std::vector<Track> tracks;
};

} // namespace aiora
