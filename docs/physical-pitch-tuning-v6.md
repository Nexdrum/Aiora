# Aiora physical tuning experiment v6

**Branch:** \`experiment/physical-pitch-calibration-v6\` (production \`native-android\` unchanged).
**Android debug app:** AIORA Physical Tuning (separate application ID suffix).

## What the keyboard does

V still changes supplied human energy. M still moves independently assigned
physical positions. MIDI note **does not pitch-shift output audio**.

A keyboard note chooses a precomputed collection of permitted **physical
property settings**, applied to the constructed material/geometry **before**
the voice's modal system starts. The initial physical graph remains untouched.

12-TET target: \`A4 * 2^((MIDI - 69)/12)\`. A4 defaults to 440 Hz; a 442-Hz
concert option is included. The target at middle C is approximately
261.626 Hz. A note is marked unplayable if the selected physical mechanism
cannot reach the requested modeled frequency within its assigned bounds.

## Usage

1. Open **Synth → Instrument**, construct/connect physical objects.
2. Tap the resonating physical medium you want to track; press **TRACK**.
   The track must be on a valid path from a human source to an output.
3. With a physical object selected, press **PARAM** to cycle the actual
   mechanical parameters; press **+PITCH** to add the selected property.
   Up to **four independent pitch assignments** are supported.
4. Use **SLOT** to inspect an assignment, **MIN**/**MAX** to bound its
   permitted physical motion, or **REMOVE** to unassign it.
   Assignment priority is the creation order (earlier assignments first).
5. Press **REF -** or **REF +** to choose the reference MIDI note.
6. Press **FIND**. The engine renders the original constructed instrument,
   estimates the audible spectral fundamental, solves the selected physical
   controls for the reference 12-TET frequency, precomputes all 128 notes,
   renders a retuned reference note and displays **MODEL**, **HEARD**, and
   cents error. The 12 bars show FFT spectral energy across logarithmic bands.
7. If desired, use **ON/OFF** to temporarily bypass/re-enable the completed
   physical note map without altering the original instrument.

Examples:
- A filament's **tension** can track notes while length, cross-section and
  material stay fixed.
- A filament can feed a downstream air cavity. The cavity's **length** can
  track notes while the filament's tension and geometry stay fixed.
- Multiple physical parameters on a tracked medium can cooperate, within
  independent bounds.
- Other node and link properties can be assigned, but a parameter that does
  not change the selected modal fundamental will not pretend to tune it.

## Limitations / next research tasks

- This uses the v5 **reduced analytical/modal solver**, not a verified
  bidirectional 3D solid/fluid/acoustic simulation. Solving a tracked
  resonator's calculated fundamental is **not equivalent to** solving
  the full coupled instrument's eigenmodes.
- The FFT analyzer **measures rendered audio**, but complex inharmonic
  instruments may lack a unique, audible fundamental. An uncertain output
  is reported as **unverified**, not automatically as perfect tuning.
- Tone-hole positions and coupled reed/bore acoustics require explicit,
  physically validated boundary/feedback modeling; current \`Opening\`
  is a crude open-versus-closed boundary approximation.
- In this version multiple assignments use bounded coordinate searches on
  the **selected tracked medium**. Full coupled-spectrum multivariate
  acoustic search and user-defined assignment priorities remain future work.
- A note outside its physically achievable range retains the uncalibrated
  mechanism rather than secretly pitch-shifting audio; UI does not yet
  annotate each key separately.
- Geometry/material/source/link changes make existing maps stale, as detected
  by a content fingerprint. Re-run **FIND** after changing the instrument.
- Calibration and FFT operate on the UI thread after **FIND**, never in the
  audio callback; duration can be noticeable on slow Android devices.
- Session-only tuning until native project persistence is implemented.

## Automated coverage

\`tests/physical_pitch_v6_regression.cpp\` checks exact MIDI 12-TET
formula, physical tuning with bounded ranges, multi-parameter cooperation,
downstream-chamber tuning with unchanged string, stale detection and V=0
silence. \`tests/physical_frequency_analyzer_v6_regression.cpp\` checks
FFT analysis on actually rendered output and reference calibration.
Earlier physical material/flow tests and native audio compatibility tests
remain active.
