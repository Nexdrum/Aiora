# Approved Nebular and Subula factory patches

Aiora Beta 0.7.78 ships the user-auditioned instruments as the two existing factory presets. Only their **display names** are normalized to `Nebular` and `Subula`; harmonic data and patch behavior are imported unmodified using `deserializePatchJson`.

- Factory ID 1 — **Subula**: `Subula_Metal_Fingerboard_Fretless_v4.json` (phase-locked metal fingerboard contact and independent pick transient).
- Factory ID 3 — **Nebular**: `Nebular_Steel_Physics_v2.json`.
- `app/src/main/cpp/ApprovedFactoryPatches.h` embeds the exact source JSON as C++ string literals so it can be loaded without introducing a runtime file dependency.

## Original factory backups

The previous native factory implementation, including both older Nebular and Subula voices, is preserved in Git history before this replacement:

- Pre-integration branch commit: [7c611bf2c023ba95fffafbd20e207e6e5f59155b](https://github.com/Nexdrum/Aiora/commit/7c611bf2c023ba95fffafbd20e207e6e5f59155b)
- Legacy file: `app/src/main/cpp/FactoryPresets.cpp`, blob `4cd8ec362d5a2265f44e6b8c9ca6c054e77b1401`.
- Both approved JSONs also remain available in this directory for independent restoration and future sound-design revision.

## Verification

Native regressions compare decoded factory patches field-for-field by serializing them to JSON and comparing against independently decoded approved source JSON, and check Spectrello remains unaffected. This does not substitute for listening and touchscreen testing on Android.
