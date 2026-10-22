# Aiora — modern synth baseline (Beta 0.7.83)

This release intentionally has no compatibility promise for earlier song/patch formats.
The four factory instruments (Subula, Spectrello, Nebular, Nexdrum) and all seven
Nexdrum pads are loaded through ProjectCodec from their approved modern patches.

## Current sound and expression controls
- Operator-level timeline envelopes: volume, pitch, volume morph, pitch morph.
- Operator V/M/K response curves and waveform morph destinations.
- FM matrix, per-operator phase variation and harmonic spectra.
- Six-lane evolving filter (frequency, gain, Q and their morph variants), plus
  position/intensity/key response curves and slope profiles.
- Independent master-amplitude and unison timelines; LFO and ordered FX rack.
- Piano-roll velocity V and strike/location expression M.

## Retired completely
- Per-operator and patch-level four-stage ADSR structs, controls and DSP states.
- Patch-level velocity/modulation link slots and retired target vocabularies.
- Fixed legacy filter ADSR/type and cutoff fallback.
- Timpani/cymbal hard-coded synthesis flags and kick pitch-drop shortcut.
- Hidden ADSR and patch-link UI, dropdown handlers and number-entry routes.
- Legacy synthesized factory-orchestra picker entries (originals archived in
  docs/backups/orchestral/Orchestral_Legacy_Reference.zip).

New instruments use editable curves rather than hidden sound-specific logic.
