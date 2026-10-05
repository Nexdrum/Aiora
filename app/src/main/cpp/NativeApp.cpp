#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <array>

#include "AudioEngine.h"
#include "NativeUi.h"
#include "ProjectCore.h"

namespace {

constexpr char kTag[] = "AIORA";
constexpr size_t kMaxPointers = 16;

struct PointerVoice {
    int32_t pointerId{-1};
    int midi{-1};
    int voiceId{-1};
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
};

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
}

void destroySurface(NativeState& state) {
    releaseAllTouches(state);
    state.drawable = false;
    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(
            state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state.context != EGL_NO_CONTEXT) {
            eglDestroyContext(state.display, state.context);
        }
        if (state.surface != EGL_NO_SURFACE) {
            eglDestroySurface(state.display, state.surface);
        }
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
    if (touch.voiceId >= 0) {
        aiora::AudioEngine::instance().noteOff(touch.voiceId);
    }
    if (touch.midi >= 0) {
        state.ui.setPitchActive(touch.midi, false);
    }
    touch = {};
}

void startPitch(NativeState& state, PointerVoice& touch, int midi) {
    if (touch.midi == midi && touch.voiceId >= 0) return;
    if (touch.voiceId >= 0 || touch.midi >= 0) {
        stopTouch(state, touch);
    }

    touch.midi = midi;
    state.ui.setPitchActive(midi, true);

    if (state.ui.page() == aiora::NativePage::Drums) {
        touch.voiceId = aiora::AudioEngine::instance().noteOnPad(
            aiora::NativeUi::padIndexForMidi(midi), midi, 0.85f);
    } else {
        touch.voiceId = aiora::AudioEngine::instance().noteOn(midi, 0.85f);
    }
}

void handlePointerPosition(
    NativeState& state,
    int32_t pointerId,
    float x,
    float y,
    bool allowNav) {

    if (allowNav) {
        if (const auto nav = state.ui.hitNav(x, y)) {
            releaseAllTouches(state);
            state.ui.setPage(*nav);
            return;
        }
    }

    auto* touch = allocateTouch(state, pointerId);
    if (!touch) return;

    if (const auto midi = state.ui.hitPitch(x, y)) {
        startPitch(state, *touch, *midi);
    } else if (touch->voiceId >= 0 || touch->midi >= 0) {
        stopTouch(state, *touch);
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
            handlePointerPosition(
                state,
                pointerId,
                AMotionEvent_getX(event, index),
                AMotionEvent_getY(event, index),
                true);
            return 1;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            const size_t count = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < count; ++i) {
                const int32_t pointerId = AMotionEvent_getPointerId(event, i);
                handlePointerPosition(
                    state,
                    pointerId,
                    AMotionEvent_getX(event, i),
                    AMotionEvent_getY(event, i),
                    false);
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            const size_t index = action == AMOTION_EVENT_ACTION_UP ? 0u : actionIndex;
            const int32_t pointerId = AMotionEvent_getPointerId(event, index);
            if (auto* touch = findTouch(state, pointerId)) {
                stopTouch(state, *touch);
            }
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
    app_dummy();

    NativeState state;
    state.app = app;
    for (auto& touch : state.touches) touch.pointerId = -1;

    app->userData = &state;
    app->onAppCmd = handleCommand;
    app->onInputEvent = handleInput;

    __android_log_print(
        ANDROID_LOG_INFO, kTag,
        "AIORA C++ NativeActivity boot");

    aiora::ProjectCore::instance().reset();

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
