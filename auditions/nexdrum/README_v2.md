# Nexdrum Cymbal Texture Audition v2: No Grain

Experimental **importable** patches for the current Aiora APK. The approved factory kit and v1 prototypes are unmodified.

- `Crash_Metallic_Bloom_v2.json`: broad, bright continuous wash with quicker, livelier metallic vibration
- `Ride_Heavy_Bow_v2.json`: thick ride with more focused, lower-chaos ring and restrained continuous wash
- `Nexdrum_Cymbals_v2_Test_Kit.json`: both audition patches and five other original factory drums, unchanged

## Import

For individual patches: select Crash or Ride on a Nexdrum drum track, visit Synth, and use Import Patch. For the complete test kit: Drums > Import Kit > Replace **in a test project**. Reload the factory Nexdrum kit to restore originals.

## Physical controls

**V = impact strength** (soft to hard) and **M = strike position** (0 = edge, 1 = bell). V must not change the strike position; M must not be treated as attack strength. Moving toward the bell suppresses continuous wash and strengthens pitched friction. Both variants preserve the original stick transient and existing V/M envelopes.

Textures use custom source 0 for continuous colored noise, source 2 for metallic impulses, and source 3 for the pitched friction bell. No custom source 1 (grain), Grain waveform, or grain wave morph is present.

The heavy ride deliberately differs from the crash with reduced wash level, less impulsive density and chaos, deeper sustained metal tone, and focused stick/bell response. These are preliminary synthesis experiments, not measured physical cymbal models.

Compare at M=0 edge / M=.5 bow / M=1 bell, and different V strike strengths.
Baseline factory reference 061a126349ab973256dfec483810a3750ef98937.
