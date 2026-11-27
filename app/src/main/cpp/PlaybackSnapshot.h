#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "AioraTypes.h"
#include "FxProcessor.h"

namespace aiora {

// True fractional-length scheduled gates: only the one-sample safety
// minimum remains; a 90ms hold would defeat rapid notes.
inline uint64_t playbackGateFrames(double seconds,int sampleRate) noexcept {
    return static_cast<uint64_t>(std::max(1.0,
        std::ceil(std::max(0.0,seconds)*std::max(1,sampleRate))));
}


struct PlaybackEvent {
    int32_t midi{62};
    float lengthSteps{1.0f};
    double durationSeconds{0.0};
    float offsetSteps{0.0f}; // fractional note onset within its time-grid step
    DspPatch patch{};
    VoiceAutomation automation{};
    float gainLeft{1.0f};
    float gainRight{1.0f};
    int32_t fxBus{-1};
};

struct PlaybackStep {
    std::vector<PlaybackEvent> events;
};

struct PlaybackSnapshot {
    float bpm{112.0f};
    std::vector<double> secondsPerStep;
    int32_t divisions{4};
    int32_t lengthSteps{16};
    float masterVolume{0.9f};
    Fx masterEffects{};
    std::vector<Fx> fxBuses;
    // A pad FX bus may feed its drum-track FX bus before song master.
    std::vector<int32_t> fxBusParent;
    std::vector<FxProcessor> preparedFx;
    std::vector<float> scratchLeft,scratchRight;
    std::vector<PlaybackStep> steps;
};

} // namespace aiora
