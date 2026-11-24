# Experimental Aiora Instrument Designer — acoustic engine proof of concept

**Status:** isolated research prototype. Not connected to Android synthesizer UI, project serialization, or playback. The original Spectrachord audio engine and piano roll are unchanged.

## Mechanism

1. **Materials:** physical density, Young's modulus, mechanical loss, and contact softness (illustrative approximate values, not instrument-calibrated measurements).
2. **Tensioned string:** bidirectional digital waveguide with two ring-buffer rails, nut/bridge reflections, simple dissipative boundary losses, and pitch-by-length adaptation. The peak period is quantized to integer sample delays in this first stage.
3. **Excitation:** triangular initial displacement for pluck; finite-duration contact pulse for hammer, dependent on exciter material and hardness.
4. **Bridge and body:** an approximate coupling coefficient feeds six second-order modes of a hollow box, cylinder, plate, or bell. Body resonant frequency uses an idealized sqrt(stiffness/density)/size relationship. Body-to-string feedback is **intentionally disabled** in this version after testing revealed that an unconstrained feedback shortcut can create self-amplifying resonance. A passive impedance-scattering junction is needed before that interaction is enabled.
5. **Performance:** E1 determines the force of excitation at note onset; E2 changes position and pressure of a soft damping contact while the note continues. E2 is smoothed to reduce discontinuities.
6. **Audio:** the sum of mode radiation and a small direct bridge contribution. Real-time render performs no allocations and has cost independent of the string's waveguide length.

## Try it offline

From repository root:

~~~sh
c++ -O2 -std=c++20 -Iapp/src/main/cpp tests/physical_instrument_prototype.cpp -o /tmp/physical-test
/tmp/physical-test
c++ -O2 -std=c++20 -Iapp/src/main/cpp tools/render_physical_instrument_demo.cpp -o /tmp/physical-demo
/tmp/physical-demo /tmp/aiora-physical-prototype.wav
~~~

The demo renders six three-second sounds (with one-third-second gaps): steel pluck, palm-mute added midway, nylon pluck, hard hammer, felt hammer, and steel string connected to a bronze bell-like resonator.

## Limitations and next stage

- This is **not** a full finite-element simulation. Body mode parameters are physically inspired placeholders, not measured models; coupling into the body is one-way in this early prototype and not yet an energy-conserving impedance solution.
- Integer waveguide periods cause tuning error at high pitches; add fractional-delay interpolation before production use.
- Contact is a localized loss approximation; nonlinear friction, stick-slip bowing, geometric deformation, and contact force from velocity must be studied separately.
- Very large polyphony still needs per-device CPU measurement; fixed-capacity voices make runtime reasonably predictable but do not prove mobile performance.
- Benchmark and calibrate against recordings, with passivity/stability tests for all supported connections before introducing arbitrary user-defined physical graphs.
- The proper next stage is an **opt-in** Instrument Designer UI and project/patch engine selection, without altering Spectrachord presets or existing project files.
