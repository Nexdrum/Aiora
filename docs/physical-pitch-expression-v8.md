# Aiora physical pitch expression — v8

Branch: \`experiment/physical-pitch-expression-v8\` based on Scan All v7;
production \`native-android\` and previous experiments remain untouched.
Android debug app installs separately as **AIORA Pitch Expression**.

## Confirmed root cause

The piano roll already saves its free-drawn B (bend) as a semitone-offset
curve and ProjectPlayback supplies it to note events. In v7, AudioEngine
forwarded only V and M to MaterialTimeVoice; the physical voice applied
tuning once during note-on and had no live physical pitch update.

## v8 behavior

- The piano-roll **B** curve is sampled at note-on and every 32 samples
  during transport, using the same interpolation as all other note curves.
- **Find Frequency** and **Scan All → Apply** generate an offline fine
  physical-parameter map at **1/8 semitone** increments, using the existing
  bounded physical solver and note range. Only source-to-output connected
  tuners accepted by v6/v7 calibration are eligible.
- During playback the expression uses fractional MIDI position
  \`note.midi + bend_semitones\` to select/interpolate these **physical
  parameter values**, including multiple assignments.
- The active material object's geometry or tension and its modal
  frequency/partial/damping coefficients update **in place**. Modal real/
  imaginary state, arrival delay buffers and energy are **not reset**;
  there is no audio resampler, playback-rate multiplier, generic DSP
  pitch shift, or re-triggered note.
- Each polyphonic voice owns its own current physical geometry. V remains
  the human energy input; M remains independent physical interaction motion.
- Note-range limits are respected; bends beyond the achievable map clamp
  to its nearest solved physical setting. Gaps are not interpolated as
  pretend-valid pitches. When there is no active calibration, B cannot
  secretly pitch-shift a generic output.
- The fine map is computed **off the audio callback**. The live callback
  only looks up small fixed-size arrays and recomputes affected modal
  coefficients at control-rate intervals.

## Notes and limitations

- This still uses the v5 reduced modal acoustic model; it does not solve
  full coupled 3D solid/fluid physics. Extremely nonlinear or
  discontinuous mechanisms may yield abrupt pitch changes, while some
  inharmonic instruments do not have an unambiguous audible fundamental.
- Recomputing modal coefficients while preserving states models a
  continually changing physical resonator approximately; it is not
  a detailed continuous-time variable-boundary solver.
- Calibration happens when **FIND** or **APPLY** is used, not silently when
  drawing a curve. Retune if geometry/material changes.
- No project-persistent physical patch storage has been added; this
  remains a session-level experimental instrument as in v7.
- Only voices with an enabled, valid, physically playable calibrated
  note map accept physical B expression. Other instruments retain
  their original behavior.
- The 1/8 semitone physical map is precomputed by bounded optimization;
  between samples the physical property values are interpolated.
  Actual pitch precision depends on physical model accuracy.

## Coverage

\`tests/physical_pitch_expression_v8_regression.cpp\` checks fractional
12-TET accuracy, audible render differences against an identical unbent
voice, modal-energy preservation across mid-note retuning, per-note
polyphony, multi-assignment tuning, range clamping, stale calibration
rejection, an air chamber tuned while the feeding string is unchanged,
and disabled-calibration behavior. All v3–v7 and native compatibility
tests remain in GitHub Actions.
