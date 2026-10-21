# Native drum-kit import/export — Beta 0.7.80

The native **Drums** page offers **Import Kit** and **Export Kit** for the currently selected drum track.

- **Export Kit** opens Android's folder picker and saves `<track-name>.kit.json`, not a song project.
- **Import Kit** asks **Replace current kit** or **Append pads** and opens Android's document picker.
- **Replace** swaps only the selected drum track's pad definitions. Its notes, track mixing, name, other tracks and the song remain unchanged.
- **Append** adds pads only when all their ranges are unoccupied. Overlap or exceeding 49 pads rejects the entire import without changing the project. Duplicate pad IDs are renamed when appending.
- Existing note events are never deleted by kit import; notes outside the imported pad ranges simply have no mapped pad until a matching range is assigned.
- Native filename is `.kit.json`. Individual instrument patch files remain `.patch.json`.

## Format

```json
{
  "type": "aiora-drum-kit",
  "version": 1,
  "name": "Nexdrum",
  "selectedPad": 0,
  "pads": [
    {
      "id": "kick",
      "name": "Kick",
      "m": 38,
      "icon": "kick",
      "vol": 1.0,
      "pan": 0.0,
      "patch": { "...": "same patch object used by AIORA projects" },
      "range": [38, 44]
    }
  ]
}
```

Each pad stores the **complete six-operator synth patch**, including waveforms, spectral banks, morphs, envelopes, filter, expression mappings and patch FX. Pitch ranges must lie in MIDI 38–86, be internally nonoverlapping and contain the pad's center note. 1–49 pads are supported. Import is validated before it changes the project.

Files in this format are independent of songs; they do not contain notes, song tempo or the master effects rack. This format uses the project's existing patch codec rather than a second synth serialization system.
