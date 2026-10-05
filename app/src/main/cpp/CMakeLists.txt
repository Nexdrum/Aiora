cmake_minimum_required(VERSION 3.22.1)
project(aiora LANGUAGES C CXX)

find_package(oboe REQUIRED CONFIG)

add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

add_library(aiora SHARED
    NativeApp.cpp
    NativeUi.cpp
    AudioEngine.cpp
    FactoryPresets.cpp
    FxProcessor.cpp
    NexdrumKit.cpp
    ProjectCore.cpp
    ProjectPlayback.cpp
    SpectrachordVoice.cpp
)

target_include_directories(aiora PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(aiora PRIVATE cxx_std_20)

target_link_libraries(aiora
    native_app_glue
    oboe::oboe
    android
    log
    EGL
    GLESv3
)
