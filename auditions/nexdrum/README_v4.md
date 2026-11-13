# Nexdrum Cymbals v4 — Strike-Position PM Audition

This is an **experimental alternate version**, not a replacement for the working v3 files or the approved factory Nexdrum kit. Aiora's native engine and global evolving filter are unchanged.

## Files
- `Crash_Strike_Position_PM_v4.json` — quicker-to-excite outer-edge crash.
- `Ride_Strike_Position_PM_v4.json` — controlled heavy ride requiring more force to excite wash.
- `Nexdrum_Cymbals_v4_Test_Kit.json` — full seven-pad kit; only Crash and Ride embedded patches differ from v3. Roto Tom, Snare, Tom, Floor Tom, and Kick are identical to their v3 kit objects.

## Operators and physics
| Operator | Physical role | V/M routing | W1/W2 interaction |
| --- | --- | --- | --- |
| OP1 | Plate impact (short) | M gates plate contact; V evolves impact envelope and W1/W2 tone | CF |
| OP2 | Plate resonance (long) | M changes resonance PM index and plate amplitude; V affects volume-envelope morph | PM |
| OP3 | Bell impact (short) | **M below 80 gives exactly zero**; V changes impact character | CF |
| OP4 | Bell resonance (long) | Faint across the plate, stronger approaching bell; M adjusts phase modulation | PM |
| OP5 | Stick contact (transient) | Unchanged from v3 | CF |
| OP6 | Edge wash | M gates outermost 20%; V brings in wash through volume-envelope morph | CF |

Resonance W1 is the existing metallic impulse/friction texture. Resonance W2 is a pitched sine at an intentionally inharmonic frequency ratio (crash plate 1.61803; ride plate 1.41421; crash bell 1.37; ride bell 1.285) acting as the **phase modulator**, not an audio crossfade. M controls phase-modulation index, not pitch bend. Bell impact is fully absent for M < 80 and ramps in just above M 80; the bell resonance remains faint on the plate.

## Wash envelope and limits
- Crash OP6 wash: silent at **V ≤ 50**; emerges above ~50 at the edge.
- Ride OP6 wash: silent at **V ≤ 75**; emerges above ~75 at the edge, reaching strong wash only with very hard edge strikes.
- On both patches OP6 is strictly silent for **M ≥ 20** and decreases continuously before that cutoff.
- Aiora's present operator routing can multiply the **M position gate** by the **V-driven envelope morph**. The increasing *perceptual* velocity threshold toward M 20 is therefore approximate, not a true two-dimensional M/V conditional threshold; we should audition that interaction before considering any engine change.
- At V=50, **neither patch generates OP6 wash anywhere on M**.

## Envelope and filter
- Crash longest operator tail remains no longer than 3 seconds, ride no longer than 5 seconds; the v3 master envelopes remain unchanged.
- OP1/OP3 are now genuine short **impacts**, leaving OP2/OP4 to carry the long resonances.
- Existing global filter, its automation and M/V routes, pitch sweeps, FX, master envelope and OP5 are preserved. The filter has not been moved to an individual operator.

## Import and audition
1. Save or download the new v4 kit JSON and use **Drums → Import Kit → Replace** in an audition project; alternatively import the two individual v4 patches using **Synth → Import Patch**.
2. Hold **V=50** and test M at **0, 25, 50, 79, 80, 90, 100**. Listen for changing plate partials through OP2 PM, strictly gated bell impact and faint continuous bell resonance.
3. For crash edge wash, test M **0, 10, 15, 20, 25** with V **50, 65, 80, 95, 100**.
4. For ride edge wash, test the same positions with V **70, 75, 84, 91, 100**.
5. Repeat strikes at medium and high density to judge CPU load, long resonance and metallic realism. Live M motion may reweight ringing modes; it does not retrigger the impact.
6. Re-import these new patches after future revisions: existing imported kits are not automatically replaced.

This is an **audition**, not a claim that the timbre is final or that the CPU benefit has been confirmed on a phone. No factory assets were edited.
