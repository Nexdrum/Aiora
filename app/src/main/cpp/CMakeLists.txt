cmake_minimum_required(VERSION 3.22.1)
project(aiora LANGUAGES CXX)

find_package(oboe REQUIRED CONFIG)

add_library(aiora SHARED
    AudioEngine.cpp
    FactoryPresets.cpp
    FxProcessor.cpp
    JniBridge.cpp
    NexdrumKit.cpp
    SpectrachordVoice.cpp
)

target_include_directories(aiora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(aiora PRIVATE cxx_std_20)

target_link_libraries(aiora
    oboe::oboe
    android
    log
)
