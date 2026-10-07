#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "AudioEngine.h"
#include "AiHeuristic.h"
#include "FactoryPresets.h"
#include "NativeEditor.h"
#include "NativeOverlay.h"
#include "NativeUi.h"
#include "ProjectCore.h"
#include "ProjectCodec.h"
#include "ProjectStorage.h"
#include "SongExport.h"

namespace {

constexpr char kTag[] = "AIORA";
constexpr size_t kMaxPointers = 16;
constexpr int64_t kLongPressMs = 450;
constexpr int64_t kRollDoubleTapMs = 360;
constexpr float kMaxRollNoteLengthSteps = 4096.0f;
constexpr int kRequestSaveJson = 4101;
constexpr int kRequestLoadJson = 4102;
constexpr int kRequestExportWav = 4103;
constexpr int kRequestExportMidi = 4104;
constexpr int kRequestExportPatch = 4105;
constexpr int kRequestImportPatch = 4106;
constexpr int kRequestRenameTrack = 4201;
constexpr int kRequestRenamePad = 4202;
constexpr int kRequestOperatorRatio = 4203;

struct PointerVoice {
    int32_t pointerId{-1};
    int midi{-1};
    int voiceId{-1};
};

enum class RollGestureKind : uint8_t {
    None,
    PitchScroll,
    TimeScroll,
    SelectTool,
    Lasso,
    NoteEdit,
    AutomationEdit,
    GroupAutomation
};

enum class RollNoteEdit : uint8_t {
    None,
    Create,
    Move,
    Resize,
    GroupMove
};

struct RollClipboardTrack {
    int track{-1};
    std::vector<aiora::Note> notes{};
};

struct GroupAutomationCurve {
    bool initialized{false};
    float start{0.0f};
    float end{0.0f};
    std::vector<aiora::RollGroupPoint> points{};
};

struct RollGesture {
    int32_t pointerId{-1};
    RollGestureKind kind{RollGestureKind::None};
    RollNoteEdit noteEdit{RollNoteEdit::None};
    float downX{0.0f};
    float downY{0.0f};
    float lastX{0.0f};
    float lastY{0.0f};
    float accumX{0.0f};
    float accumY{0.0f};
    int64_t downTimeMs{0};
    bool moved{false};
    bool longPressTriggered{false};

    int track{-1};
    int noteIndex{-1};
    int curveKind{-1};
    int curvePoint{-1};
    bool curveIsNew{false};
    bool curveFree{false};
    int noteMidi{-1};
    float noteStart{0.0f};
    float noteLength{1.0f};

    std::vector<int> groupIndices{};
    int dragCellMidi{-1};
    int dragCellStep{-1};
    int groupAppliedMidiDelta{0};
    int groupAppliedStepDelta{0};

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
    std::vector<RollClipboardTrack> rollClipboard{};
    bool rollClipboardAllTracks{false};
    std::array<GroupAutomationCurve,2> rollGroupAutomation{};
    int64_t rollLastBeatTapMs{0};
    int rollLastBeatTapStep{-1};
    int32_t editorPointerId{-1};
    int32_t trackControlPointerId{-1};
    int32_t drumControlPointerId{-1};
    int32_t pageScrollPointerId{-1};
    float pageScrollDownX{0.0f};
    float pageScrollDownY{0.0f};
    float pageScrollLastY{0.0f};
    bool pageScrollMoved{false};
    int editorPreviewVoice{-1};
    int64_t editorPreviewStopMs{0};
    std::string autosavePath{};
    std::string jsonExportPath{};
    std::string wavExportPath{};
    std::string midiExportPath{};
    std::string documentResultPath{};
    std::string documentLoadPath{};
    std::string patchExportPath{};
    std::string patchLoadPath{};
    int64_t autosaveDueMs{0};
    bool autosaveDirty{false};
};

void createDefaultProject() {
    auto& project=aiora::ProjectCore::instance();
    project.reset();

    const int bass=project.addTrack(false);
    project.setTrackName(bass,"Bass");
    project.selectTrack(bass);
    project.replaceSelectedPatch(
        aiora::makeFactoryPatch(aiora::FactoryPreset::Subula));

    const int harmony=project.addTrack(false);
    project.setTrackName(harmony,"Harmony");
    project.selectTrack(harmony);
    project.replaceSelectedPatch(
        aiora::makeFactoryPatch(aiora::FactoryPreset::Spectrello));

    const int lead=project.addTrack(false);
    project.setTrackName(lead,"Lead");
    project.selectTrack(lead);
    project.replaceSelectedPatch(
        aiora::makeFactoryPatch(aiora::FactoryPreset::Nebular));

    const int drums=project.addTrack(true);
    project.setTrackName(drums,"Nexdrum");

    project.selectTrack(bass);
}

void createDemoProject() {
    createDefaultProject();
    auto& project=aiora::ProjectCore::instance();
    project.setBpm(112.0f);
    project.setSignature(4,4);

    // Bass
    for(int s:{0,8,16,24})project.addNote(0,38,static_cast<float>(s),4.0f);

    // Harmony
    for(int s:{0,16}){
        project.addNote(1,50,static_cast<float>(s),8.0f);
        project.addNote(1,57,static_cast<float>(s),8.0f);
        project.addNote(1,64,static_cast<float>(s),8.0f);
    }

    // Lead
    const int leadMidi[8]{62,64,69,67,66,64,62,57};
    for(int i=0;i<8;++i)
        project.addNote(2,leadMidi[i],static_cast<float>(i*4),2.0f);

    // Nexdrum: kick / snare / ride-crash examples using actual mapped ranges.
    for(int s:{0,4,8,12,16,20,24,28})
        project.addNote(3,38,static_cast<float>(s),1.0f);
    for(int s:{4,12,20,28})
        project.addNote(3,59,static_cast<float>(s),1.0f);
    for(int s:{2,6,10,14,18,22,26,30})
        project.addNote(3,73,static_cast<float>(s),1.0f);

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

JNIEnv* androidEnv(ANativeActivity* activity,bool& attached) {
    attached=false;
    if(!activity||!activity->vm)return nullptr;
    JNIEnv* env=nullptr;
    const jint status=activity->vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
    if(status==JNI_OK)return env;
    if(status!=JNI_EDETACHED)return nullptr;
    if(activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return nullptr;
    attached=true;
    return env;
}

void detachAndroidEnv(ANativeActivity* activity,bool attached) {
    if(attached&&activity&&activity->vm)activity->vm->DetachCurrentThread();
}

bool clipboardSetText(ANativeActivity* activity,const std::string& text) {
    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity->clazz)return false;

    bool ok=false;
    jclass activityClass=env->GetObjectClass(activity->clazz);
    jmethodID getService=activityClass
        ?env->GetMethodID(activityClass,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;")
        :nullptr;
    jstring service=env->NewStringUTF("clipboard");
    jobject manager=getService&&service
        ?env->CallObjectMethod(activity->clazz,getService,service)
        :nullptr;

    jclass clipDataClass=env->FindClass("android/content/ClipData");
    jmethodID newPlainText=clipDataClass
        ?env->GetStaticMethodID(
            clipDataClass,
            "newPlainText",
            "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;")
        :nullptr;
    jstring label=env->NewStringUTF("AIORA Project");
    jstring value=env->NewStringUTF(text.c_str());
    jobject clip=newPlainText&&label&&value
        ?env->CallStaticObjectMethod(clipDataClass,newPlainText,label,value)
        :nullptr;

    jclass managerClass=manager?env->GetObjectClass(manager):nullptr;
    jmethodID setPrimary=managerClass
        ?env->GetMethodID(managerClass,"setPrimaryClip","(Landroid/content/ClipData;)V")
        :nullptr;
    if(manager&&clip&&setPrimary){
        env->CallVoidMethod(manager,setPrimary,clip);
        ok=!env->ExceptionCheck();
    }
    if(env->ExceptionCheck())env->ExceptionClear();

    if(managerClass)env->DeleteLocalRef(managerClass);
    if(clip)env->DeleteLocalRef(clip);
    if(value)env->DeleteLocalRef(value);
    if(label)env->DeleteLocalRef(label);
    if(clipDataClass)env->DeleteLocalRef(clipDataClass);
    if(manager)env->DeleteLocalRef(manager);
    if(service)env->DeleteLocalRef(service);
    if(activityClass)env->DeleteLocalRef(activityClass);
    detachAndroidEnv(activity,attached);
    return ok;
}

std::string clipboardGetText(ANativeActivity* activity) {
    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity->clazz)return {};

    std::string out;
    jclass activityClass=env->GetObjectClass(activity->clazz);
    jmethodID getService=activityClass
        ?env->GetMethodID(activityClass,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;")
        :nullptr;
    jstring service=env->NewStringUTF("clipboard");
    jobject manager=getService&&service
        ?env->CallObjectMethod(activity->clazz,getService,service)
        :nullptr;
    jclass managerClass=manager?env->GetObjectClass(manager):nullptr;
    jmethodID hasPrimary=managerClass
        ?env->GetMethodID(managerClass,"hasPrimaryClip","()Z")
        :nullptr;
    jmethodID getPrimary=managerClass
        ?env->GetMethodID(managerClass,"getPrimaryClip","()Landroid/content/ClipData;")
        :nullptr;

    const bool has=manager&&hasPrimary&&env->CallBooleanMethod(manager,hasPrimary)==JNI_TRUE;
    jobject clip=has&&getPrimary?env->CallObjectMethod(manager,getPrimary):nullptr;
    jclass clipClass=clip?env->GetObjectClass(clip):nullptr;
    jmethodID itemCount=clipClass?env->GetMethodID(clipClass,"getItemCount","()I"):nullptr;
    jmethodID getItem=clipClass
        ?env->GetMethodID(clipClass,"getItemAt","(I)Landroid/content/ClipData$Item;")
        :nullptr;

    if(clip&&itemCount&&getItem&&env->CallIntMethod(clip,itemCount)>0){
        jobject item=env->CallObjectMethod(clip,getItem,0);
        jclass itemClass=item?env->GetObjectClass(item):nullptr;
        jmethodID coerce=itemClass
            ?env->GetMethodID(
                itemClass,
                "coerceToText",
                "(Landroid/content/Context;)Ljava/lang/CharSequence;")
            :nullptr;
        jobject chars=coerce?env->CallObjectMethod(item,coerce,activity->clazz):nullptr;
        jclass charsClass=chars?env->GetObjectClass(chars):nullptr;
        jmethodID toString=charsClass
            ?env->GetMethodID(charsClass,"toString","()Ljava/lang/String;")
            :nullptr;
        jstring stringValue=toString
            ?static_cast<jstring>(env->CallObjectMethod(chars,toString))
            :nullptr;

        if(stringValue&&!env->ExceptionCheck()){
            const char* utf=env->GetStringUTFChars(stringValue,nullptr);
            if(utf){out=utf;env->ReleaseStringUTFChars(stringValue,utf);}
        }
        if(env->ExceptionCheck())env->ExceptionClear();

        if(stringValue)env->DeleteLocalRef(stringValue);
        if(charsClass)env->DeleteLocalRef(charsClass);
        if(chars)env->DeleteLocalRef(chars);
        if(itemClass)env->DeleteLocalRef(itemClass);
        if(item)env->DeleteLocalRef(item);
    }else if(env->ExceptionCheck()){
        env->ExceptionClear();
    }

    if(clipClass)env->DeleteLocalRef(clipClass);
    if(clip)env->DeleteLocalRef(clip);
    if(managerClass)env->DeleteLocalRef(managerClass);
    if(manager)env->DeleteLocalRef(manager);
    if(service)env->DeleteLocalRef(service);
    if(activityClass)env->DeleteLocalRef(activityClass);
    detachAndroidEnv(activity,attached);
    return out;
}

bool launchCreateDocument(
    ANativeActivity* activity,int requestCode,
    const char* suggestedName,const char* mimeType,
    const std::string& sourcePath){

    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return false;

    bool ok=false;
    jclass cls=env->GetObjectClass(activity->clazz);
    jmethodID method=cls
        ?env->GetMethodID(
            cls,"createDocument",
            "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")
        :nullptr;
    jstring name=env->NewStringUTF(suggestedName?suggestedName:"Aiora");
    jstring mime=env->NewStringUTF(mimeType?mimeType:"application/octet-stream");
    jstring path=env->NewStringUTF(sourcePath.c_str());
    if(method&&name&&mime&&path){
        env->CallVoidMethod(activity->clazz,method,requestCode,name,mime,path);
        ok=!env->ExceptionCheck();
    }
    if(env->ExceptionCheck())env->ExceptionClear();
    if(path)env->DeleteLocalRef(path);
    if(mime)env->DeleteLocalRef(mime);
    if(name)env->DeleteLocalRef(name);
    if(cls)env->DeleteLocalRef(cls);
    detachAndroidEnv(activity,attached);
    return ok;
}

bool launchOpenDocument(
    ANativeActivity* activity,int requestCode,const char* mimeType){

    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return false;

    bool ok=false;
    jclass cls=env->GetObjectClass(activity->clazz);
    jmethodID method=cls
        ?env->GetMethodID(
            cls,"openDocument","(ILjava/lang/String;)V")
        :nullptr;
    jstring mime=env->NewStringUTF(mimeType?mimeType:"application/json");
    if(method&&mime){
        env->CallVoidMethod(activity->clazz,method,requestCode,mime);
        ok=!env->ExceptionCheck();
    }
    if(env->ExceptionCheck())env->ExceptionClear();
    if(mime)env->DeleteLocalRef(mime);
    if(cls)env->DeleteLocalRef(cls);
    detachAndroidEnv(activity,attached);
    return ok;
}

bool launchNameEditor(
    ANativeActivity* activity,int requestCode,int targetIndex,
    const char* title,const std::string& currentName){

    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return false;

    bool ok=false;
    jclass cls=env->GetObjectClass(activity->clazz);
    jmethodID method=cls
        ?env->GetMethodID(
            cls,"showNameEditor",
            "(IILjava/lang/String;Ljava/lang/String;)V")
        :nullptr;
    jstring jTitle=env->NewStringUTF(title?title:"Rename");
    jstring jCurrent=env->NewStringUTF(currentName.c_str());
    if(method&&jTitle&&jCurrent){
        env->CallVoidMethod(
            activity->clazz,method,requestCode,targetIndex,jTitle,jCurrent);
        ok=!env->ExceptionCheck();
    }
    if(env->ExceptionCheck())env->ExceptionClear();
    if(jCurrent)env->DeleteLocalRef(jCurrent);
    if(jTitle)env->DeleteLocalRef(jTitle);
    if(cls)env->DeleteLocalRef(cls);
    detachAndroidEnv(activity,attached);
    return ok;
}

bool launchNumberEditor(
    ANativeActivity* activity,int requestCode,int targetIndex,
    const char* title,const std::string& currentValue){

    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return false;

    bool ok=false;
    jclass cls=env->GetObjectClass(activity->clazz);
    jmethodID method=cls
        ?env->GetMethodID(
            cls,"showNumberEditor",
            "(IILjava/lang/String;Ljava/lang/String;)V")
        :nullptr;
    jstring jTitle=env->NewStringUTF(title?title:"Value");
    jstring jCurrent=env->NewStringUTF(currentValue.c_str());
    if(method&&jTitle&&jCurrent){
        env->CallVoidMethod(
            activity->clazz,method,requestCode,targetIndex,jTitle,jCurrent);
        ok=!env->ExceptionCheck();
    }
    if(env->ExceptionCheck())env->ExceptionClear();
    if(jCurrent)env->DeleteLocalRef(jCurrent);
    if(jTitle)env->DeleteLocalRef(jTitle);
    if(cls)env->DeleteLocalRef(cls);
    detachAndroidEnv(activity,attached);
    return ok;
}

void scheduleAutosave(NativeState& state,int64_t delayMs);
void previewEditorPatch(NativeState& state);

void serviceDocumentResult(NativeState& state){
    if(state.documentResultPath.empty())return;

    std::ifstream in(state.documentResultPath,std::ios::binary);
    if(!in)return;

    std::string requestLine;
    std::string successLine;
    std::string thirdLine;
    std::string fourthLine;
    std::getline(in,requestLine);
    std::getline(in,successLine);
    std::getline(in,thirdLine);
    std::getline(in,fourthLine);
    in.close();
    std::remove(state.documentResultPath.c_str());

    int requestCode=0;
    try{requestCode=std::stoi(requestLine);}catch(...){return;}
    const bool success=successLine=="1";

    if(requestCode==kRequestRenameTrack||requestCode==kRequestRenamePad){
        if(!success)return;

        int target=0;
        try{target=std::stoi(thirdLine);}catch(...){return;}
        auto& project=aiora::ProjectCore::instance();

        if(requestCode==kRequestRenameTrack){
            if(target>=0&&target<project.trackCount()){
                project.setTrackName(target,fourthLine);
                scheduleAutosave(state,0);
            }
        }else{
            const int track=(target>>16)&0xffff;
            const int pad=target&0xffff;
            if(track>=0&&track<project.trackCount()&&
               project.trackIsDrums(track)&&
               pad>=0&&pad<project.padCount(track)){
                project.setPadName(track,pad,fourthLine);
                scheduleAutosave(state,0);
            }
        }
        return;
    }

    if(requestCode==kRequestOperatorRatio){
        if(!success)return;

        int op=0;
        float ratio=1.0f;
        try{
            op=std::stoi(thirdLine);
            ratio=std::stof(fourthLine);
        }catch(...){
            return;
        }
        if(!std::isfinite(ratio)||op<0||op>=6)return;

        auto& project=aiora::ProjectCore::instance();
        if(project.setSelectedOperatorParam(
            op,aiora::OperatorParam::Ratio,ratio)){
            aiora::AudioEngine::instance().syncProject();
            previewEditorPatch(state);
            scheduleAutosave(state,0);
        }
        return;
    }

    const std::string message=thirdLine;

    if(requestCode==kRequestLoadJson){
        if(!success){
            if(message!="Cancelled")
                __android_log_print(
                    ANDROID_LOG_WARN,kTag,"song load picker failed: %s",
                    message.c_str());
            return;
        }

        aiora::Project loaded;
        std::string error;
        if(!state.documentLoadPath.empty()&&
           aiora::loadProjectFile(state.documentLoadPath,loaded,&error)){
            auto& audio=aiora::AudioEngine::instance();
            audio.stopTransport();
            aiora::ProjectCore::instance().replaceProject(std::move(loaded),0);
            audio.syncProject();
            state.ui.resetDrumRangeArm();
            scheduleAutosave(state,0);
            __android_log_print(
                ANDROID_LOG_INFO,kTag,
                "loaded AIORA song from Android document picker");
        }else{
            __android_log_print(
                ANDROID_LOG_WARN,kTag,
                "selected song could not be loaded: %s",
                error.empty()?"load handoff file missing":error.c_str());
        }
        if(!state.documentLoadPath.empty())
            std::remove(state.documentLoadPath.c_str());
        return;
    }

    if(requestCode==kRequestImportPatch){
        if(!success){
            if(message!="Cancelled")
                __android_log_print(
                    ANDROID_LOG_WARN,kTag,"patch import picker failed: %s",
                    message.c_str());
            return;
        }

        aiora::Patch imported;
        std::string error;
        auto& project=aiora::ProjectCore::instance();
        if(!state.patchLoadPath.empty()&&
           aiora::loadPatchFile(state.patchLoadPath,imported,&error)&&
           project.replaceSelectedPatch(std::move(imported))){
            aiora::AudioEngine::instance().syncProject();
            previewEditorPatch(state);
            scheduleAutosave(state,0);
            __android_log_print(
                ANDROID_LOG_INFO,kTag,
                "loaded AIORA patch from Android document picker");
        }else{
            __android_log_print(
                ANDROID_LOG_WARN,kTag,
                "selected patch could not be loaded: %s",
                error.empty()?"patch handoff file missing or no selected patch target":error.c_str());
        }
        if(!state.patchLoadPath.empty())
            std::remove(state.patchLoadPath.c_str());
        return;
    }

    const char* kind=
        requestCode==kRequestSaveJson?"song JSON":
        requestCode==kRequestExportWav?"WAV":
        requestCode==kRequestExportMidi?"MIDI":
        requestCode==kRequestExportPatch?"patch JSON":"document";
    if(success){
        __android_log_print(ANDROID_LOG_INFO,kTag,"saved AIORA %s",kind);
    }else if(message!="Cancelled"){
        __android_log_print(
            ANDROID_LOG_WARN,kTag,"AIORA %s save failed: %s",
            kind,message.c_str());
    }
}

struct SystemInsets {
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

SystemInsets querySystemInsets(ANativeActivity* activity) {
    SystemInsets out{};
    bool attached=false;
    JNIEnv* env=androidEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return out;

    jclass activityClass=env->GetObjectClass(activity->clazz);
    jmethodID getWindow=activityClass
        ?env->GetMethodID(activityClass,"getWindow","()Landroid/view/Window;")
        :nullptr;
    jobject window=getWindow?env->CallObjectMethod(activity->clazz,getWindow):nullptr;
    jclass windowClass=window?env->GetObjectClass(window):nullptr;
    jmethodID getDecor=windowClass
        ?env->GetMethodID(windowClass,"getDecorView","()Landroid/view/View;")
        :nullptr;
    jobject decor=getDecor?env->CallObjectMethod(window,getDecor):nullptr;
    jclass decorClass=decor?env->GetObjectClass(decor):nullptr;
    jmethodID getRootInsets=decorClass
        ?env->GetMethodID(decorClass,"getRootWindowInsets","()Landroid/view/WindowInsets;")
        :nullptr;
    jobject wi=getRootInsets?env->CallObjectMethod(decor,getRootInsets):nullptr;
    jclass wiClass=wi?env->GetObjectClass(wi):nullptr;

    int sdk=26;
    jclass versionClass=env->FindClass("android/os/Build$VERSION");
    if(versionClass){
        jfieldID sdkField=env->GetStaticFieldID(versionClass,"SDK_INT","I");
        if(sdkField)sdk=env->GetStaticIntField(versionClass,sdkField);
    }

    if(wi&&wiClass&&sdk>=30){
        jclass typeClass=env->FindClass("android/view/WindowInsets$Type");
        jmethodID systemBars=typeClass
            ?env->GetStaticMethodID(typeClass,"systemBars","()I")
            :nullptr;
        const jint mask=systemBars?env->CallStaticIntMethod(typeClass,systemBars):0;
        jmethodID getInsets=env->GetMethodID(
            wiClass,"getInsets","(I)Landroid/graphics/Insets;");
        jobject ins=(getInsets&&mask!=0)?env->CallObjectMethod(wi,getInsets,mask):nullptr;
        jclass insClass=ins?env->GetObjectClass(ins):nullptr;
        if(insClass){
            jfieldID left=env->GetFieldID(insClass,"left","I");
            jfieldID top=env->GetFieldID(insClass,"top","I");
            jfieldID right=env->GetFieldID(insClass,"right","I");
            jfieldID bottom=env->GetFieldID(insClass,"bottom","I");
            if(left)out.left=env->GetIntField(ins,left);
            if(top)out.top=env->GetIntField(ins,top);
            if(right)out.right=env->GetIntField(ins,right);
            if(bottom)out.bottom=env->GetIntField(ins,bottom);
        }
        if(insClass)env->DeleteLocalRef(insClass);
        if(ins)env->DeleteLocalRef(ins);
        if(typeClass)env->DeleteLocalRef(typeClass);
    }else if(wi&&wiClass){
        const auto readLegacy=[&](const char* name){
            jmethodID method=env->GetMethodID(wiClass,name,"()I");
            return method?static_cast<int>(env->CallIntMethod(wi,method)):0;
        };
        out.left=readLegacy("getSystemWindowInsetLeft");
        out.top=readLegacy("getSystemWindowInsetTop");
        out.right=readLegacy("getSystemWindowInsetRight");
        out.bottom=readLegacy("getSystemWindowInsetBottom");
    }

    if(env->ExceptionCheck()){
        env->ExceptionClear();
        out={};
    }

    if(versionClass)env->DeleteLocalRef(versionClass);
    if(wiClass)env->DeleteLocalRef(wiClass);
    if(wi)env->DeleteLocalRef(wi);
    if(decorClass)env->DeleteLocalRef(decorClass);
    if(decor)env->DeleteLocalRef(decor);
    if(windowClass)env->DeleteLocalRef(windowClass);
    if(window)env->DeleteLocalRef(window);
    if(activityClass)env->DeleteLocalRef(activityClass);
    detachAndroidEnv(activity,attached);
    return out;
}

void updateSafeInsets(NativeState& state) {
    SystemInsets insets=querySystemInsets(state.app?state.app->activity:nullptr);

    if(state.app){
        const auto& rect=state.app->contentRect;
        if(rect.right>rect.left&&rect.bottom>rect.top){
            insets.left=std::max(insets.left,rect.left);
            insets.top=std::max(insets.top,rect.top);
            insets.right=std::max(insets.right,std::max(0,state.width-rect.right));
            insets.bottom=std::max(insets.bottom,std::max(0,state.height-rect.bottom));
        }
    }

    insets.left=std::clamp(insets.left,0,std::max(0,state.width/2));
    insets.right=std::clamp(insets.right,0,std::max(0,state.width/2));
    insets.top=std::clamp(insets.top,0,std::max(0,state.height/2));
    insets.bottom=std::clamp(insets.bottom,0,std::max(0,state.height/2));

    state.ui.setSafeInsets(insets.left,insets.top,insets.right,insets.bottom);
    __android_log_print(
        ANDROID_LOG_INFO,kTag,
        "safe insets l=%d t=%d r=%d b=%d",
        insets.left,insets.top,insets.right,insets.bottom);
}

bool createSurface(NativeState& state) {
    const EGLint msaaAttrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 1,
        EGL_SAMPLES, 4,
        EGL_NONE
    };
    const EGLint basicAttrs[] = {
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
    bool usingMsaa =
        eglChooseConfig(state.display, msaaAttrs, &config, 1, &count) == EGL_TRUE &&
        count >= 1;
    if(!usingMsaa){
        count=0;
        if (eglChooseConfig(state.display, basicAttrs, &config, 1, &count) != EGL_TRUE ||
            count < 1) {
            return false;
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,kTag,
        usingMsaa?"OpenGL UI: 4x MSAA":"OpenGL UI: MSAA unavailable, using filtered glyphs");

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
    updateSafeInsets(state);
    aiora::NativeOverlay::instance().init(state.app ? state.app->activity : nullptr);
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

int findCurvePointIndex(
    int track,int note,int kind,float step,float value,bool free){
    auto& project=aiora::ProjectCore::instance();
    const int count=project.curvePointCount(track,note,kind);
    int best=-1;float bestScore=1.0e9f;
    for(int i=0;i<count;++i){
        const float ds=std::fabs(project.curvePointStep(track,note,kind,i)-step);
        const float dv=std::fabs(project.curvePointValue(track,note,kind,i)-value);
        const float df=project.curvePointFree(track,note,kind,i)==free?0.0f:10.0f;
        const float score=ds*4.0f+dv+df;
        if(score<bestScore){bestScore=score;best=i;}
    }
    return best;
}

void resetRollGroupAutomation(NativeState& state){
    state.rollGroupAutomation={};
    state.ui.clearRollGroupAutomation();
}

bool rollSelectedBounds(
    NativeState& state,float& start,float& end){
    if(!state.ui.rollNoteSelectionActive())return false;
    const int track=state.ui.rollSelectedTrack();
    const auto snapshot=aiora::ProjectCore::instance().projectCopy();
    if(track<0||track>=static_cast<int>(snapshot.tracks.size()))return false;
    const auto& notes=snapshot.tracks[static_cast<size_t>(track)].notes;
    bool any=false;
    start=1.0e9f;
    end=-1.0e9f;
    for(int index:state.ui.rollSelectedNotes()){
        if(index<0||index>=static_cast<int>(notes.size()))continue;
        const auto& note=notes[static_cast<size_t>(index)];
        start=std::min(start,note.startStep);
        end=std::max(
            end,
            note.startStep+
                std::max(0.0f,std::max(1.0f,note.lengthSteps)-1.0f));
        any=true;
    }
    if(!any)return false;
    if(end<start)end=start;
    return true;
}

GroupAutomationCurve* groupAutomationForKind(
    NativeState& state,int kind){
    if(kind!=1&&kind!=2)return nullptr;
    return &state.rollGroupAutomation[static_cast<size_t>(kind-1)];
}

void syncRollGroupAutomationUi(NativeState& state){
    const auto mode=state.ui.rollMode();
    const int kind=static_cast<int>(mode)-1;
    if(state.ui.rollSelectionTool()!=aiora::RollSelectionTool::Pencil||
       !state.ui.rollNoteSelectionActive()||
       (kind!=1&&kind!=2)){
        state.ui.clearRollGroupAutomation();
        return;
    }
    auto* curve=groupAutomationForKind(state,kind);
    if(!curve||!curve->initialized){
        state.ui.clearRollGroupAutomation();
        return;
    }
    state.ui.setRollGroupAutomation(
        kind,curve->start,curve->end,curve->points);
}

bool ensureRollGroupAutomation(NativeState& state,int kind){
    auto* curve=groupAutomationForKind(state,kind);
    if(!curve)return false;

    float start=0.0f,end=0.0f;
    if(!rollSelectedBounds(state,start,end))return false;
    const float duration=std::max(0.0f,end-start);
    if(!curve->initialized){
        curve->initialized=true;
        curve->start=start;
        curve->end=end;
        const float fallback=kind==1?1.0f:0.0f;
        curve->points.clear();
        curve->points.push_back({0.0f,fallback});
        if(duration>1.0e-5f)
            curve->points.push_back({duration,fallback});
    }else{
        const float oldDuration=std::max(0.0f,curve->end-curve->start);
        curve->start=start;
        curve->end=end;
        if(std::fabs(oldDuration-duration)>1.0e-4f){
            const float fallback=kind==1?1.0f:0.0f;
            curve->points.clear();
            curve->points.push_back({0.0f,fallback});
            if(duration>1.0e-5f)
                curve->points.push_back({duration,fallback});
        }
    }
    syncRollGroupAutomationUi(state);
    return true;
}

bool applyRollGroupAutomation(NativeState& state,int kind){
    auto* curve=groupAutomationForKind(state,kind);
    if(!curve||!curve->initialized||
       !state.ui.rollNoteSelectionActive())return false;
    std::vector<aiora::CurvePoint> points;
    points.reserve(curve->points.size());
    for(const auto& p:curve->points)
        points.push_back({p.step,p.value,true});
    return aiora::ProjectCore::instance().applyGroupAutomation(
        state.ui.rollSelectedTrack(),
        state.ui.rollSelectedNotes(),
        kind,curve->start,points);
}

void refreshRollGroupAutomationBounds(NativeState& state){
    float start=0.0f,end=0.0f;
    if(!rollSelectedBounds(state,start,end)){
        resetRollGroupAutomation(state);
        return;
    }
    for(auto& curve:state.rollGroupAutomation){
        if(!curve.initialized)continue;
        curve.start=start;
        curve.end=end;
    }
    syncRollGroupAutomationUi(state);
}

void setRollNoteSelection(
    NativeState& state,int track,std::vector<int> indices,
    bool preserveGroupAutomation=false){
    state.ui.setRollNoteSelection(track,std::move(indices));
    if(!preserveGroupAutomation)resetRollGroupAutomation(state);
    else refreshRollGroupAutomationBounds(state);
    if(state.ui.rollNoteSelectionActive()&&
       state.ui.rollMode()==aiora::RollMode::Bend){
        state.ui.setRollMode(aiora::RollMode::Notes);
    }
}

void clearRollNoteSelection(NativeState& state){
    state.ui.clearRollNoteSelection();
    resetRollGroupAutomation(state);
}

int findGroupPointIndex(
    const GroupAutomationCurve& curve,float step,float value){
    int best=-1;
    float bestScore=1.0e9f;
    for(int i=0;i<static_cast<int>(curve.points.size());++i){
        const auto& p=curve.points[static_cast<size_t>(i)];
        const float score=std::fabs(p.step-step)*4.0f+
            std::fabs(p.value-value);
        if(score<bestScore){bestScore=score;best=i;}
    }
    return best;
}

void serviceRollLongPress(NativeState& state){
    auto& g=state.rollGesture;
    if(g.pointerId<0||g.moved||g.longPressTriggered)return;
    if(nowMs()-g.downTimeMs<kLongPressMs)return;

    if(g.kind==RollGestureKind::SelectTool){
        if(state.ui.rollSelectionTool()==aiora::RollSelectionTool::Lasso){
            state.ui.toggleRollMultiLasso();
            g.longPressTriggered=true;
        }
        return;
    }

    if(g.kind==RollGestureKind::TimeScroll){
        if(const auto step=state.ui.hitRollBeatStep(g.downX,g.downY)){
            if(state.ui.rollSelectionActive()){
                state.ui.setRollSelection(false,0,0);
            }else{
                state.ui.setRollSelection(true,*step,*step);
            }
            resetRollGroupAutomation(state);
            state.rollLastBeatTapMs=0;
            state.rollLastBeatTapStep=-1;
            g.longPressTriggered=true;
        }
        return;
    }

    if(g.kind==RollGestureKind::NoteEdit&&
       g.track>=0&&g.noteIndex>=0&&
       g.noteEdit!=RollNoteEdit::Create){
        const bool hadGroup=
            g.noteEdit==RollNoteEdit::GroupMove&&!g.groupIndices.empty();
        std::vector<int> source=
            hadGroup?g.groupIndices:std::vector<int>{g.noteIndex};
        auto added=aiora::ProjectCore::instance().duplicateNotes(
            g.track,source,1.0f);
        if(!added.empty()){
            setRollNoteSelection(
                state,g.track,added,hadGroup);
            g.noteEdit=RollNoteEdit::GroupMove;
            g.groupIndices=std::move(added);
            g.noteIndex=g.groupIndices.front();
            g.groupAppliedMidiDelta=0;
            g.groupAppliedStepDelta=0;
            g.longPressTriggered=true;
            aiora::AudioEngine::instance().syncProject();
            scheduleAutosave(state,0);
        }
        return;
    }

    if(g.kind!=RollGestureKind::AutomationEdit||
       g.curveIsNew||g.track<0||g.noteIndex<0||g.curvePoint<0)return;

    auto& project=aiora::ProjectCore::instance();
    const float step=project.curvePointStep(
        g.track,g.noteIndex,g.curveKind,g.curvePoint);
    const float value=project.curvePointValue(
        g.track,g.noteIndex,g.curveKind,g.curvePoint);
    const bool free=!project.curvePointFree(
        g.track,g.noteIndex,g.curveKind,g.curvePoint);

    if(project.updateCurvePoint(
        g.track,g.noteIndex,g.curveKind,g.curvePoint,
        free?step:std::round(step),
        free?value:(g.curveKind==0?std::round(value):value),
        free)){
        g.curveFree=free;
        g.curvePoint=findCurvePointIndex(
            g.track,g.noteIndex,g.curveKind,
            free?step:std::round(step),
            free?value:(g.curveKind==0?std::round(value):value),
            free);
        g.longPressTriggered=true;
        aiora::AudioEngine::instance().syncProject();
        scheduleAutosave(state);
    }
}

void refreshSurfaceGeometry(NativeState& state) {
    if(!state.drawable||
       state.display==EGL_NO_DISPLAY||
       state.surface==EGL_NO_SURFACE)return;

    EGLint eglWidth=0;
    EGLint eglHeight=0;
    eglQuerySurface(state.display,state.surface,EGL_WIDTH,&eglWidth);
    eglQuerySurface(state.display,state.surface,EGL_HEIGHT,&eglHeight);

    int windowWidth=0;
    int windowHeight=0;
    if(state.app&&state.app->window){
        windowWidth=ANativeWindow_getWidth(state.app->window);
        windowHeight=ANativeWindow_getHeight(state.app->window);
    }

    const int width=
        windowWidth>0?windowWidth:static_cast<int>(eglWidth);
    const int height=
        windowHeight>0?windowHeight:static_cast<int>(eglHeight);

    if(width<=0||height<=0)return;
    if(width==state.width&&height==state.height)return;

    __android_log_print(
        ANDROID_LOG_INFO,kTag,
        "surface resized %dx%d -> %dx%d (egl %dx%d)",
        state.width,state.height,width,height,
        static_cast<int>(eglWidth),static_cast<int>(eglHeight));

    state.width=width;
    state.height=height;
    state.ui.resize(state.width,state.height);
    updateSafeInsets(state);
}

void drawFrame(NativeState& state) {
    serviceDocumentResult(state);
    if (!state.drawable) return;
    refreshSurfaceGeometry(state);
    serviceEditorPreview(state);
    serviceAutosave(state);
    serviceRollLongPress(state);
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

int mappedPadForMidi(int track,int midi){
    auto& project=aiora::ProjectCore::instance();
    if(track<0||!project.trackIsDrums(track))return -1;
    const int count=project.padCount(track);
    for(int p=0;p<count;++p){
        const int lo=std::min(project.padLow(track,p),project.padHigh(track,p));
        const int hi=std::max(project.padLow(track,p),project.padHigh(track,p));
        if(midi>=lo&&midi<=hi)return p;
    }
    return -1;
}

void previewRollPitch(NativeState& state,int midi){
    auto& project=aiora::ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return;

    auto& audio=aiora::AudioEngine::instance();
    if(state.editorPreviewVoice>=0){
        audio.noteOff(state.editorPreviewVoice);
        state.editorPreviewVoice=-1;
        state.editorPreviewStopMs=0;
    }

    if(project.trackIsDrums(track)){
        const int pad=mappedPadForMidi(track,midi);
        if(pad<0)return;
        state.editorPreviewVoice=audio.noteOnPad(pad,midi,0.88f);
    }else{
        state.editorPreviewVoice=audio.noteOn(midi,0.85f);
    }

    if(state.editorPreviewVoice>=0)
        state.editorPreviewStopMs=nowMs()+420;
}

void startPitch(NativeState& state, PointerVoice& touch, int midi) {
    if (touch.midi == midi && touch.voiceId >= 0) return;
    if (touch.voiceId >= 0 || touch.midi >= 0) stopTouch(state, touch);

    touch.midi = midi;
    state.ui.setPitchActive(midi, true);

    auto& project=aiora::ProjectCore::instance();
    auto& audio=aiora::AudioEngine::instance();
    const int track=project.selectedTrack();

    if(state.ui.page()==aiora::NativePage::Drums&&
       track>=0&&project.trackIsDrums(track)){
        int pad=project.selectedPad(track);
        if(pad<0||pad>=project.padCount(track))pad=mappedPadForMidi(track,midi);
        if(pad>=0)touch.voiceId=audio.noteOnPad(pad,midi,0.85f);
        return;
    }

    if(state.ui.page()==aiora::NativePage::Play&&
       track>=0&&project.trackIsDrums(track)){
        int pad=mappedPadForMidi(track,midi);
        if(pad>=0){
            touch.voiceId=audio.noteOnPad(pad,midi,0.85f);
            return;
        }
        // Preserve the old Nexdrum fallback for unmapped pitches.
        touch.voiceId=audio.noteOnPad(
            aiora::NativeUi::padIndexForMidi(midi),midi,0.85f);
        return;
    }

    touch.voiceId=audio.noteOn(midi,0.85f);
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

    if(state.ui.dropdownOpen()){
        const auto choice=state.ui.hitDropdown(x,y);
        if(!choice){
            state.ui.closeDropdown();
            return true;
        }

        bool changed=false;
        switch(choice->kind){
            case aiora::DropdownKind::Track:
                if(choice->option>=0&&choice->option<project.trackCount()){
                    project.selectTrack(choice->option);
                    state.ui.resetDrumRangeArm();
                    if(state.ui.page()==aiora::NativePage::Synth||
                       state.ui.page()==aiora::NativePage::Fx)
                        previewEditorPatch(state);
                }
                return true;

            case aiora::DropdownKind::Pad:{
                const int track=choice->context;
                if(track>=0&&track<project.trackCount()&&
                   project.trackIsDrums(track)&&
                   choice->option>=0&&choice->option<project.padCount(track)){
                    project.selectTrack(track);
                    project.selectPad(track,choice->option);
                    state.ui.resetDrumRangeArm();
                    if(state.ui.page()==aiora::NativePage::Synth||
                       state.ui.page()==aiora::NativePage::Fx)
                        previewEditorPatch(state);
                }
                return true;
            }

            case aiora::DropdownKind::Patch:{
                const int track=choice->context;
                if(track>=0&&track<project.trackCount()&&
                   choice->option>=0&&
                   choice->option<static_cast<int>(aiora::FactoryPreset::Count)){
                    project.selectTrack(track);
                    if(choice->option==static_cast<int>(aiora::FactoryPreset::Nexdrum)){
                        changed=project.loadNexdrumKit(track);
                        state.ui.resetDrumRangeArm();
                    }else{
                        changed=project.replaceSelectedPatch(
                            aiora::makeFactoryPatch(
                                static_cast<aiora::FactoryPreset>(choice->option)));
                    }
                }
                break;
            }

            case aiora::DropdownKind::Wave:
            case aiora::DropdownKind::FilterType:
            case aiora::DropdownKind::LfoTarget:
            case aiora::DropdownKind::ModTarget:
                changed=aiora::NativeEditor::instance().applyDropdownChoice(*choice);
                break;

            case aiora::DropdownKind::None:
                return true;
        }

        if(changed){
            audio.syncProject();
            scheduleAutosave(state,0);
            if(state.ui.page()==aiora::NativePage::Synth||
               state.ui.page()==aiora::NativePage::Fx)
                previewEditorPatch(state);
        }
        return true;
    }

    if(state.ui.openUiDropdownAt(x,y))return true;

    if(state.ui.page()==aiora::NativePage::Synth||
       state.ui.page()==aiora::NativePage::Fx){
        const auto page=state.ui.page()==aiora::NativePage::Synth
            ?aiora::EditorPage::Synth:aiora::EditorPage::Fx;
        if(aiora::NativeEditor::instance().openDropdownAt(page,x,y,state.ui))
            return true;
    }

    if(state.ui.page()==aiora::NativePage::Synth){
        auto& editor=aiora::NativeEditor::instance();
        if(const auto op=editor.hitOperatorRatio(x,y)){
            const auto patch=project.selectedPatch();
            const float ratio=patch.ops[static_cast<size_t>(*op)].ratio;
            char value[32]{};
            std::snprintf(value,sizeof(value),"%.6g",static_cast<double>(ratio));
            if(!launchNumberEditor(
                state.app?state.app->activity:nullptr,
                kRequestOperatorRatio,*op,
                ("Operator "+std::to_string(*op+1)+" ratio").c_str(),
                value)){
                __android_log_print(
                    ANDROID_LOG_WARN,kTag,
                    "operator ratio editor could not start");
            }
            return true;
        }
    }

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
            case aiora::HeaderAction::TransportStart:
                state.ui.setRollStartStep(0);
                state.ui.setPerformanceFollow(true);
                break;
            case aiora::HeaderAction::TransportToggle:
                if(audio.transportPlaying()){
                    audio.stopTransport();
                }else{
                    state.ui.setPerformanceFollow(true);
                    audio.playTransport(state.ui.rollStartStep());
                }
                break;
            case aiora::HeaderAction::TransportEnd:
                state.ui.setRollStartStep(
                    std::max(0,project.playLengthSteps()));
                state.ui.setPerformanceFollow(true);
                break;
            case aiora::HeaderAction::PerformanceView:
                releaseAllTouches(state);
                state.ui.togglePerformancePage();
                return true;
        }
        if(*action!=aiora::HeaderAction::TransportStart&&
           *action!=aiora::HeaderAction::TransportToggle&&
           *action!=aiora::HeaderAction::TransportEnd){
            scheduleAutosave(state);
        }
        return true;
    }

    if (const auto nav = state.ui.hitNav(x, y)) {
        releaseAllTouches(state);
        if(*nav!=aiora::NativePage::Roll)
            state.ui.setRollSelection(false,0,0);
        state.ui.setPage(*nav);
        state.ui.resetDrumRangeArm();
        if (*nav == aiora::NativePage::Drums) ensureDrumTrackSelected();
        return true;
    }

    if(const auto sw=state.ui.hitTrackSwitch(x,y)){
        const int count=project.trackCount();
        if(count>0){
            int track=std::clamp(project.selectedTrack(),0,count-1);
            if(*sw==aiora::TrackSwitchAction::PreviousTrack){
                track=(track+count-1)%count;
                project.selectTrack(track);
                state.ui.setRollSelection(false,0,0);
                state.ui.resetDrumRangeArm();
            }else if(*sw==aiora::TrackSwitchAction::NextTrack){
                track=(track+1)%count;
                project.selectTrack(track);
                state.ui.setRollSelection(false,0,0);
                state.ui.resetDrumRangeArm();
            }else if(project.trackIsDrums(track)){
                const int pads=project.padCount(track);
                if(pads>0){
                    int pad=std::clamp(project.selectedPad(track),0,pads-1);
                    if(*sw==aiora::TrackSwitchAction::PreviousPad)pad=(pad+pads-1)%pads;
                    else pad=(pad+1)%pads;
                    project.selectPad(track,pad);
                }
            }
            if(state.ui.page()==aiora::NativePage::Synth||
               state.ui.page()==aiora::NativePage::Fx){
                previewEditorPatch(state);
            }
        }
        return true;
    }

    if (state.ui.page() == aiora::NativePage::Tracks) {
        if(const auto renameTrack=state.ui.hitTrackName(x,y)){
            project.selectTrack(*renameTrack);
            if(!launchNameEditor(
                state.app?state.app->activity:nullptr,
                kRequestRenameTrack,*renameTrack,
                "Track name",project.trackName(*renameTrack))){
                __android_log_print(
                    ANDROID_LOG_WARN,kTag,"track rename dialog could not start");
            }
            return true;
        }

        if(const auto utility=state.ui.hitTrackUtility(x,y)){
            switch(*utility){
                case aiora::TrackUtilityAction::DozenalToggle:
                    project.setDozenal(!project.dozenal());
                    scheduleAutosave(state,0);
                    break;

                case aiora::TrackUtilityAction::ClearTrack:{
                    const int track=project.selectedTrack();
                    if(track>=0){
                        project.clearTrackNotes(track);
                        audio.syncProject();
                        scheduleAutosave(state,0);
                    }
                    break;
                }

                case aiora::TrackUtilityAction::Demo:
                    audio.stopTransport();
                    createDemoProject();
                    audio.syncProject();
                    state.ui.resetDrumRangeArm();
                    scheduleAutosave(state,0);
                    break;

                case aiora::TrackUtilityAction::SaveProject:{
                    std::string error;
                    const bool prepared=!state.jsonExportPath.empty()&&
                        aiora::saveProjectFile(
                            state.jsonExportPath,project.projectCopy(),&error);
                    const bool launched=prepared&&launchCreateDocument(
                        state.app?state.app->activity:nullptr,
                        kRequestSaveJson,
                        "Aiora Song.json","application/json",
                        state.jsonExportPath);
                    if(!launched){
                        __android_log_print(
                            ANDROID_LOG_WARN,kTag,
                            "Save As could not start: %s",
                            error.empty()?"document picker unavailable":error.c_str());
                    }
                    break;
                }

                case aiora::TrackUtilityAction::LoadProject:{
                    if(!launchOpenDocument(
                        state.app?state.app->activity:nullptr,
                        kRequestLoadJson,"application/json")){
                        __android_log_print(
                            ANDROID_LOG_WARN,kTag,
                            "song file picker could not start");
                    }
                    break;
                }

                case aiora::TrackUtilityAction::AiFromClipboard:{
                    const std::string description=
                        clipboardGetText(state.app?state.app->activity:nullptr);
                    if(!description.empty()){
                        aiora::Patch generated=aiora::heuristicPatch(description);
                        const int track=project.selectedTrack();
                        if(generated.nexdrumLow>=0&&track>=0){
                            project.loadNexdrumKit(track);
                            state.ui.resetDrumRangeArm();
                        }else{
                            project.replaceSelectedPatch(std::move(generated));
                        }
                        audio.syncProject();
                        previewEditorPatch(state);
                        scheduleAutosave(state,0);
                        __android_log_print(
                            ANDROID_LOG_INFO,kTag,
                            "generated AIORA patch from Tracks AI designer");
                    }else{
                        __android_log_print(
                            ANDROID_LOG_WARN,kTag,
                            "Tracks AI designer clipboard is empty");
                    }
                    break;
                }
            }
            return true;
        }

        if(const auto transfer=state.ui.hitProjectTransfer(x,y)){
            if(*transfer==aiora::ProjectTransferAction::ExportWav){
                std::string error;
                int sampleRate=audio.sampleRate();
                if(sampleRate<22050)sampleRate=48000;
                const auto copy=project.projectCopy();
                const bool prepared=!state.wavExportPath.empty()&&
                    aiora::exportProjectWav(
                        state.wavExportPath,copy,sampleRate,&error);
                const bool launched=prepared&&launchCreateDocument(
                    state.app?state.app->activity:nullptr,
                    kRequestExportWav,
                    "Aiora Song.wav","audio/wav",
                    state.wavExportPath);
                if(!launched){
                    __android_log_print(
                        ANDROID_LOG_WARN,kTag,"WAV export failed: %s",
                        error.empty()?"document picker unavailable":error.c_str());
                }
            }else{
                std::string error;
                const auto copy=project.projectCopy();
                const bool prepared=!state.midiExportPath.empty()&&
                    aiora::exportProjectMidi(
                        state.midiExportPath,copy,&error);
                const bool launched=prepared&&launchCreateDocument(
                    state.app?state.app->activity:nullptr,
                    kRequestExportMidi,
                    "Aiora Song.mid","audio/midi",
                    state.midiExportPath);
                if(!launched){
                    __android_log_print(
                        ANDROID_LOG_WARN,kTag,"MIDI export failed: %s",
                        error.empty()?"document picker unavailable":error.c_str());
                }
            }
            return true;
        }
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

    if(state.ui.page()==aiora::NativePage::Synth){
        if(const auto preset=aiora::NativeEditor::instance().hitFactoryPreset(x,y)){
            const int track=project.selectedTrack();
            if(track>=0){
                if(*preset==static_cast<int>(aiora::FactoryPreset::Nexdrum)){
                    project.loadNexdrumKit(track);
                    state.ui.resetDrumRangeArm();
                }else{
                    project.replaceSelectedPatch(
                        aiora::makeFactoryPatch(static_cast<aiora::FactoryPreset>(*preset)));
                }
                audio.syncProject();
                scheduleAutosave(state,0);
                previewEditorPatch(state);
            }
            return true;
        }
    }

    if(state.ui.page()==aiora::NativePage::Synth || state.ui.page()==aiora::NativePage::Fx){
        const auto editorPage=state.ui.page()==aiora::NativePage::Synth
            ?aiora::EditorPage::Synth:aiora::EditorPage::Fx;
        if(const auto transfer=aiora::NativeEditor::instance().hitPatchTransfer(editorPage,x,y)){
            if(*transfer==aiora::PatchTransferAction::AiFromClipboard){
                const std::string description=clipboardGetText(state.app?state.app->activity:nullptr);
                if(!description.empty()){
                    aiora::Patch generated=aiora::heuristicPatch(description);
                    const int track=project.selectedTrack();
                    if(generated.nexdrumLow>=0&&track>=0){
                        project.loadNexdrumKit(track);
                        state.ui.resetDrumRangeArm();
                    }else{
                        project.replaceSelectedPatch(std::move(generated));
                    }
                    audio.syncProject();
                    previewEditorPatch(state);
                    scheduleAutosave(state,0);
                    __android_log_print(ANDROID_LOG_INFO,kTag,"generated offline AIORA patch from clipboard description");
                }else{
                    __android_log_print(ANDROID_LOG_WARN,kTag,"AI patch description clipboard is empty");
                }
            }else if(*transfer==aiora::PatchTransferAction::CopyPatch){
                std::string error;
                const bool prepared=!state.patchExportPath.empty()&&
                    aiora::savePatchFile(
                        state.patchExportPath,project.selectedPatch(),&error);
                const bool launched=prepared&&launchCreateDocument(
                    state.app?state.app->activity:nullptr,
                    kRequestExportPatch,
                    "Aiora Patch.patch.json","application/json",
                    state.patchExportPath);
                if(!launched){
                    __android_log_print(
                        ANDROID_LOG_WARN,kTag,"patch export failed: %s",
                        error.empty()?"document picker unavailable":error.c_str());
                }
            }else{
                if(!launchOpenDocument(
                    state.app?state.app->activity:nullptr,
                    kRequestImportPatch,"application/json")){
                    __android_log_print(
                        ANDROID_LOG_WARN,kTag,
                        "patch file picker could not start");
                }
            }
            return true;
        }
    }

    if(state.ui.page()==aiora::NativePage::Drums){
        if(const auto renamePad=state.ui.hitPadName(x,y)){
            const int track=project.selectedTrack();
            if(track>=0&&project.trackIsDrums(track)){
                project.selectPad(track,*renamePad);
                std::string current=project.padName(track,*renamePad);
                if(current.empty()){
                    const std::string icon=project.padIcon(track,*renamePad);
                    current=icon.empty()?"Pad":icon;
                    if(!current.empty())current[0]=static_cast<char>(std::toupper(
                        static_cast<unsigned char>(current[0])));
                }
                const int packedTarget=(track<<16)|(*renamePad&0xffff);
                if(!launchNameEditor(
                    state.app?state.app->activity:nullptr,
                    kRequestRenamePad,packedTarget,
                    "Pad name",current)){
                    __android_log_print(
                        ANDROID_LOG_WARN,kTag,"pad rename dialog could not start");
                }
            }
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

    if(state.ui.hitRollRangeScopeToggle(x,y)){
        state.ui.setRollRangeAllTracks(!state.ui.rollRangeAllTracks());
        return true;
    }

    if(state.ui.hitRollDelete(x,y)){
        int removed=0;
        if(state.ui.rollNoteSelectionActive()){
            const int selectionTrack=state.ui.rollSelectedTrack();
            removed=project.deleteNotes(
                selectionTrack,state.ui.rollSelectedNotes());
            clearRollNoteSelection(state);
        }else if(state.ui.rollSelectionActive()&&
                 state.ui.rollSelectionAnchorStep()!=
                    state.ui.rollSelectionEndStep()){
            const float lo=static_cast<float>(std::min(
                state.ui.rollSelectionAnchorStep(),
                state.ui.rollSelectionEndStep()));
            const float hi=static_cast<float>(std::max(
                state.ui.rollSelectionAnchorStep(),
                state.ui.rollSelectionEndStep()));
            if(state.ui.rollRangeAllTracks()){
                for(int t=0;t<project.trackCount();++t)
                    removed+=project.deleteNotesInRange(t,lo,hi);
            }else{
                const int track=project.selectedTrack();
                if(track>=0)removed=project.deleteNotesInRange(track,lo,hi);
            }
            state.ui.setRollSelection(false,0,0);
            resetRollGroupAutomation(state);
        }
        if(removed>0){
            audio.syncProject();
            scheduleAutosave(state,0);
        }
        return true;
    }

    if(const auto action=state.ui.hitRollCornerAction(x,y)){
        if(*action==aiora::RollCornerAction::Copy){
            const int track=project.selectedTrack();
            const int lo=std::min(
                state.ui.rollSelectionAnchorStep(),
                state.ui.rollSelectionEndStep());
            const int hi=std::max(
                state.ui.rollSelectionAnchorStep(),
                state.ui.rollSelectionEndStep());

            state.rollClipboard.clear();
            state.rollClipboardAllTracks=state.ui.rollRangeAllTracks();
            const auto snapshot=project.projectCopy();
            if(hi>lo){
                if(state.rollClipboardAllTracks){
                    for(int t=0;t<static_cast<int>(snapshot.tracks.size());++t){
                        RollClipboardTrack entry;
                        entry.track=t;
                        for(const auto& note:
                            snapshot.tracks[static_cast<size_t>(t)].notes){
                            if(note.startStep>=static_cast<float>(lo)&&
                               note.startStep<static_cast<float>(hi)){
                                auto copy=note;
                                copy.startStep-=static_cast<float>(lo);
                                entry.notes.push_back(std::move(copy));
                            }
                        }
                        if(!entry.notes.empty())
                            state.rollClipboard.push_back(std::move(entry));
                    }
                }else if(
                    track>=0&&track<static_cast<int>(snapshot.tracks.size())){
                    RollClipboardTrack entry;
                    entry.track=-1;
                    for(const auto& note:
                        snapshot.tracks[static_cast<size_t>(track)].notes){
                        if(note.startStep>=static_cast<float>(lo)&&
                           note.startStep<static_cast<float>(hi)){
                            auto copy=note;
                            copy.startStep-=static_cast<float>(lo);
                            entry.notes.push_back(std::move(copy));
                        }
                    }
                    if(!entry.notes.empty())
                        state.rollClipboard.push_back(std::move(entry));
                }
            }

            state.ui.setRollSelection(false,0,0);
            resetRollGroupAutomation(state);
            state.ui.setRollClipboardAvailable(!state.rollClipboard.empty());
            return true;
        }

        if(*action==aiora::RollCornerAction::Paste){
            int added=0;
            const int selectedTrack=project.selectedTrack();
            for(const auto& entry:state.rollClipboard){
                const int target=state.rollClipboardAllTracks
                    ?entry.track:selectedTrack;
                if(target<0||target>=project.trackCount())continue;
                added+=project.pasteNotes(
                    target,entry.notes,
                    static_cast<float>(state.ui.rollStartStep()));
            }
            if(added>0){
                audio.syncProject();
                scheduleAutosave(state,0);
            }
            state.rollClipboard.clear();
            state.rollClipboardAllTracks=false;
            state.ui.setRollClipboardAvailable(false);
            return true;
        }
    }

    if (const auto mode = state.ui.hitRollMode(x, y)) {
        const auto next=state.ui.rollMode()==*mode
            ?aiora::RollMode::Notes:*mode;
        state.ui.setRollMode(next);
        if(state.ui.rollNoteSelectionActive()&&
           state.ui.rollSelectionTool()==aiora::RollSelectionTool::Pencil&&
           (next==aiora::RollMode::Velocity||next==aiora::RollMode::Mod)){
            ensureRollGroupAutomation(
                state,static_cast<int>(next)-1);
        }else{
            state.ui.clearRollGroupAutomation();
        }
        return true;
    }

    return false;
}

void handleRollBeatTap(NativeState& state,float x,float y,int64_t timeMs){
    const auto step=state.ui.hitRollBeatStep(x,y);
    if(!step)return;

    const bool doubleTap=
        state.rollLastBeatTapMs>0&&
        timeMs-state.rollLastBeatTapMs<=kRollDoubleTapMs&&
        std::abs(*step-state.rollLastBeatTapStep)<=1;

    if(doubleTap){
        if(state.ui.rollSelectionActive()){
            state.ui.setRollSelection(
                true,state.ui.rollSelectionAnchorStep(),*step);
        }else{
            state.ui.setRollStartStep(*step);
        }
        state.rollLastBeatTapMs=0;
        state.rollLastBeatTapStep=-1;
    }else{
        state.rollLastBeatTapMs=timeMs;
        state.rollLastBeatTapStep=*step;
    }
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
    int64_t timeMs,
    RollGestureKind kind) {

    state.rollGesture.clear();
    state.rollGesture.pointerId = pointerId;
    state.rollGesture.kind = kind;
    state.rollGesture.downX = x;
    state.rollGesture.downY = y;
    state.rollGesture.lastX = x;
    state.rollGesture.lastY = y;
    state.rollGesture.downTimeMs = timeMs;
}

bool beginRollNoteGesture(
    NativeState& state,
    int32_t pointerId,
    float x,
    float y,
    int64_t timeMs) {

    auto& project=aiora::ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return false;

    const auto hit=state.ui.hitRollCell(x,y);
    if(!hit)return false;
    const auto noteHit=state.ui.hitRollNote(x,y);

    if(state.ui.rollNoteSelectionActive()){
        if(state.ui.rollSelectedTrack()!=track){
            clearRollNoteSelection(state);
            return false;
        }
        if(!noteHit||!state.ui.rollNoteSelected(noteHit->noteIndex)){
            // In Pencil mode, touching anything outside the active group only
            // clears the selection; it does not leak through into note editing.
            clearRollNoteSelection(state);
            return false;
        }

        beginRollGesture(state,pointerId,x,y,timeMs,RollGestureKind::NoteEdit);
        auto& g=state.rollGesture;
        g.track=track;
        g.noteIndex=noteHit->noteIndex;
        g.noteEdit=RollNoteEdit::GroupMove;
        g.groupIndices=state.ui.rollSelectedNotes();
        g.dragCellMidi=hit->midi;
        g.dragCellStep=hit->step;
        g.groupAppliedMidiDelta=0;
        g.groupAppliedStepDelta=0;
        return true;
    }

    beginRollGesture(state,pointerId,x,y,timeMs,RollGestureKind::NoteEdit);
    auto& g=state.rollGesture;
    g.track=track;

    if(!noteHit){
        g.noteIndex=project.addNote(
            track,hit->midi,static_cast<float>(hit->step),1.0f);
        if(g.noteIndex<0){g.clear();return false;}
        g.noteEdit=RollNoteEdit::Create;
        g.noteMidi=hit->midi;
        g.noteStart=static_cast<float>(hit->step);
        g.noteLength=1.0f;
        previewRollPitch(state,hit->midi);
        return true;
    }

    const int note=noteHit->noteIndex;
    g.noteIndex=note;
    g.noteMidi=project.noteMidi(track,note);
    g.noteStart=project.noteStart(track,note);
    g.noteLength=project.noteLength(track,note);
    g.noteEdit=noteHit->tail?RollNoteEdit::Resize:RollNoteEdit::Move;
    g.dragCellMidi=hit->midi;
    g.dragCellStep=hit->step;
    return true;
}

bool beginRollAutomationGesture(
    NativeState& state,
    int32_t pointerId,
    float x,float y,
    int64_t timeMs){

    const auto mode=state.ui.rollMode();
    if(mode==aiora::RollMode::Notes)return false;
    const int kind=static_cast<int>(mode)-1;
    const auto hit=state.ui.hitRollAutomation(x,y,kind);
    if(!hit)return false;

    auto& project=aiora::ProjectCore::instance();
    const int track=project.selectedTrack();
    if(track<0)return false;

    beginRollGesture(
        state,pointerId,x,y,timeMs,RollGestureKind::AutomationEdit);
    auto& g=state.rollGesture;
    g.track=track;
    g.noteIndex=hit->noteIndex;
    g.curveKind=kind;
    g.curveFree=hit->free;

    if(hit->pointIndex>=0){
        g.curvePoint=hit->pointIndex;
        g.curveIsNew=false;
    }else{
        g.curvePoint=project.addCurvePoint(
            track,hit->noteIndex,kind,hit->step,hit->value,false);
        if(g.curvePoint<0){g.clear();return false;}
        g.curveIsNew=true;
        g.curveFree=false;
        aiora::AudioEngine::instance().syncProject();
        scheduleAutosave(state);
    }
    return true;
}

bool beginRollGroupAutomationGesture(
    NativeState& state,int32_t pointerId,
    float x,float y,int64_t timeMs){
    const auto mode=state.ui.rollMode();
    if(mode!=aiora::RollMode::Velocity&&mode!=aiora::RollMode::Mod)
        return false;
    const int kind=static_cast<int>(mode)-1;
    if(!ensureRollGroupAutomation(state,kind)||
       !state.ui.hitRollGroupAutomationGutter(x,y))
        return false;

    auto* curve=groupAutomationForKind(state,kind);
    if(!curve)return false;

    beginRollGesture(
        state,pointerId,x,y,timeMs,RollGestureKind::GroupAutomation);
    auto& g=state.rollGesture;
    g.track=state.ui.rollSelectedTrack();
    g.curveKind=kind;

    if(const auto point=state.ui.hitRollGroupAutomationPoint(x,y)){
        g.curvePoint=*point;
        g.curveIsNew=false;
        return true;
    }

    float step=0.0f,value=0.0f;
    if(!state.ui.rollGroupAutomationPosition(x,y,step,value)){
        g.clear();
        return false;
    }
    if(curve->points.size()>=24){
        g.clear();
        return false;
    }
    curve->points.push_back({step,value});
    std::sort(
        curve->points.begin(),curve->points.end(),
        [](const aiora::RollGroupPoint&a,const aiora::RollGroupPoint&b){
            return a.step<b.step;
        });
    g.curvePoint=findGroupPointIndex(*curve,step,value);
    g.curveIsNew=true;
    syncRollGroupAutomationUi(state);
    applyRollGroupAutomation(state,kind);
    return true;
}

void moveRollGesture(NativeState& state, float x, float y) {
    auto& g = state.rollGesture;
    if (g.pointerId < 0) return;

    const float dx = x - g.lastX;
    const float dy = y - g.lastY;
    g.lastX = x;
    g.lastY = y;

    const float totalDx = x - g.downX;
    const float totalDy = y - g.downY;
    if (!g.moved && std::hypot(totalDx,totalDy)>8.0f) g.moved=true;

    const float threshold = std::max(16.0f, state.ui.rollCellPixels() * 0.72f);

    if(g.kind==RollGestureKind::PitchScroll){
        g.accumX+=dx;
        while(std::fabs(g.accumX)>=threshold){
            const int delta=g.accumX<0.0f?1:-1;
            state.ui.scrollRoll(delta,0);
            g.accumX+=g.accumX<0.0f?threshold:-threshold;
        }
        return;
    }

    if(g.kind==RollGestureKind::TimeScroll){
        g.accumY+=dy;
        while(std::fabs(g.accumY)>=threshold){
            const int delta=g.accumY<0.0f?1:-1;
            state.ui.scrollRoll(0,delta);
            g.accumY+=g.accumY<0.0f?threshold:-threshold;
        }
        return;
    }

    if(g.kind==RollGestureKind::SelectTool){
        return;
    }

    if(g.kind==RollGestureKind::Lasso){
        if(g.moved)state.ui.appendRollLasso(x,y);
        return;
    }

    if(g.kind==RollGestureKind::GroupAutomation){
        if(!g.moved||g.curveKind<1||g.curveKind>2||
           g.curvePoint<0)return;
        auto* curve=groupAutomationForKind(state,g.curveKind);
        if(!curve||g.curvePoint>=static_cast<int>(curve->points.size()))return;

        float step=0.0f,value=0.0f;
        if(!state.ui.rollGroupAutomationPosition(x,y,step,value))return;
        const bool first=g.curvePoint==0;
        const bool last=
            g.curvePoint==static_cast<int>(curve->points.size())-1;
        const float duration=std::max(0.0f,curve->end-curve->start);
        if(first)step=0.0f;
        if(last)step=duration;

        curve->points[static_cast<size_t>(g.curvePoint)]={step,value};
        std::sort(
            curve->points.begin(),curve->points.end(),
            [](const aiora::RollGroupPoint&a,const aiora::RollGroupPoint&b){
                return a.step<b.step;
            });
        g.curvePoint=findGroupPointIndex(*curve,step,value);
        syncRollGroupAutomationUi(state);
        applyRollGroupAutomation(state,g.curveKind);
        return;
    }

    if(g.kind==RollGestureKind::AutomationEdit){
        if(!g.moved||g.track<0||g.noteIndex<0||g.curvePoint<0)return;
        float step=0.0f,value=0.0f;
        if(!state.ui.rollAutomationPosition(
            x,y,g.noteIndex,g.curveKind,g.curveFree,step,value))return;
        auto& project=aiora::ProjectCore::instance();
        if(project.updateCurvePoint(
            g.track,g.noteIndex,g.curveKind,g.curvePoint,
            step,value,g.curveFree)){
            g.curvePoint=findCurvePointIndex(
                g.track,g.noteIndex,g.curveKind,step,value,g.curveFree);
        }
        return;
    }

    if(g.kind!=RollGestureKind::NoteEdit||!g.moved||g.track<0||g.noteIndex<0)return;

    const auto hit=state.ui.hitRollCell(x,y);
    if(!hit)return;

    auto& project=aiora::ProjectCore::instance();
    if(g.noteEdit==RollNoteEdit::GroupMove){
        const int totalMidi=hit->midi-g.dragCellMidi;
        const int totalStep=hit->step-g.dragCellStep;
        const int midiDelta=totalMidi-g.groupAppliedMidiDelta;
        const int stepDelta=totalStep-g.groupAppliedStepDelta;
        if((midiDelta!=0||stepDelta!=0)&&
           project.moveNotes(
               g.track,g.groupIndices,midiDelta,
               static_cast<float>(stepDelta))){
            g.groupAppliedMidiDelta=totalMidi;
            g.groupAppliedStepDelta=totalStep;
            refreshRollGroupAutomationBounds(state);
        }
        return;
    }

    if(g.noteEdit==RollNoteEdit::Create){
        const float length=std::clamp(
            static_cast<float>(hit->step)-g.noteStart+1.0f,
            1.0f,kMaxRollNoteLengthSteps);
        if(project.updateNote(g.track,g.noteIndex,hit->midi,g.noteStart,length)){
            g.noteMidi=hit->midi;
            g.noteLength=length;
        }
    }else if(g.noteEdit==RollNoteEdit::Move){
        if(project.updateNote(
            g.track,g.noteIndex,hit->midi,static_cast<float>(hit->step),g.noteLength)){
            g.noteMidi=hit->midi;
            g.noteStart=static_cast<float>(hit->step);
        }
    }else if(g.noteEdit==RollNoteEdit::Resize){
        const float length=std::clamp(
            static_cast<float>(hit->step)-g.noteStart+1.0f,
            1.0f,kMaxRollNoteLengthSteps);
        if(project.updateNote(g.track,g.noteIndex,g.noteMidi,g.noteStart,length)){
            g.noteLength=length;
        }
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

            // Dropdowns are modal: never let a menu tap leak through to
            // sliders, scrolling, notes, or other controls beneath it.
            if(state.ui.dropdownOpen()){
                handleUiTap(state,x,y);
                return 1;
            }

            if(state.ui.page()==aiora::NativePage::Tracks&&state.ui.trackPointerDown(x,y)){
                state.trackControlPointerId=pointerId;
                return 1;
            }

            if(state.ui.page()==aiora::NativePage::Drums&&state.ui.drumPointerDown(x,y)){
                state.drumControlPointerId=pointerId;
                return 1;
            }

            if(state.ui.hitScrollableBody(x,y)){
                state.pageScrollPointerId=pointerId;
                state.pageScrollDownX=x;
                state.pageScrollDownY=y;
                state.pageScrollLastY=y;
                state.pageScrollMoved=false;
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
                if(state.rollGesture.pointerId>=0)return 1;
                const int64_t timeMs=nowMs();

                if(state.ui.hitRollPitchHeader(x,y)){
                    beginRollGesture(
                        state,pointerId,x,y,timeMs,RollGestureKind::PitchScroll);
                    return 1;
                }

                if(state.ui.hitRollBeatGutter(x,y)){
                    beginRollGesture(
                        state,pointerId,x,y,timeMs,RollGestureKind::TimeScroll);
                    return 1;
                }

                if(state.ui.hitRollNoteArea(x,y)){
                    if(state.ui.rollMode()==aiora::RollMode::Notes){
                        beginRollNoteGesture(state,pointerId,x,y,timeMs);
                    }else{
                        beginRollAutomationGesture(
                            state,pointerId,x,y,timeMs);
                    }
                    return 1;
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

            if(state.pageScrollPointerId>=0){
                const size_t count=AMotionEvent_getPointerCount(event);
                for(size_t i=0;i<count;++i){
                    if(AMotionEvent_getPointerId(event,i)==state.pageScrollPointerId){
                        const float x=AMotionEvent_getX(event,i);
                        const float y=AMotionEvent_getY(event,i);
                        const float dx=x-state.pageScrollDownX;
                        const float dy=y-state.pageScrollDownY;
                        if(dx*dx+dy*dy>100.0f)state.pageScrollMoved=true;
                        if(state.pageScrollMoved){
                            state.ui.scrollPage(state.pageScrollLastY-y);
                        }
                        state.pageScrollLastY=y;
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

            if(state.pageScrollPointerId==pointerId){
                const bool wasMoved=state.pageScrollMoved;
                state.pageScrollPointerId=-1;
                state.pageScrollMoved=false;
                if(!wasMoved)handleUiTap(state,x,y);
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
                auto& g=state.rollGesture;
                if(g.kind==RollGestureKind::PitchScroll){
                    if(!g.moved){
                        if(const auto midi=state.ui.hitRollPitchHeaderMidi(
                            g.downX,g.downY)){
                            previewRollPitch(state,*midi);
                        }
                    }
                }else if(g.kind==RollGestureKind::TimeScroll){
                    if(!g.moved&&!g.longPressTriggered){
                        handleRollBeatTap(state,g.downX,g.downY,nowMs());
                    }else if(g.moved){
                        state.rollLastBeatTapMs=0;
                        state.rollLastBeatTapStep=-1;
                    }
                }else if(g.kind==RollGestureKind::NoteEdit){
                    auto& project=aiora::ProjectCore::instance();
                    if(!g.moved &&
                       (g.noteEdit==RollNoteEdit::Move||g.noteEdit==RollNoteEdit::Resize) &&
                       g.track>=0&&g.noteIndex>=0){
                        project.deleteNote(g.track,g.noteIndex);
                    }
                    aiora::AudioEngine::instance().syncProject();
                    scheduleAutosave(state);
                }else if(g.kind==RollGestureKind::AutomationEdit){
                    auto& project=aiora::ProjectCore::instance();
                    bool changed=g.curveIsNew||g.moved||g.longPressTriggered;
                    if(!g.curveIsNew&&!g.moved&&!g.longPressTriggered&&
                       g.track>=0&&g.noteIndex>=0&&g.curvePoint>=0){
                        changed=project.deleteCurvePoint(
                            g.track,g.noteIndex,g.curveKind,g.curvePoint);
                    }
                    if(changed){
                        aiora::AudioEngine::instance().syncProject();
                        scheduleAutosave(state);
                    }
                }

                g.clear();
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
            state.pageScrollPointerId=-1;
            state.pageScrollMoved=false;
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
        case APP_CMD_CONFIG_CHANGED:
            if(state.drawable){
                refreshSurfaceGeometry(state);
                updateSafeInsets(state);
                drawFrame(state);
            }
            break;

        case APP_CMD_WINDOW_REDRAW_NEEDED:
            if(state.drawable){
                refreshSurfaceGeometry(state);
                drawFrame(state);
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
        const std::string base=app->activity->internalDataPath;
        state.autosavePath=base+"/aiora.json";
        state.jsonExportPath=base+"/aiora_song_export.json";
        state.wavExportPath=base+"/aiora_song_export.wav";
        state.midiExportPath=base+"/aiora_song_export.mid";
        state.documentResultPath=base+"/aiora_document_result.txt";
        state.documentLoadPath=base+"/aiora_open_song.json";
        state.patchExportPath=base+"/aiora_patch_export.patch.json";
        state.patchLoadPath=base+"/aiora_open_patch.patch.json";
    }

    aiora::Project restored;
    std::string restoreError;
    if(!state.autosavePath.empty()&&aiora::loadProjectFile(state.autosavePath,restored,&restoreError)){
        aiora::ProjectCore::instance().replaceProject(std::move(restored),0);
        __android_log_print(ANDROID_LOG_INFO,kTag,"restored AIORA autosave");
    }else{
        createDefaultProject();
    }
    serviceDocumentResult(state);

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
