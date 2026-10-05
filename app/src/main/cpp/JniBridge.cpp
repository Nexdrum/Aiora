#include <jni.h>
#include "AudioEngine.h"

using aiora::AudioEngine;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_nexdrum_aiora_NativeBridge_startAudio(JNIEnv*, jobject) {
    return AudioEngine::instance().start() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_nexdrum_aiora_NativeBridge_stopAudio(JNIEnv*, jobject) {
    AudioEngine::instance().stop();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexdrum_aiora_NativeBridge_noteOn(JNIEnv*, jobject, jint midi, jfloat velocity) {
    return AudioEngine::instance().noteOn(midi, velocity);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nexdrum_aiora_NativeBridge_noteOff(JNIEnv*, jobject, jint voiceId) {
    AudioEngine::instance().noteOff(voiceId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nexdrum_aiora_NativeBridge_panic(JNIEnv*, jobject) {
    AudioEngine::instance().panic();
}

extern "C" JNIEXPORT void JNICALL
Java_com_nexdrum_aiora_NativeBridge_setFactoryPreset(JNIEnv*, jobject, jint index) {
    AudioEngine::instance().setFactoryPreset(index);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexdrum_aiora_NativeBridge_factoryPreset(JNIEnv*, jobject) {
    return AudioEngine::instance().factoryPreset();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexdrum_aiora_NativeBridge_sampleRate(JNIEnv*, jobject) {
    return AudioEngine::instance().sampleRate();
}
