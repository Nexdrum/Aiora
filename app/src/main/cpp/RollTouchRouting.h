#pragma once

namespace aiora {

// The draw finger may return as Android's POINTER_DOWN when expression
// fingers remain on V and M. A hit on the resume target must work for either
// DOWN type without ending the independent expression touches.
// A primary DOWN away from the target completes and routes as before;
// secondary touches away from the target are ignored while paused.
enum class PausedRollTouch {
    Ignore,
    Resume,
    CompleteAndRoute
};

constexpr PausedRollTouch routePausedRollTouch(
    bool onRollPage,bool active,bool paused,bool primaryDown,
    bool additionalPointerDown,bool insideResumeTarget) noexcept {
    if(!active||!paused||(!primaryDown&&!additionalPointerDown))
        return PausedRollTouch::Ignore;
    if(onRollPage&&insideResumeTarget)return PausedRollTouch::Resume;
    return primaryDown?PausedRollTouch::CompleteAndRoute:
        PausedRollTouch::Ignore;
}

} // namespace aiora
