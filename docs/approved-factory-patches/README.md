# Approved physical-instrument factory patches

Aiora Beta 0.7.79 replaces the factory **Spectrello** slot (ID 2) with the approved, user-auditioned five-string carbon-fiber electric cello. The previously approved **Nebular** (ID 3) and **Subula** (ID 1) are unchanged from Beta 0.7.78.

The patches use `deserializePatchJson` to import the complete approved source JSON, retaining harmonic amplitudes and phases, both morph banks, V/M/K expression, custom sustain/release envelopes, filter settings and dry FX. Only factory display names are normalized.

- ID 1 — **Subula**: `Subula_Metal_Fingerboard_Fretless_v4.json` (Beta 0.7.78)
- ID 2 — **Spectrello**: `Spectrello_Carbon_Fiber_Five_String_v2_Resonant_Release.json` (Beta 0.7.79)
- ID 3 — **Nebular**: `Nebular_Steel_Physics_v2.json` (Beta 0.7.78)

**Implementation:** `app/src/main/cpp/ApprovedFactoryPatches.h` embeds the approved Nebular/Subula patches; `SpectrelloFactoryV2.h` embeds Spectrello. There are no new runtime data-file dependencies.

## Preservation / rollback

The legacy native Nebular and Subula factory definitions remain in Git history at commit `7c611bf2c023ba95fffafbd20e207e6e5f59155b`. The original native Spectrello definition remains in Git history at commit `ba3e0eabded71b327d8e524b512096c2a43c58a2`.

User-approved source JSON copies are retained in this directory. Existing projects with embedded patch snapshots are not automatically rewritten when factory defaults change.

## Verification

Native regressions compare each decoded factory instrument against the independently decoded approved JSON, checking the entire serialized patch including custom envelope points. Spectrello-specific tests verify its six enabled operators, short friction release, dry output, disabled automatic vibrato, and C2–E4-derived K reference range (MIDI 36–88). These tests do not replace on-device auditioning.
