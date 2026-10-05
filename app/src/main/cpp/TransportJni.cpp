#include <jni.h>
#include "AudioEngine.h"

using aiora::AudioEngine;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_nexdrum_aiora_NativeBridge_syncProject(JNIEnv*, jobject) {
    return AudioEngine::instance().syncProject() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_nexdrum_aiora_NativeBridge_playTransport(JNIEnv*, jobject) {
    return AudioEngine::instance().playTransport() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_nexdrum_aiora_NativeBridge_stopTransport(JNIEnv*, jobject) {
    AudioEngine::instance().stopTransport();
    AudioEngine::instance().collectRetiredSnapshots();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_nexdrum_aiora_NativeBridge_transportPlaying(JNIEnv*, jobject) {
    return AudioEngine::instance().transportPlaying() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexdrum_aiora_NativeBridge_playheadStep(JNIEnv*, jobject) {
    AudioEngine::instance().collectRetiredSnapshots();
    return AudioEngine::instance().playheadStep();
}
