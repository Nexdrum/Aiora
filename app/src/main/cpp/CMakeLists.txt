cmake_minimum_required(VERSION 3.22.1)
project(aiora LANGUAGES C CXX)

find_package(oboe REQUIRED CONFIG)

add_library(aiora SHARED
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    NativeApp.cpp
    NativeUi.cpp
    NativeEditor.cpp
    NativeOverlay.cpp
    AiHeuristic.cpp
    AudioEngine.cpp
    FactoryPresets.cpp
    FxProcessor.cpp
    NexdrumKit.cpp
    ProjectCore.cpp
    ProjectCodec.cpp
    ProjectStorage.cpp
    ProjectPlayback.cpp
    SpectrachordVoice.cpp
)

target_include_directories(aiora PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANDROID_NDK}/sources/android/native_app_glue
)
target_compile_features(aiora PRIVATE cxx_std_20)

target_link_libraries(aiora
    oboe::oboe
    android
    log
    EGL
    GLESv3
)
