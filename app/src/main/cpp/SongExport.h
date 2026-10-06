#pragma once

#include <string>

#include "AioraTypes.h"

namespace aiora {

// Render the current project through the same Spectrachord voices and FX path
// used by transport playback, then write 16-bit stereo PCM WAV.
bool exportProjectWav(
    const std::string& path,
    const Project& project,
    int sampleRate,
    std::string* error = nullptr);

// Write a standard MIDI file containing the project's note data and tempo.
// One SMF track is emitted for each AIORA track.
bool exportProjectMidi(
    const std::string& path,
    const Project& project,
    std::string* error = nullptr);

} // namespace aiora
