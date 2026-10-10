#pragma once
#include <vector>
#include "AioraTypes.h"

namespace aiora {

inline constexpr size_t kMaxPlaybackFxBuses = 48;

struct PlaybackEvent {
    int32_t midi{62};
    float lengthSteps{1.0f};
    float offsetSteps{0.0f}; // fractional note onset within its time-grid step
    DspPatch patch{};
    VoiceAutomation automation{};
    float gainLeft{1.0f};
    float gainRight{1.0f};
    int16_t fxBus{-1};
};

struct PlaybackStep {
    std::vector<PlaybackEvent> events;
};

struct PlaybackSnapshot {
    float bpm{112.0f};
    int32_t divisions{4};
    int32_t lengthSteps{16};
    float masterVolume{0.9f};
    float masterReverb{0.0f};
    std::vector<Fx> fxBuses;
    std::vector<PlaybackStep> steps;
};

} // namespace aiora
