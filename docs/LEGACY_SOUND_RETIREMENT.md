# Aiora — legacy sound-path retirement register (Beta 0.7.73)

This document is a temporary register, NOT a new audio feature. It distinguishes
parked behavior from legacy behavior still used by the existing factory bank.

## Parked now (stored only; not used by the new waveform morph engine)

- `ModTarget::Morph1..Morph6`: old patch-level oscillator harmonic morph links.
  They are retained in the patch format while presets are rebuilt, but the
  oscillator's waveform morph now responds only to the visible fifth operator
  V/M destination, `OperatorModDestination::WaveMorph`.
- `Patch::glide`: retained in source and patch data; already had no native
  synthesis path. Remove in final cleanup.

## Still active for existing factory presets; MUST be rebuilt before removal

- `timpaniMembrane`: post-attack irreversible M-based gain damping.
  Rebuild with operator envelopes, expressive modulation and ordinary waveform
  choices instead.
- `drumPitchDrop` and `drumPitchDropSeconds`: automatic kick transient
  pitch drop. Rebuild with an operator pitch envelope.
- `cymbalModel`: 12 additional inharmonic resonance oscillators, velocity
  excitation and automatically decaying cymbal tail. Rebuild with explicitly
  configured operators and pitch/volume envelopes.
- `Patch::modSlots` and `velocitySlots`: retired patch-level V/M routing
  used by legacy factory presets. Current operator and evolving-filter V/M
  curves are explicit and visible; old general links need replacement routes.
- Legacy fixed ADSR and FX migration fields as applicable after all factory
  presets use the new controls.

## Still active, but review separately before final cleanup

- Custom waveform normalization based on sum of harmonic amplitudes.
- Fixed squared note-level V gain curve.
- Peak EQ bypass when the evolving filter is enabled.
- Fixed post-master `tanh` saturation in playback and WAV export.

## Migration order

1. Ship and phone-test independent Wave / Wave Morph banks plus explicit V/M
   Wave Morph destination. Do not rebuild factory sounds during this step.
2. Rebuild *all* factory instruments / Nexdrum pads using visible, universal
   operator, filter, and V/M controls (including percussion).
3. Remove legacy flags and abandoned modulation targets/code, then simplify
   persistence. Old-project compatibility is explicitly NOT required at this
   development stage.
