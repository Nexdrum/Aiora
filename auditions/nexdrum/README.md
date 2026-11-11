# Nexdrum cymbal texture audition v1

These are experimental, reversible crash and ride patches for the current Aiora native-android build. The approved Nexdrum factory kit is **not modified**.

## Installation

- To try just one sound, select the **Crash** or **Ride** pad on a Nexdrum drum track, switch to **Synth**, and use **Import Patch**. Choose the corresponding Crash/Ride JSON file.
- To try both sounds together, on a disposable/test copy of a project, use **Drums > Import Kit > Replace** and choose `Nexdrum_Cymbal_Texture_Audition_v1.json`. That kit preserves the five other factory pads unchanged.
- Restore the originals by reloading the built-in Nexdrum factory kit or by loading a saved project/patch backup.

## Sound design

**Crash — Texture Bloom v1:** retains the original stick and bell oscillator layers; four remaining operators now blend pitched impulse resonances, micro-grains, colored noise and a short metallic click. The existing M (edge-to-bell) and V response curves, evolving filter and amplitude envelopes are retained.

**Ride — Textured Ping v1:** retains the original focused attack and bell oscillator layers; the tail now blends pitched impulse resonances, restrained micro-grains and quieter colored noise. Existing expression maps and long ring envelopes are retained.

These are audition patches—not measured acoustic cymbal models. Compare at low, medium, and high velocity and with M from 0 to 1 before deciding whether to update the official factory kit.

Baseline: Nexdrum v12.1 approved kit, branch `native-android` at commit `4386b759c44c1fb38570b2807e6151885aff50f0`.
