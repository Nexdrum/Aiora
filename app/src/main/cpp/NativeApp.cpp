#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "AudioEngine.h"
#include "NativeUi.h"
#include "ProjectCore.h"

namespace {

constexpr char kTag[] = "AIORA";
constexpr size_t kMaxPointers = 16;
constexpr int64_t kLongPressMs = 500;

struct PointerVoice {
    int32_t pointerId{-1};
    int midi{-1};
    int voiceId{-1};
};

struct RollGesture {
    int32_t pointerId{-1};
    float downX{0.0f};
    float downY{0.0f};
    float lastX{0.0f};
    float lastY{0.0f};
    float accumX{0.0f};
    float accumY{0.0f};
    int64_t downTimeMs{0};
    bool moved{false};

    void clear() noexcept { *this = {}; pointerId = -1; }
};

struct NativeState {
    android_app* app{};
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLSurface surface{EGL_NO_SURFACE};
    EGLContext context{EGL_NO_CONTEXT};
    int width{0};
    int height{0};
    bool drawable{false};
    aiora::NativeUi ui{};
    std::array<PointerVoice, kMaxPointers> touches{};
    RollGesture rollGesture{};
};

void createDefaultProject() {
    auto& project = aiora::ProjectCore::instance();
    project.reset();
    project.addTrack(false);
    project.addTrack(false);
    project.addTrack(false);
    project.addTrack(false);
    project.addTrack(true);
    project.selectTrack(0);
}

void ensureDrumTrackSelected() {
    auto& project = aiora::ProjectCore::instance();
    const int selected = project.selectedTrack();
    if (selected >= 0 && project.trackIsDrums(selected)) return;

    const int count = project.trackCount();
    for (int i = 0; i < count; ++i) {
        if (project.trackIsDrums(i)) {
            project.selectTrack(i);
            return;
        }
    }
    project.addTrack(true);
}

bool createSurface(NativeState& state) {
    const EGLint cfgAttrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

    state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.display == EGL_NO_DISPLAY ||
        eglInitialize(state.display, nullptr, nullptr) != EGL_TRUE) {
        return false;
    }

    EGLConfig config{};
    EGLint count = 0;
    if (eglChooseConfig(state.display, cfgAttrs, &config, 1, &count) != EGL_TRUE ||
        count < 1) {
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(state.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(state.app->window, 0, 0, format);

    state.surface = eglCreateWindowSurface(
        state.display, config, state.app->window, nullptr);
    state.context = eglCreateContext(
        state.display, config, EGL_NO_CONTEXT, ctxAttrs);

    if (state.surface == EGL_NO_SURFACE || state.context == EGL_NO_CONTEXT) {
        return false;
    }

    if (eglMakeCurrent(
            state.display, state.surface, state.surface, state.context) != EGL_TRUE) {
        return false;
    }

    eglQuerySurface(state.display, state.surface, EGL_WIDTH, &state.width);
    eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &state.height);

    state.ui.resize(state.width, state.height);
    state.drawable = true;
    return true;
}

void releaseAllTouches(NativeState& state) {
    auto& audio = aiora::AudioEngine::instance();
    for (auto& touch : state.touches) {
        if (touch.voiceId >= 0) audio.noteOff(touch.voiceId);
        if (touch.midi >= 0) state.ui.setPitchActive(touch.midi, false);
        touch = {};
    }
    state.ui.clearPitchActivity();
    state.rollGesture.clear();
}

void destroySurface(NativeState& state) {
    releaseAllTouches(state);
    state.drawable = false;
    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(
            state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state.context != EGL_NO_CONTEXT) eglDestroyContext(state.display, state.context);
        if (state.surface != EGL_NO_SURFACE) eglDestroySurface(state.display, state.surface);
        eglTerminate(state.display);
    }
    state.display = EGL_NO_DISPLAY;
    state.surface = EGL_NO_SURFACE;
    state.context = EGL_NO_CONTEXT;
}

void drawFrame(NativeState& state) {
    if (!state.drawable) return;
    glViewport(0, 0, state.width, state.height);
    state.ui.render();
    eglSwapBuffers(state.display, state.surface);
}

PointerVoice* findTouch(NativeState& state, int32_t pointerId) {
    for (auto& touch : state.touches) {
        if (touch.pointerId == pointerId) return &touch;
    }
    return nullptr;
}

PointerVoice* allocateTouch(NativeState& state, int32_t pointerId) {
    if (auto* existing = findTouch(state, pointerId)) return existing;
    for (auto& touch : state.touches) {
        if (touch.pointerId < 0) {
            touch.pointerId = pointerId;
            return &touch;
        }
    }
    return nullptr;
}

void stopTouch(NativeState& state, PointerVoice& touch) {
    if (touch.voiceId >= 0) aiora::AudioEngine::instance().noteOff(touch.voiceId);
    if (touch.midi >= 0) state.ui.setPitchActive(touch.midi, false);
    touch = {};
}

void startPitch(NativeState& state, PointerVoice& touch, int midi) {
    if (touch.midi == midi && touch.voiceId >= 0) return;
    if (touch.voiceId >= 0 || touch.midi >= 0) stopTouch(state, touch);

    touch.midi = midi;
    state.ui.setPitchActive(midi, true);

    if (state.ui.page() == aiora::NativePage::Drums) {
        touch.voiceId = aiora::AudioEngine::instance().noteOnPad(
            aiora::NativeUi::padIndexForMidi(midi), midi, 0.85f);
    } else {
        touch.voiceId = aiora::AudioEngine::instance().noteOn(midi, 0.85f);
    }
}

int noteAtCell(int track, int midi, int step) {
    auto& project = aiora::ProjectCore::instance();
    const int count = project.noteCount(track);
    for (int n = 0; n < count; ++n) {
        if (project.noteMidi(track, n) != midi) continue;
        const float start = project.noteStart(track, n);
        const float length = project.noteLength(track, n);
        if (static_cast<float>(step) >= start &&
            static_cast<float>(step) < start + length) {
            return n;
        }
    }
    return -1;
}

void handleRollTap(NativeState& state, float x, float y, bool longPress) {
    auto& project = aiora::ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0) return;

    const auto hit = state.ui.hitRollCell(x, y);
    if (!hit) return;

    const int note = noteAtCell(track, hit->midi, hit->step);
    const auto mode = state.ui.rollMode();

    if (mode == aiora::RollMode::Notes) {
        if (note >= 0) project.deleteNote(track, note);
        else project.addNote(track, hit->midi, static_cast<float>(hit->step), 1.0f);
        aiora::AudioEngine::instance().syncProject();
        return;
    }

    if (note < 0) return;

    const int kind = static_cast<int>(mode) - 1;
    const float start = project.noteStart(track, note);
    const float length = project.noteLength(track, note);
    const float relative = std::clamp(
        static_cast<float>(hit->step) - start,
        0.0f,
        std::max(0.0f, length - 1.0f));

    int nearest = -1;
    float nearestDistance = 1.0e9f;
    const int points = project.curvePointCount(track, note, kind);
    for (int p = 0; p < points; ++p) {
        const float d = std::fabs(project.curvePointStep(track, note, kind, p) - relative);
        if (d < nearestDistance) {
            nearestDistance = d;
            nearest = p;
        }
    }

    if (nearest >= 0 && nearestDistance <= 0.3f) {
        if (longPress) {
            project.updateCurvePoint(
                track,
                note,
                kind,
                nearest,
                project.curvePointStep(track, note, kind, nearest),
                project.curvePointValue(track, note, kind, nearest),
                !project.curvePointFree(track, note, kind, nearest));
        } else {
            project.deleteCurvePoint(track, note, kind, nearest);
        }
        aiora::AudioEngine::instance().syncProject();
        return;
    }

    float value = 0.0f;
    if (mode == aiora::RollMode::Velocity || mode == aiora::RollMode::Mod) {
        value = std::fabs(hit->normalizedAcross - 0.5f) * 2.0f;
    }
    project.addCurvePoint(track, note, kind, relative, value, longPress);
    aiora::AudioEngine::instance().syncProject();
}

bool handleUiTap(NativeState& state, float x, float y) {
    auto& project = aiora::ProjectCore::instance();

    if (const auto nav = state.ui.hitNav(x, y)) {
        releaseAllTouches(state);
        state.ui.setPage(*nav);
        if (*nav == aiora::NativePage::Drums) ensureDrumTrackSelected();
        return true;
    }

    if (state.ui.page() == aiora::NativePage::Tracks) {
        if (const auto track = state.ui.hitTrack(x, y)) {
            project.selectTrack(*track);
            return true;
        }
        if (const auto add = state.ui.hitAddTrack(x, y)) {
            project.addTrack(*add == aiora::TrackAddKind::Drums);
            return true;
        }
    }

    if (const auto pad = state.ui.hitPadQuick(x, y)) {
        const int track = project.selectedTrack();
        if (track >= 0 && project.trackIsDrums(track)) project.selectPad(track, *pad);
        return true;
    }

    if (const auto mode = state.ui.hitRollMode(x, y)) {
        state.ui.setRollMode(*mode);
        return true;
    }

    return false;
}

void handlePointerPosition(
    NativeState& state,
    int32_t pointerId,
    float x,
    float y) {

    auto* touch = allocateTouch(state, pointerId);
    if (!touch) return;

    if (const auto midi = state.ui.hitPitch(x, y)) {
        startPitch(state, *touch, *midi);
    } else if (touch->voiceId >= 0 || touch->midi >= 0) {
        stopTouch(state, *touch);
    }
}

void beginRollGesture(
    NativeState& state,
    int32_t pointerId,
    float x,
    float y,
    int64_t timeMs) {

    state.rollGesture.pointerId = pointerId;
    state.rollGesture.downX = x;
    state.rollGesture.downY = y;
    state.rollGesture.lastX = x;
    state.rollGesture.lastY = y;
    state.rollGesture.accumX = 0.0f;
    state.rollGesture.accumY = 0.0f;
    state.rollGesture.downTimeMs = timeMs;
    state.rollGesture.moved = false;
}

void moveRollGesture(NativeState& state, float x, float y) {
    auto& g = state.rollGesture;
    if (g.pointerId < 0) return;

    const float dx = x - g.lastX;
    const float dy = y - g.lastY;
    g.lastX = x;
    g.lastY = y;
    g.accumX += dx;
    g.accumY += dy;

    const float totalDx = x - g.downX;
    const float totalDy = y - g.downY;
    if (std::hypot(totalDx, totalDy) > 8.0f) g.moved = true;

    const float threshold = std::max(16.0f, state.ui.rollCellPixels() * 0.72f);
    while (std::fabs(g.accumX) >= threshold) {
        const int delta = g.accumX < 0.0f ? 1 : -1;
        state.ui.scrollRoll(delta, 0);
        g.accumX += g.accumX < 0.0f ? threshold : -threshold;
    }
    while (std::fabs(g.accumY) >= threshold) {
        const int delta = g.accumY < 0.0f ? 1 : -1;
        state.ui.scrollRoll(0, delta);
        g.accumY += g.accumY < 0.0f ? threshold : -threshold;
    }
}

int32_t handleInput(android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    auto& state = *static_cast<NativeState*>(app->userData);
    const int packedAction = AMotionEvent_getAction(event);
    const int action = packedAction & AMOTION_EVENT_ACTION_MASK;
    const size_t actionIndex = static_cast<size_t>(
        (packedAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
        AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            const size_t index = action == AMOTION_EVENT_ACTION_DOWN ? 0u : actionIndex;
            const int32_t pointerId = AMotionEvent_getPointerId(event, index);
            const float x = AMotionEvent_getX(event, index);
            const float y = AMotionEvent_getY(event, index);

            if (handleUiTap(state, x, y)) return 1;

            if (state.ui.page() == aiora::NativePage::Roll) {
                if (state.rollGesture.pointerId < 0) {
                    beginRollGesture(
                        state,
                        pointerId,
                        x,
                        y,
                        static_cast<int64_t>(AMotionEvent_getEventTime(event)));
                }
                return 1;
            }

            handlePointerPosition(state, pointerId, x, y);
            return 1;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            if (state.ui.page() == aiora::NativePage::Roll &&
                state.rollGesture.pointerId >= 0) {
                const size_t count = AMotionEvent_getPointerCount(event);
                for (size_t i = 0; i < count; ++i) {
                    if (AMotionEvent_getPointerId(event, i) == state.rollGesture.pointerId) {
                        moveRollGesture(
                            state,
                            AMotionEvent_getX(event, i),
                            AMotionEvent_getY(event, i));
                        break;
                    }
                }
                return 1;
            }

            const size_t count = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < count; ++i) {
                handlePointerPosition(
                    state,
                    AMotionEvent_getPointerId(event, i),
                    AMotionEvent_getX(event, i),
                    AMotionEvent_getY(event, i));
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            const size_t index = action == AMOTION_EVENT_ACTION_UP ? 0u : actionIndex;
            const int32_t pointerId = AMotionEvent_getPointerId(event, index);
            const float x = AMotionEvent_getX(event, index);
            const float y = AMotionEvent_getY(event, index);

            if (state.rollGesture.pointerId == pointerId) {
                const int64_t heldMs =
                    static_cast<int64_t>(AMotionEvent_getEventTime(event)) -
                    state.rollGesture.downTimeMs;
                if (!state.rollGesture.moved) {
                    handleRollTap(state, x, y, heldMs >= kLongPressMs);
                }
                state.rollGesture.clear();
                return 1;
            }

            if (auto* touch = findTouch(state, pointerId)) stopTouch(state, *touch);
            return 1;
        }

        case AMOTION_EVENT_ACTION_CANCEL:
            releaseAllTouches(state);
            aiora::AudioEngine::instance().panic();
            return 1;

        default:
            return 1;
    }
}

void handleCommand(android_app* app, int32_t command) {
    auto& state = *static_cast<NativeState*>(app->userData);

    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && createSurface(state)) {
                aiora::AudioEngine::instance().start();
                drawFrame(state);
            }
            break;

        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (state.drawable) {
                eglQuerySurface(state.display, state.surface, EGL_WIDTH, &state.width);
                eglQuerySurface(state.display, state.surface, EGL_HEIGHT, &state.height);
                state.ui.resize(state.width, state.height);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            destroySurface(state);
            break;

        case APP_CMD_GAINED_FOCUS:
            aiora::AudioEngine::instance().start();
            break;

        case APP_CMD_LOST_FOCUS:
            releaseAllTouches(state);
            break;

        case APP_CMD_LOW_MEMORY:
            aiora::AudioEngine::instance().collectRetiredSnapshots();
            break;

        default:
            break;
    }
}

} // namespace

void android_main(android_app* app) {
    NativeState state;
    state.app = app;
    for (auto& touch : state.touches) touch.pointerId = -1;
    state.rollGesture.clear();

    app->userData = &state;
    app->onAppCmd = handleCommand;
    app->onInputEvent = handleInput;

    __android_log_print(
        ANDROID_LOG_INFO, kTag,
        "AIORA C++ NativeActivity boot");

    createDefaultProject();

    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;

        while (ALooper_pollOnce(
                   state.drawable ? 0 : -1,
                   nullptr,
                   &events,
                   reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);

            if (app->destroyRequested != 0) {
                aiora::AudioEngine::instance().stop();
                destroySurface(state);
                return;
            }
        }

        drawFrame(state);
    }
}
