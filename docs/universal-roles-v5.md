# AIORA — Universal Physical Roles Lab v5

This is a **research experiment**, isolated from the production \`native-android\`
branch. It replaces the old named instrument creation controls in the native
editor with a source-first physical construction system.

## Rules of the construction grid

- **Vertical Y** is the time of *first energy arrival* at each component.
  Energy already stored in a medium continues vibrating after the source stops.
- **Horizontal X** denotes interaction / entry location, not an artificial
  tone control; individual energy-transfer links own their entry position.
- **V** scales supplied human-source energy (source force magnitude approximately
  scales as sqrt(V) for this reduced, normalized model). It does not directly
  set final audio volume.
- **M** moves assigned positions. Every node and every link can have its own
  enabled state, minimum and maximum. Minimum > maximum reverses motion.
  Multiple assignments respond to the same M input.
- **Five freely repeatable roles:** source, transfer medium, interaction,
  resonating medium, and output. Connections may branch and merge. Causal
  links connect earlier first-arrival nodes to later ones.
- **Source types** are human hand/arm, fingers, breath, and foot/leg. The
  performer can supply impact-like, deflect/release, or sustained motion.
  A bow, pick, mouthpiece, reed or mallet is **not** a source: construct the
  transfer/interacting object with material and geometry instead.

## Material construction

Each universal non-source/non-output role supports:
- One of the materials in the material library, with density, elastic modulus,
  approximate loss factor and contact softness.
- A physical continuum class: filament, thin surface, solid, or air cavity.
  This is a solver-family selection, **not** an instrument preset.
- Length, width, depth, wall thickness, curvature, taper, opening fraction,
  end fixation, tension, and structural loss.
- Four cross-section profile stations at 0%, 33%, 67%, and 100% along the
  constructed object's axis. Their values feed the modal approximation.

Changing material or the profile now changes calculated modal frequencies,
inharmonic spacing, damping, and/or coupling. A normalized pitch is no
longer hard-coded as the frequency of every universal resonator: the generic
geometry and continuum determine each component's natural modal frequencies.

## Native editor

Choose **EXAMPLE** for an already-connected chain of five generic roles;
choose **CLEAR** to leave one human source and build the rest freely.

Use **+SOURCE**, **+TRANSFER**, **+INTERACT**, **+RESONATE**, **+OUTPUT**
as often as needed within the current real-time node budget. Choose
**LINK**, tap an earlier source then a later receiver; tap a link line to
edit entry location, independent M enabled/min/max, coupling aperture and
contact law. Tap a generic physical node to configure geometry and material.
Click the **CONTINUUM** field to cycle solver families. Drag nodes to
change first-arrival time or grid interaction placement.

The editor is intentionally a scalar **geometry-profile editor**, not yet a
3D spline/mesh design viewport. That upgrade should follow after the
physical solver's geometry interfaces are validated.

## Scope and limitations

**This is not a fully general 3D wave/solid/fluid solver.**

- The first pass uses fixed-capacity, reduced analytic/modal approximations
  with geometry-derived coefficients. An arbitrary shape will **not** yet
  produce a verified finite-element response.
- Nonlinear friction, impact, contact geometry, 3D radiation, and reed/jet
  feedback require further physically based implementations.
- The material table uses rough representative properties; real grain
  orientation, anisotropy, moisture, temperature, contact compliance,
  and measurement uncertainty remain unmodeled.
- Connection delays follow editor Y ordering with a small material wave
  propagation term; geometry-based propagation in full 3D remains future work.
- Stored energy decays naturally after a source releases, up to an activity
  threshold; fixed-capacity buffers impose current instrument-size limits.
- Session-only physical blueprints; persistent project serialization and
  a dedicated geometric profile/mesh workspace are still outstanding.
- Existing named instrument types remain in compatibility code for
  regression coverage but are **not** the new creation controls.

## Physical verification to extend

1. Validate frequency vs. measured length, thickness, density, tension.
2. Validate the four-station section profile against FEM / measured modes.
3. Calibrate contact force, friction and bow/string stick-slip.
4. Model breath pressure/flow, jet edge instability and reed/bore feedback.
5. Implement energy conservation accounting and geometric wave travel.
6. Create a touch-friendly geometry editor with spline profiles and
   multiple material regions, then performance test on Android.
