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
- factory bank: Spectrachord Init, Subula, Spectrello, Nebular, Nexdrum, Violin, Viola, Cello, Contrabass, Flute, Clarinet, Oboe, Bassoon, French Horn, Trumpet, Trombone, Tuba, Timpani
- four solo symphonic bowed-string voices with distinct 16-partial body spectra and responsive bow noise; the M curve controls bow pressure/harshness while V independently controls dynamics
- four solo woodwind presets: Flute (M = focused air jet to audible breath), Clarinet (M = single-reed embouchure pressure), Oboe (M = double-reed compression / nasal brightness), and Bassoon (M = woody resonance to double-reed grain). Each combines instrument-specific harmonic/bore profiles with attack turbulence; V remains independent dynamics. French horn is classified with brass, although it often blends with woodwinds.

- four solo brass presets: French Horn (M = open bell through shading to stopped, nasal tone), Trumpet (M = lip drive / brilliant upper harmonics), Trombone (M = broad overblown bark), and Tuba (M = low, resonant lip growl). Each has custom 16-partial harmonic profiles, bore resonance, transient breath/lip noise, and M-directed spectral changes. V remains independent dynamics; no automatic pitch jumps, slides, vibrato, or note-end falls.

- solo tuned Timpani: deliberately non-harmonic membrane modes and brief felt-mallet transient. M shapes impact hardness during the first 65 ms, then irreversibly damps the membrane ring-out. V stays independent dynamics and Bend can emulate pedal tuning. The timpani sound model survives patch/song export and import.

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
- Bend points snap to pitch centers and half-semitone grid lines in X and to cell centers, interior lines and exact note edges in Y. V/M values remain horizontally continuous in both modes while their time positions snap in Y. Each curve has one point per time position (including imported projects), and overlapping moves or snapped conversions are rejected. Filled V/M circles, hollow free diamonds, and conversion haptic feedback remain.
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
- selecting a factory preset on a melodic track automatically renames the track to the preset name; new melodic tracks still start as blank Spectrachord, and drum-pad preset edits do not rename the kit
- piano roll zoom cycles 13 default / 8 close / 25 overview pitch cells using a changing grid-density icon before the lasso/pencil switch; time grid cells remain square, fill the visible height and retain note automation; beat and bar lines use higher-contrast grays

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
