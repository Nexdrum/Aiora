# Physical Instrument Scan All — experiment v7

Branch: \`experiment/physical-scan-all-v7\`
APK: **AIORA Scan All**, isolated from production and from the v6 tuning experiment.

## User workflow

Open **Synth → Instrument**. Build a connected instrument with a human
source, physical objects and an acoustic output. In the pitch panel:

1. Choose a 12-TET reference note with **REF -/+**, and A4 440/442.
2. Press **SCAN ALL**. The scan runs on a worker rather than the audio thread
   or native UI thread. Manual pitch assignments remain untouched.
3. Browse the ranked suggestions with **<RESULT** and **RESULT>**. Each shows
   the physical node/property selected, model-playable range, audible
   comparison count and quality score.
4. Press **APPLY** to commit the selected candidate's physically calculated
   128-note map (including every modeled out-of-range note), or **DISCARD**
   to keep the previous manual configuration.
5. **FIND** remains the original single tracked-resonator tuning workflow.

Changes to the graph or the reference note invalidate scan suggestions,
so an older scan cannot be applied to different geometry. Running scans
do not hold the audio callback or user interface.

## What is scanned

The scanner visits every **universal vibrational component on a complete
human-energy-to-output path**, including transfer materials, interacting
media and resonators (not merely the one selected with TRACK). For each
it examines every currently supported tunable node property: length,
width, depth, wall, curvature, taper, opening, fixing, tension, profile
stations 0–3 and coupling.

Every connected link is included in the census with its entry position
and aperture. These are not currently selected as valid tuners because
the reduced v6 modal model does not include a physically justified
link-entry-to-frequency law. Similarly, unrelated or acoustically inert
properties cannot be called tuners without a frequency response.

An analytical feasibility pass computes 12-TET reachability across the
keyboard. The best shortlists from **each object** are then assessed
through actual synthesized audio, at low, reference and high notes, via
FFT spectral frequency, pitch error, fundamental confidence and RMS
level. The rank favors **contiguous playable semitone span multiplied by
audible tuning quality**. Selected top candidates are also tested with
pairs of cooperating controls **on the same physical medium** to expand
the usable range.

Scan results are proposals, not guaranteed instrument-wide eigenmode
solutions. The tracked mode can sometimes be weak or perceptually
ambiguous in a coupled physical system. A frequency estimate is labeled
as verified only when the sampled output really matches; untested
notes must not be described as measured. The scanner does not presently
solve real 3D elastic/fluid contact, tone-hole physics or simultaneous
multi-medium eigenfrequency tuning. A true all-body coupled-audio
optimization awaits a more complete physical model.

## Tests

\`tests/physical_scan_all_v7_regression.cpp\`: two different physical
media in one connected instrument; scan must examine both, inspect all
eligible physical properties and links, evaluate actual audio, return
scored results, preserve manual assignments until Apply and exclude
disconnected objects. All prior v6 tuning and physical engine regressions
continue to run.
