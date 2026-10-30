#pragma once
#include <vector>
#include "AioraTypes.h"
#include "FxProcessor.h"

namespace aiora {


struct PlaybackEvent {
    int32_t midi{62};
    float lengthSteps{1.0f};
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
