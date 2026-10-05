#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include "AudioEngine.h"
#include "NativeEditor.h"
#include "NativeOverlay.h"
#include "NativeUi.h"
#include "ProjectCore.h"
#include "ProjectStorage.h"

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
    int32_t editorPointerId{-1};
    int32_t trackControlPointerId{-1};
    int32_t drumControlPointerId{-1};
    int editorPreviewVoice{-1};
    int64_t editorPreviewStopMs{0};
    std::string autosavePath{};
    int64_t autosaveDueMs{0};
    bool autosaveDirty{false};
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
    aiora::NativeOverlay::instance().init();
    state.drawable = true;
    return true;
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void saveProjectNow(NativeState& state) {
    if(state.autosavePath.empty())return;
    std::string error;
    if(aiora::saveProjectFile(state.autosavePath,aiora::ProjectCore::instance().projectCopy(),&error)){
        state.autosaveDirty=false;
        state.autosaveDueMs=0;
    }else{
        __android_log_print(ANDROID_LOG_WARN,kTag,"autosave failed: %s",error.c_str());
    }
}

void scheduleAutosave(NativeState& state,int64_t delayMs=350) {
    state.autosaveDirty=true;
    state.autosaveDueMs=nowMs()+delayMs;
}

void serviceAutosave(NativeState& state) {
    if(state.autosaveDirty&&state.autosaveDueMs>0&&nowMs()>=state.autosaveDueMs)saveProjectNow(state);
}

void serviceEditorPreview(NativeState& state) {
    if(state.editorPreviewVoice>=0 && nowMs()>=state.editorPreviewStopMs){
        aiora::AudioEngine::instance().noteOff(state.editorPreviewVoice);
        state.editorPreviewVoice=-1;
        state.editorPreviewStopMs=0;
    }
}

void previewDrumPitch(NativeState& state,int midi) {
    if(state.editorPreviewVoice>=0){
        aiora::AudioEngine::instance().noteOff(state.editorPreviewVoice);
        state.editorPreviewVoice=-1;
    }
    auto& project=aiora::ProjectCore::instance();const int track=project.selectedTrack();
    if(track<0||!project.trackIsDrums(track))return;
    const int pad=project.selectedPad(track);
    if(pad<0||pad>=project.padCount(track))return;
    state.editorPreviewVoice=aiora::AudioEngine::instance().noteOnPad(pad,midi,0.88f);
    if(state.editorPreviewVoice>=0)state.editorPreviewStopMs=nowMs()+420;
}

void previewEditorPatch(NativeState& state) {
    auto& project=aiora::ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return;

    if(project.trackIsDrums(track)){
        const int pad=project.selectedPad(track);
        if(pad>=0&&pad<project.padCount(track))previewDrumPitch(state,project.padCenter(track,pad));
        return;
    }

    if(state.editorPreviewVoice>=0){
        aiora::AudioEngine::instance().noteOff(state.editorPreviewVoice);
        state.editorPreviewVoice=-1;
    }
    state.editorPreviewVoice=aiora::AudioEngine::instance().noteOn(60,0.82f);
    if(state.editorPreviewVoice>=0)state.editorPreviewStopMs=nowMs()+420;
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
    aiora::NativeEditor::instance().cancel();
    state.editorPointerId=-1;
    state.trackControlPointerId=-1;
    state.drumControlPointerId=-1;
    state.ui.trackPointerUp();
    state.ui.drumPointerUp();
    if(state.editorPreviewVoice>=0){
        audio.noteOff(state.editorPreviewVoice);
        state.editorPreviewVoice=-1;
        state.editorPreviewStopMs=0;
    }
}

void destroySurface(NativeState& state) {
    releaseAllTouches(state);
    state.drawable = false;
    if (state.display != EGL_NO_DISPLAY) {
        aiora::NativeOverlay::instance().shutdown();
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
    serviceEditorPreview(state);
    serviceAutosave(state);
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
        scheduleAutosave(state);
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
        scheduleAutosave(state);
        return;
    }

    float value = 0.0f;
    if (mode == aiora::RollMode::Velocity || mode == aiora::RollMode::Mod) {
        value = std::fabs(hit->normalizedAcross - 0.5f) * 2.0f;
    }
    project.addCurvePoint(track, note, kind, relative, value, longPress);
    aiora::AudioEngine::instance().syncProject();
    scheduleAutosave(state);
}

bool handleUiTap(NativeState& state, float x, float y) {
    auto& project = aiora::ProjectCore::instance();
    auto& audio = aiora::AudioEngine::instance();

    if (const auto action = state.ui.hitHeader(x, y)) {
        switch(*action) {
            case aiora::HeaderAction::BpmDown:
                project.setBpm(project.bpm()-1.0f);
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::BpmUp:
                project.setBpm(project.bpm()+1.0f);
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::BeatsDown:
                project.setSignature(std::max(1,project.beats()-1),project.divisions());
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::BeatsUp:
                project.setSignature(std::min(12,project.beats()+1),project.divisions());
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::DivDown:
                project.setSignature(project.beats(),std::max(1,project.divisions()-1));
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::DivUp:
                project.setSignature(project.beats(),std::min(12,project.divisions()+1));
                if(audio.transportPlaying()) audio.syncProject();
                break;
            case aiora::HeaderAction::DozenalToggle:
                project.setDozenal(!project.dozenal());
                break;
            case aiora::HeaderAction::TransportToggle:
                if(audio.transportPlaying()) audio.stopTransport();
                else audio.playTransport();
                break;
        }
        if(*action!=aiora::HeaderAction::TransportToggle)scheduleAutosave(state);
        return true;
    }

    if (const auto nav = state.ui.hitNav(x, y)) {
        releaseAllTouches(state);
        state.ui.setPage(*nav);
        state.ui.resetDrumRangeArm();
        if (*nav == aiora::NativePage::Drums) ensureDrumTrackSelected();
        return true;
    }

    if (state.ui.page() == aiora::NativePage::Tracks) {
        if (const auto track = state.ui.hitTrack(x, y)) {
            project.selectTrack(*track);
            state.ui.resetDrumRangeArm();
            return true;
        }
        if (const auto add = state.ui.hitAddTrack(x, y)) {
            project.addTrack(*add == aiora::TrackAddKind::Drums);
            scheduleAutosave(state);
            return true;
        }
    }

    if (const auto pad = state.ui.hitPadQuick(x, y)) {
        const int track = project.selectedTrack();
        if (track >= 0 && project.trackIsDrums(track)) {
            project.selectPad(track, *pad);
            state.ui.resetDrumRangeArm();
            scheduleAutosave(state);
            if(state.ui.page()==aiora::NativePage::Synth || state.ui.page()==aiora::NativePage::Fx){
                previewEditorPatch(state);
            }
        }
        return true;
    }

    if(state.ui.page()==aiora::NativePage::Drums){
        if(const auto midi=state.ui.hitPitch(x,y)){
            if(state.ui.drumPitchTap(*midi)){
                audio.syncProject();
                scheduleAutosave(state);
                previewDrumPitch(state,*midi);
            }
            return true;
        }
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

            if(state.ui.page()==aiora::NativePage::Tracks&&state.ui.trackPointerDown(x,y)){
                state.trackControlPointerId=pointerId;
                return 1;
            }

            if(state.ui.page()==aiora::NativePage::Drums&&state.ui.drumPointerDown(x,y)){
                state.drumControlPointerId=pointerId;
                return 1;
            }

            if (handleUiTap(state, x, y)) return 1;

            if (state.ui.page() == aiora::NativePage::Synth ||
                state.ui.page() == aiora::NativePage::Fx) {
                const auto editorPage = state.ui.page() == aiora::NativePage::Synth
                    ? aiora::EditorPage::Synth : aiora::EditorPage::Fx;
                if(aiora::NativeEditor::instance().pointerDown(editorPage,x,y)){
                    state.editorPointerId=pointerId;
                    return 1;
                }
            }

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
            if(state.trackControlPointerId>=0){
                const size_t count=AMotionEvent_getPointerCount(event);
                for(size_t i=0;i<count;++i){
                    if(AMotionEvent_getPointerId(event,i)==state.trackControlPointerId){
                        state.ui.trackPointerMove(AMotionEvent_getX(event,i),AMotionEvent_getY(event,i));
                        break;
                    }
                }
                return 1;
            }

            if(state.drumControlPointerId>=0){
                const size_t count=AMotionEvent_getPointerCount(event);
                for(size_t i=0;i<count;++i){
                    if(AMotionEvent_getPointerId(event,i)==state.drumControlPointerId){
                        state.ui.drumPointerMove(AMotionEvent_getX(event,i),AMotionEvent_getY(event,i));
                        break;
                    }
                }
                return 1;
            }

            if(state.editorPointerId>=0){
                const size_t count=AMotionEvent_getPointerCount(event);
                for(size_t i=0;i<count;++i){
                    if(AMotionEvent_getPointerId(event,i)==state.editorPointerId){
                        aiora::NativeEditor::instance().pointerMove(
                            AMotionEvent_getX(event,i),AMotionEvent_getY(event,i));
                        break;
                    }
                }
                return 1;
            }

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

            if(state.trackControlPointerId==pointerId){
                const bool changed=state.ui.trackPointerUp();
                state.trackControlPointerId=-1;
                if(changed){
                    aiora::AudioEngine::instance().syncProject();
                    scheduleAutosave(state);
                }
                return 1;
            }

            if(state.drumControlPointerId==pointerId){
                const bool changed=state.ui.drumPointerUp();
                state.drumControlPointerId=-1;
                if(changed){
                    aiora::AudioEngine::instance().syncProject();
                    scheduleAutosave(state);
                    previewEditorPatch(state);
                }
                return 1;
            }

            if(state.editorPointerId==pointerId){
                const bool changed=aiora::NativeEditor::instance().pointerUp();
                state.editorPointerId=-1;
                if(changed){
                    previewEditorPatch(state);
                    scheduleAutosave(state);
                    aiora::AudioEngine::instance().syncProject();
                }
                return 1;
            }

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
            if(state.trackControlPointerId>=0){
                const bool changed=state.ui.trackPointerUp();
                state.trackControlPointerId=-1;
                if(changed){
                    aiora::AudioEngine::instance().syncProject();
                    scheduleAutosave(state);
                }
            }
            if(state.drumControlPointerId>=0){
                const bool changed=state.ui.drumPointerUp();
                state.drumControlPointerId=-1;
                if(changed){
                    aiora::AudioEngine::instance().syncProject();
                    scheduleAutosave(state);
                }
            }
            aiora::NativeEditor::instance().cancel();
            state.editorPointerId=-1;
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
            if(state.autosaveDirty)saveProjectNow(state);
            break;

        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
            if(state.autosaveDirty)saveProjectNow(state);
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

    if(app->activity&&app->activity->internalDataPath){
        state.autosavePath=std::string(app->activity->internalDataPath)+"/aiora.json";
    }

    aiora::Project restored;
    std::string restoreError;
    if(!state.autosavePath.empty()&&aiora::loadProjectFile(state.autosavePath,restored,&restoreError)){
        aiora::ProjectCore::instance().replaceProject(std::move(restored),0);
        __android_log_print(ANDROID_LOG_INFO,kTag,"restored AIORA autosave");
    }else{
        createDefaultProject();
    }

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
                if(state.autosaveDirty)saveProjectNow(state);
                aiora::AudioEngine::instance().stop();
                destroySurface(state);
                return;
            }
        }

        drawFrame(state);
    }
}
