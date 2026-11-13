# Nexdrum Cymbals v3 — Physical V/M Response Audition

Experimental, grain-free, *fully synthesized* crash and thick ride patches for the native Aiora DAW. Factory presets are **not modified**.

## Files
- `Crash_Metallic_Bloom_v3.json` — expressive, readily excited crash with a nonlinear wash bloom
- `Ride_Heavy_Bow_v3.json` — thicker, resistant ride with a clearer ping, later wash onset, and longer metallic ring
- `Nexdrum_Cymbals_v3_Test_Kit.json` — complete seven-pad experimental kit with **only Crash and Ride replaced**; Roto Tom, Snare, Tom, Floor Tom, and Kick are byte-for-byte-equivalent JSON objects to the v2 test kit's unchanged factory references

## Import
- For individual patches: select the Crash or Ride pad, go to **Synth > Import Patch**.
- For the kit: **Drums > Import Kit > Replace**, preferably in a test project.
- To restore production sounds, reload the approved factory Nexdrum kit. **No changes to ApprovedNexdrumKit.h or other native files.**

## Six-operator design
| OP | Role | Wave interaction control | Volume/excitation |
| --- | --- | --- | --- |
| 1 | Fundamental-matched plate impulse | V -> W1/W2 | M plate weight; V volume-envelope morph |
| 2 | Inharmonic plate/bow impulse | M -> W1/W2 | M plate weight; V volume-envelope morph |
| 3 | Fundamental-matched pitched friction bell | V -> W1/W2 | M bell weight; V volume-envelope morph |
| 4 | Inharmonic secondary bell friction | M -> W1/W2 | M bell weight; V volume-envelope morph |
| 5 | Original-style stick contact, short | M -> W1/W2 | M contact weight; V volume-envelope morph |
| 6 | Colored-noise wash and high-force surge | V -> W1/W2 | M wash weight; strongly nonlinear V envelope morph, with an independent V pitch-envelope morph |

All W1/W2 interactions start as **CF (crossfade)**; this avoids unnecessary metallic sideband artifacts before the basic excitation is auditioned. The engine's PM/FM/RM modes remain available for future trials.

### High-force wash pitch-envelope detail
- OP6 Wave 1 = *unpitched* colored noise texture (source 0).
- OP6 Wave 2 = *pitched* brighter noise texture (also source 0).
- OP6 secondary pitch envelope begins at **+8 semitones for crash**, returning to 0 at ~115 ms; the ride starts at +3 and returns by ~85 ms.
- V gradually exposes Wave 2 at high impact and separately morphs the primary/secondary pitch envelopes; since W1 is unpitched, its broadband bed remains largely unaffected.
- These sweeps are transient *brightness motion*, not a claim that real cymbal modes pitch-bend as a unit.

### Expression model
- **V 0–100** = impact energy, operator-specific nonlinear excitation, timbral change, and envelope evolution.
- **M 0–100** = outer edge to bell, continuous plate/bell balance, alternate wave character, and contact timbre.
- **Filter M** drives the evolving filter's **F Morph** (frequency lane blend) and **Q Morph**. **Filter V** drives its **A Morph** (gain lane blend). Because the two controls drive *different* destinations, the envelopes are retained and there is no averaging conflict.
- The evolving filter's paired lanes produce brief high-frequency emphasis that gradually settles; the ride's boost is more restrained.
- Every impulse generator has internal inharmonic modes; OP1 compensates the impulse resonance multiplier so its primary response aligns with the note's fundamental. OP3 friction is at ratio 1. Other modes are intentionally inharmonic.

### Physical differences
- Crash: rising wash response at mid-high V, more colorful/chaotic high-V impulse modes, shorter metallic tail, stronger high-force brightness fall.
- Thick ride: clear soft ping, relatively less chaotic impulses, wash strongly inhibited below V~75, longest ring in plate and bell, subtler high-force brightness motion.
- OP5 remains brief at all V so the ringing body does not drag the initial stick click.

## Shortened natural tail audition
- Crash: maximum primary/secondary operator volume-envelope tail is **3.0 seconds**; the master also reaches zero at 3.0 seconds (previously up to 3.35 seconds for an operator and 3.7 seconds for the master).
- Heavy ride: maximum primary/secondary operator volume-envelope tail is **5.0 seconds**; the master also reaches zero at 5.0 seconds (previously up to 7.4 seconds for an operator and 7.9 seconds for the master).
- Existing early impact and bloom volume-envelope nodes through **0.6 seconds (crash)** and **1.5 seconds (ride)** remain unchanged. Later volume-envelope node times are compressed while their values are retained; the master applies a brief natural final fade.
- Both pads in the seven-pad test kit use the same revised patches. Re-import the updated individual patches or kit to audition these durations; existing imported projects are not updated automatically.
- B/V/M response, operator pitch envelopes, waveform morphs, evolving filter, oscillator textures and the native engine are all unchanged. The five other test-kit pads remain unchanged.

## Audition plan
Test both patches at **M=0 edge, M=50 bow, M=100 bell**. At each position, tap **V=10, 30, 50, 75, 90, 100**.
1. Compare the very soft *tink/ping* against the original approved factory sounds.
2. Confirm that intermediate V gradually excites modes without becoming a scaled-down full crash.
3. Verify that high V at M=100 strengthens the bell but does **not** trigger an edge wash.
4. On the crash, listen for an explosive but natural onset and a smooth bright-to-dark wash. If it sounds electronic, reduce OP6 secondary pitch depth or late-V Wave 2.
5. On the ride, check stick definition and long metallic ringing between repeated medium-strength hits; see whether the full-force wash comes in too early or too late.
6. Listen for unexpected harshness or clipping. The factory approval decision must depend on hands-on audition.

**Still experimental — sound quality and realism are unverified until auditioned in Aiora.** No grains, sample playback, or engine changes are included.
