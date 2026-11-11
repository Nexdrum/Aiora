#pragma once
#include "AioraTypes.h"
#include <algorithm>
#include <cmath>

namespace aiora {

// In Free mode, note creation uses the actual touchdown within a row,
// instead of forcing the start to that row's grid boundary.
inline float rollNoteTouchStart(int step,float normalizedDown,bool free) noexcept {
    return static_cast<float>(step)+
        (free?std::clamp(normalizedDown,0.0f,1.0f):0.0f);
}

inline float rollNoteCreateLength(
    int step,float normalizedDown,float start,bool free) noexcept {
    if(free)return std::max(kMinNoteLengthSteps,
        static_cast<float>(step)+std::clamp(normalizedDown,0.0f,1.0f)-start);
    return std::max(1.0f,static_cast<float>(step)-start+1.0f);
}

// Resize relative to the original pointer down; touching near the tail
// (rather than at its exact edge) cannot cause the duration to jump.
inline float rollNoteFreeResizeLength(
    float initialLength,float dragY,float pixelsPerStep) noexcept {
    return std::max(kMinNoteLengthSteps,
        initialLength+dragY/std::max(1.0f,pixelsPerStep));
}

// Note expression recording is continuous in both modes; in Free mode
// the NOTE length must also be continuous, never ceil()'d to the grid.
inline float rollNoteDrawLength(float cursor,bool free) noexcept {
    const float length=cursor+0.5f;
    return free?std::max(kMinNoteLengthSteps,length):
        std::max(1.0f,std::ceil(length));
}

} // namespace aiora
