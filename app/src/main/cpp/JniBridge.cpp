#include <jni.h>
#include <string>
#include "AudioEngine.h"
#include "ProjectCore.h"

using aiora::AudioEngine;
using aiora::ProjectCore;

namespace {
std::string fromJava(JNIEnv* env, jstring text) {
    if(!text) return {};
    const char* p=env->GetStringUTFChars(text,nullptr);
    std::string s=p?p:"";
    if(p)env->ReleaseStringUTFChars(text,p);
    return s;
}
jstring toJava(JNIEnv* env,const std::string& text){return env->NewStringUTF(text.c_str());}
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_startAudio(JNIEnv*, jobject) { return AudioEngine::instance().start()?JNI_TRUE:JNI_FALSE; }
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_stopAudio(JNIEnv*, jobject) { AudioEngine::instance().stop(); }
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_noteOn(JNIEnv*, jobject, jint midi, jfloat velocity) { return AudioEngine::instance().noteOn(midi,velocity); }
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_noteOnPad(JNIEnv*, jobject, jint padIndex, jint midi, jfloat velocity) { return AudioEngine::instance().noteOnPad(padIndex,midi,velocity); }
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_noteOff(JNIEnv*, jobject, jint voiceId) { AudioEngine::instance().noteOff(voiceId); }
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_panic(JNIEnv*, jobject) { AudioEngine::instance().panic(); }
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setFactoryPreset(JNIEnv*, jobject, jint index) { AudioEngine::instance().setFactoryPreset(index); }
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_factoryPreset(JNIEnv*, jobject) { return AudioEngine::instance().factoryPreset(); }
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_sampleRate(JNIEnv*, jobject) { return AudioEngine::instance().sampleRate(); }

extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_projectReset(JNIEnv*, jobject){ProjectCore::instance().reset();}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_addTrack(JNIEnv*, jobject, jboolean drums){return ProjectCore::instance().addTrack(drums==JNI_TRUE);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_deleteTrack(JNIEnv*, jobject, jint i){return ProjectCore::instance().deleteTrack(i)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_selectTrack(JNIEnv*, jobject, jint i){return ProjectCore::instance().selectTrack(i)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_selectedTrack(JNIEnv*, jobject){return ProjectCore::instance().selectedTrack();}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_trackCount(JNIEnv*, jobject){return ProjectCore::instance().trackCount();}
extern "C" JNIEXPORT jstring JNICALL Java_com_nexdrum_aiora_NativeBridge_trackName(JNIEnv* env,jobject,jint i){return toJava(env,ProjectCore::instance().trackName(i));}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setTrackName(JNIEnv* env,jobject,jint i,jstring s){ProjectCore::instance().setTrackName(i,fromJava(env,s));}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_trackIsDrums(JNIEnv*,jobject,jint i){return ProjectCore::instance().trackIsDrums(i)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_trackMute(JNIEnv*,jobject,jint i){return ProjectCore::instance().trackMute(i)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_trackSolo(JNIEnv*,jobject,jint i){return ProjectCore::instance().trackSolo(i)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_trackVolume(JNIEnv*,jobject,jint i){return ProjectCore::instance().trackVolume(i);}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_trackPan(JNIEnv*,jobject,jint i){return ProjectCore::instance().trackPan(i);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setTrackMute(JNIEnv*,jobject,jint i,jboolean v){ProjectCore::instance().setTrackMute(i,v==JNI_TRUE);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setTrackSolo(JNIEnv*,jobject,jint i,jboolean v){ProjectCore::instance().setTrackSolo(i,v==JNI_TRUE);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setTrackVolume(JNIEnv*,jobject,jint i,jfloat v){ProjectCore::instance().setTrackVolume(i,v);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setTrackPan(JNIEnv*,jobject,jint i,jfloat v){ProjectCore::instance().setTrackPan(i,v);}

extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_loadNexdrumKit(JNIEnv*,jobject,jint t){return ProjectCore::instance().loadNexdrumKit(t)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_padCount(JNIEnv*,jobject,jint t){return ProjectCore::instance().padCount(t);}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_selectedPad(JNIEnv*,jobject,jint t){return ProjectCore::instance().selectedPad(t);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_selectPad(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().selectPad(t,p)?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jstring JNICALL Java_com_nexdrum_aiora_NativeBridge_padIcon(JNIEnv* env,jobject,jint t,jint p){return toJava(env,ProjectCore::instance().padIcon(t,p));}
extern "C" JNIEXPORT jstring JNICALL Java_com_nexdrum_aiora_NativeBridge_padPatchName(JNIEnv* env,jobject,jint t,jint p){return toJava(env,ProjectCore::instance().padPatchName(t,p));}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_padCenter(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().padCenter(t,p);}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_padLow(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().padLow(t,p);}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_padHigh(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().padHigh(t,p);}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_padVolume(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().padVolume(t,p);}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_padPan(JNIEnv*,jobject,jint t,jint p){return ProjectCore::instance().padPan(t,p);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setPadVolume(JNIEnv*,jobject,jint t,jint p,jfloat v){ProjectCore::instance().setPadVolume(t,p,v);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setPadPan(JNIEnv*,jobject,jint t,jint p,jfloat v){ProjectCore::instance().setPadPan(t,p,v);}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_setPadRange(JNIEnv*,jobject,jint t,jint p,jint lo,jint hi){return ProjectCore::instance().setPadRange(t,p,lo,hi)?JNI_TRUE:JNI_FALSE;}

extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_bpm(JNIEnv*,jobject){return ProjectCore::instance().bpm();}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_beats(JNIEnv*,jobject){return ProjectCore::instance().beats();}
extern "C" JNIEXPORT jint JNICALL Java_com_nexdrum_aiora_NativeBridge_divisions(JNIEnv*,jobject){return ProjectCore::instance().divisions();}
extern "C" JNIEXPORT jboolean JNICALL Java_com_nexdrum_aiora_NativeBridge_dozenal(JNIEnv*,jobject){return ProjectCore::instance().dozenal()?JNI_TRUE:JNI_FALSE;}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_masterVolume(JNIEnv*,jobject){return ProjectCore::instance().masterVolume();}
extern "C" JNIEXPORT jfloat JNICALL Java_com_nexdrum_aiora_NativeBridge_masterReverb(JNIEnv*,jobject){return ProjectCore::instance().masterReverb();}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setBpm(JNIEnv*,jobject,jfloat v){ProjectCore::instance().setBpm(v);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setSignature(JNIEnv*,jobject,jint b,jint d){ProjectCore::instance().setSignature(b,d);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setDozenal(JNIEnv*,jobject,jboolean e){ProjectCore::instance().setDozenal(e==JNI_TRUE);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setMasterVolume(JNIEnv*,jobject,jfloat v){ProjectCore::instance().setMasterVolume(v);}
extern "C" JNIEXPORT void JNICALL Java_com_nexdrum_aiora_NativeBridge_setMasterReverb(JNIEnv*,jobject,jfloat v){ProjectCore::instance().setMasterReverb(v);}
