# AIORA Native

Native Android port of the AIORA browser DAW.

The browser build remains the behavioral reference while the Android version reproduces the same musical model natively.

## Architecture

- **C++20 NativeActivity application**
- **OpenGL ES 3 native UI**
- **Oboe 1.11 low-latency audio**
- allocation-free realtime Spectrachord voices
- lock-free UI → audio event queue
- immutable sequencer snapshots
- per-track and per-drum-pad native FX buses
- native JSON project / patch codec
- internal atomic autosave and restore
- minimal Android/JNI boundary used only for platform services such as clipboard

There is no WebView, JavaScript runtime, Jetpack Compose UI, or Kotlin application layer on the native branch.

## Ported AIORA behavior

### Spectrachord

- six operators
- sine / saw / square / triangle / custom / noise waves
- 16-partial custom waves and muted morph partials
- 6×6 FM matrix
- filter and amp envelopes
- velocity response
- LFO
- unison and glide
- four M-envelope modulation links
- distortion, delay, delay time, feedback and reverb
- factory bank: Spectrachord Init, Subula, Spectrello, Nebular, Nexdrum

### Nexdrum

- ranged pitched drum pads
- per-pad Spectrachord patch
- per-pad volume and pan
- pad-specific Synth / FX preview
- quick pad selection in Drums, Synth and FX
- shared 7×7 Play / drum-assignment pitch grid
- factory seven-family Nexdrum kit

### Sequencer

- vertical-time / horizontal-pitch piano roll
- melodic range MIDI 14–110
- drum tracks show only assigned pad pitches
- Notes / bend / V / M editing
- drum bends constrained to the pad range
- native playhead and transport
- track volume, pan, mute and solo
- master volume and reverb

### Native interface

- compiled AIORA pitch glyphs and logo
- AIORA pitch colors
- dozenal mode
- BPM, signature and transport controls
- realtime triggered output oscilloscope
- native multitouch Play grid
- Tracks, Drums, Roll, Synth, FX and Play views

### Project interchange

AIORA projects and patches use the native JSON codec and can currently move between web and Android through clipboard controls:

- COPY SONG / PASTE SONG
- COPY PATCH / PASTE PATCH

The app also autosaves internally to `aiora.json` and restores it on launch.

### Offline AI sound designer

The browser AI designer's heuristic fallback is ported to C++. **AI FROM CLIP** reads a natural-language sound description from the Android clipboard and designs the patch locally without requiring network access.

The native heuristic includes the same main sound families and modifiers used by the web version, including bowed strings, brass, woodwinds, bass, leads, pads, FM/plucked sounds, percussion recipes, and dark / bright / space transformations.

## Build

GitHub Actions builds the `native-android` branch automatically.

Current development builds target `arm64-v8a` to keep the edit → APK loop fast. Multi-ABI release packaging can be restored later.

Local toolchain:

- JDK 17
- Android SDK 36
- NDK `30.0.16248370`
- CMake
- Gradle 9.6+

Build with:

    gradle :app:assembleDebug

APK:

    app/build/outputs/apk/debug/app-debug.apk

## Branches

- `main` — original / earlier repository history
- `native-android` — active C++ Android port
