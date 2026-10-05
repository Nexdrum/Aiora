# AIORA Native

Native Android port of AIORA.

- Native Android UI (Jetpack Compose)
- C++ application/audio core
- Oboe low-latency audio output
- JNI bridge kept intentionally thin
- Web AIORA build is the behavioral reference for notation, sequencing, drum ranges, Spectrachord, Nexdrum, and per-note ∿ / V / M expression

## Build

GitHub Actions builds a debug APK on every push to `main`. Local builds require JDK 17, Android SDK 36, NDK r30 (`30.0.16248370`), CMake, and Gradle 9.6+.

```bash
gradle :app:assembleDebug
```

The APK is produced under `app/build/outputs/apk/debug/`.
