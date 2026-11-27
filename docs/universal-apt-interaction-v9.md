# Aiora v9 — Universal APT excitation and material interaction (experiment)

Branch: `experiment/physical-apt-interactions-v9` (isolated from `native-android`).

## Builder chain

`APT source -> transfer material -> interaction material/method -> resonating medium -> output`

This experiment replaces the visible human anatomy and motion-gesture source
presets. The other physical nodes retain their material, shape, position,
movement, and coupling controls; this is **not** the later geometry UI polish.

## APT controls

- **Acceleration**: free driving acceleration (0.1–120 m/s²) applied to the
  connected transfer medium mass using **F = m·a**. Medium mass is estimated
  from density × parametric geometric volume (and profile). This is a reduced
  lumped approximation, not an anatomical muscle simulation.
- **Potential**: normalized mechanical energy capacity, **not** pressure.
  For a finite stroke the model limits total work delivered; for a note-locked
  stroke it limits the continuously supplied mechanical power. Actual
  velocity emerges from force, drag, available energy, and contact reaction.
- **Time**: manual energy-delivery duration (0.4–2000 ms).
- **Lock to Note Length**: continually drive until note-off, ignoring the
  manual Time duration. Existing modal and transit states survive release.
  Conversely manual Time is intentionally independent of early note-off.
- **V lane**: scales the available Potential continuously; initial V also
  scales driving acceleration gently (~12%) once at note start. Mid-note
  changes do not reinitialize velocity, contact state, or resonator phase.
- **M lane**: existing assigned material/connection position movement.
- **B lane**: existing offline-calibrated *physical* pitch property changes;
  no waveform pitch-shift or state reset.

There is **no** replacement instrument-specific waveform/envelope generator.
The source transmits bounded movement into the connected physical graph.

## Interaction

Existing **Coupled / Friction / Contact / Flow** laws remain available as the
interaction method. New interaction-node conditions:

- **Relative Motion Angle**: -90° … +90°, with 0° normal to the contact
  surface and +/-90° opposite tangential travel directions.
- **Contact Engagement**: 0–100%, working alongside the interaction
  material's softness, coupling, geometry, and link aperture.

A voice-local contact state supports compliant storage, release, and
dissipation. Friction can accumulate elastic displacement before slipping;
normal contact responds to compression and rebound; flow uses a signed
pressure-to-flow approximation. An existing resonator's quadrature modal
motion affects the next contact response, and the calculated reaction
reduces or reverses the transfer's driving motion. Source Potential settings
remain unchanged: the **resulting acoustic excitation** changes.

## Caveats and validation

This is a bounded real-time **reduced acoustic/mechanical model**, not a
verified, energy-exact, bi-directional finite-element or fluid solver. In
particular, friction and flow are approximations; stick-slip synchronization
must still be evaluated by listening and measuring. Kinetic work values use
normalized reference capacities chosen for the current audio scale. The
forward-delay graph itself is retained; feedback is a bounded force reaction,
not an additional backwards audio link.

The new `tests/physical_apt_interaction_v9_regression.cpp` exercises source
mass, finite/sustained delivery, note-off, V expression, contact response,
and calibration invalidation. Earlier regression and Android debug APK
build steps remain enabled. Experimental debug package suffix:
`.aptv9`, label **AIORA APT Physics**.

**Explicitly deferred:** Sustain Reset piano-roll lane, any additional
source-specific envelope controls, and the separate geometry UI polish.
