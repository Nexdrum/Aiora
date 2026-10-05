#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>

#include "AudioEngine.h"
#include "ProjectCore.h"

namespace {

constexpr char kTag[] = "AIORA";

struct NativeState {
    android_app* app{};
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLSurface surface{EGL_NO_SURFACE};
    EGLContext context{EGL_NO_CONTEXT};
    int width{0};
    int height{0};
    bool drawable{false};
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

    glDisable(GL_DEPTH_TEST);
    state.drawable = true;
    return true;
}

void destroySurface(NativeState& state) {
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
    glClearColor(0.0627f, 0.0706f, 0.0863f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // First native milestone: EGL + Oboe + C++ project core are live.
    // The finalized AIORA panels/grids will be rendered here next.
    eglSwapBuffers(state.display, state.surface);
}

int32_t handleInput(android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    auto& state = *static_cast<NativeState*>(app->userData);
    const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;

    if (action == AMOTION_EVENT_ACTION_DOWN ||
        action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        const size_t index =
            action == AMOTION_EVENT_ACTION_POINTER_DOWN
                ? static_cast<size_t>(
                      (AMotionEvent_getAction(event) &
                       AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT)
                : 0u;

        const float x = AMotionEvent_getX(event, index);
        const float y = AMotionEvent_getY(event, index);

        const float nx = state.width > 0
            ? std::clamp(x / static_cast<float>(state.width), 0.0f, 1.0f)
            : 0.5f;
        const float ny = state.height > 0
            ? std::clamp(y / static_cast<float>(state.height), 0.0f, 1.0f)
            : 0.5f;

        // Temporary smoke-test input until the native 7x7 AIORA grid lands:
        // horizontal position selects pitch, vertical position selects velocity.
        const int midi = 38 + static_cast<int>(nx * 48.0f);
        const float velocity = 1.0f - ny * 0.65f;
        aiora::AudioEngine::instance().noteOn(midi, velocity);
        return 1;
    }

    return 1;
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

        case APP_CMD_TERM_WINDOW:
            destroySurface(state);
            break;

        case APP_CMD_GAINED_FOCUS:
            aiora::AudioEngine::instance().start();
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
