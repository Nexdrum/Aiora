#pragma once

namespace aiora {

// A drawing paused at the viewport edge may be resumed only by a
// primary touch within Roll's resume target. Any other primary touch
// finishes the note and MUST be dispatched normally (including scrolling).
enum class PausedRollTouch {
    Ignore,
    Resume,
    CompleteAndRoute
};

constexpr PausedRollTouch routePausedRollTouch(
    bool onRollPage,bool active,bool paused,bool primaryDown,
    bool insideResumeTarget) noexcept {
    if(!active||!paused||!primaryDown)return PausedRollTouch::Ignore;
    if(onRollPage&&insideResumeTarget)return PausedRollTouch::Resume;
    return PausedRollTouch::CompleteAndRoute;
}

} // namespace aiora
