# Third-party software

Ohpe is licensed under the AGPL-3.0-or-later. It uses the following third-party software; each
keeps its own licence.

| Library | Use | Licence |
|---|---|---|
| [JUCE](https://juce.com) 8 | plug-in formats, standalone app, UI | AGPLv3 (or commercial) |
| [nlohmann/json](https://github.com/nlohmann/json) | state, presets, MIDI maps | MIT |
| [doctest](https://github.com/doctest/doctest) | unit tests (not shipped) | MIT |
| [Ableton Link](https://github.com/Ableton/link) 4.1 | network tempo sync in the standalone app (build option `OHPE_WITH_LINK`, default on) | GPL-2.0-or-later (compatible with the AGPL-3.0-or-later) |
| Space Grotesk | UI font | SIL Open Font License 1.1 |

Ableton Link is fetched at build time with its bundled standalone Asio (Boost Software License
1.0). "Ableton" and "Link" are trademarks of their owner; Ohpe is not affiliated with it.

## Factory drum kit recordings – `resources/drumkit/*.flac`

* **Source:** *Virtuosity Drums* by Versilian Studios LLC and Karoryfer Samples, performed by
  Austin McMahon, recorded at Virtuosity Musical Instruments, Boston –
  <https://github.com/sfzinstruments/virtuosity_drums>, revision
  `9f04cf9a734527edfbb0a4eee1f674e45bbf71bc`.
* **License:** Creative Commons Zero 1.0 Universal (public-domain dedication), copy in
  `resources/drumkit/LICENSE-CC0.txt`. Attribution is not required; it is given here to keep the
  provenance clear, as the authors ask of derivative works.
* **Changes made for Ohpe:** one velocity layer / round robin per pad; the close, mid and overhead
  microphones mixed per piece (kick and snare: close mic + mid + overheads; toms and cymbals:
  mid + overheads; auxiliary percussion: close mic); resampled to 48 kHz; leading silence trimmed;
  shortened with a fade-out; peak-normalised to −1 dBFS; 16-bit FLAC. Pad 5 (clap) has no
  recording and uses Ohpe's synthetic clap model.

| Pad | File | Taken from (`Samples/…`, `_rr1` where present) |
|---|---|---|
| 1 | 01-kick | kickmic, mid, oh `kick_snoff_vl3` |
| 2 | 02-kick-snares | kickmic, mid, oh `kick_snon_vl4` |
| 3 | 03-snare | snaremic, mid, oh `snare_center_vl20` |
| 4 | 04-rimshot | snaremic, mid, oh `snare_rimshot_vl10` |
| 6 | 06-hat-closed | mid, oh `hh_closed_vl3` |
| 7 | 07-hat-open | mid, oh `hh_open_vl3` |
| 8 | 08-hat-pedal | mid, oh `hh_pedal_vl2` |
| 9 | 09-tom-low | mid, oh `ltom_center_vl12` |
| 10 | 10-tom-high | mid, oh `htom_center_vl12` |
| 11 | 11-tom-floor | mid, oh `ltom_center_vl14` |
| 12 | 12-cross-stick | snaremic, mid, oh `snare_crossstick_vl12` |
| 13 | 13-ride | mid, oh `ride_ride_vl2` |
| 14 | 14-ride-bell | mid, oh `ride_bell_vl2` |
| 15 | 15-crash | mid, oh `crash_crash_vl2` |
| 16 | 16-crash-sizzle | mid, oh `crash_sizzle_vl2` |
| 17 | 17-hat-half | mid, oh `hh_half_vl3` |
| 18 | 18-snare-ghost | snaremic, mid, oh `snare_center_vl6` |
| 19 | 19-cowbell | perc/close `Cowbell1_Normal_v2` |
| 20 | 20-tambourine | perc/close `Tamb1_Shake` |
| 21 | 21-shaker | perc/close `LShaker_Shake1D` |
| 22 | 22-claves | perc/close `Claves1_Hit_v2` |
| 23 | 23-conga | perc/close `Conga_22_HitN_101_127` |
| 24 | 24-bongo | perc/close `BongoH_Hit1_v2` |

The script that produced the files is kept with the change (`tools/drumkit/build_kit.py`).
Since 0.5 the two kick pads (`01-kick`, `02-kick-snares`) are rebuilt from the same sources by
`tools/samples/build_samples.py` with Ohpe's own processing (shorter ring, scooped box frequencies,
lifted beater click and a short synthesised pitched sub layer, see `punch_kick()` there).

## Factory sample bank – `resources/samples/**` (0.4)

The drum kits *Tight*, *Room*, *Lo-Fi*, *Hybrid* and *Boom* (0.5) and the ten Sampler / Grain instruments are
built by `tools/samples/build_samples.py` from two public-domain sources. Only CC0 material is used;
no file under a licence with conditions (attribution-required, non-commercial, share-alike) is
included. Attribution is not required; it is given to keep the provenance clear.

* **Virtuosity Drums** by Versilian Studios LLC and Karoryfer Samples, performed by Austin McMahon –
  <https://github.com/sfzinstruments/virtuosity_drums>, revision
  `9f04cf9a734527edfbb0a4eee1f674e45bbf71bc`, **CC0 1.0** (copy in `resources/drumkit/LICENSE-CC0.txt`).
  Used for the kits Tight (close and mid microphones), Room (room and overhead microphones), Lo-Fi
  (close and mid microphones since 0.5; before, the source's vintage "lofi" microphone set) and the acoustic layers, cymbals, tambourine and
  shaker of Hybrid; auxiliary percussion from the close / far percussion microphones.
* **Versatile Community Sample Library (VCSL)** by Versilian Studios and contributors –
  <https://github.com/sgossner/VCSL>, revision `c1ea7bcc3c7309650ab0da9d15c9cd1fbc4a4c7e`,
  **CC0 1.0**. Used for the hand claps of all four kits and for the instruments:

  | Instrument | VCSL folder | Files used |
  |---|---|---|
  | Kalimba | Idiophones / Plucked Idiophones / Kalimba, Tanzania | 7 notes G2 – G#5, `pluck_Main` |
  | Marimba | Idiophones / Struck Idiophones / Marimba | 7 notes C2 – C6, `med` and `loud` |
  | Vibraphone | Idiophones / Struck Idiophones / Vibraphone / Hard Mallets | 6 notes F2 – E5, `v2` |
  | Glockenspiel | Idiophones / Struck Idiophones / Glockenspiel | 6 notes G4 – C7, `medium` |
  | Harp | Chordophones / Composite Chordophones / Concert Harp | 8 notes D2 – D6, `mf` |
  | Harpsichord | Chordophones / Zithers / Harpsichord, Flemish / Sustains / Low | 7 notes C2 – C5 |
  | Recorder | Aerophones / Edge-blown Aerophones / Baroque Alto Recorder / Sustain | 7 notes F3 – E5 |
  | Singing Glass | Idiophones / Friction Idiophones / Wine Glasses / Sustains / Slow | 4 notes D#4 – D5 |
  | Zither | Chordophones / Zithers / Dan Tranh / Normal | 7 notes C#2 – B4, `mf` |
  | Bowed Metal | Idiophones / Struck Idiophones / Vibraphone / Bowed | 6 notes A2 – E5 |
  | Claps (kits) | Idiophones / Struck Idiophones / Claps | `Clap_rr1`–`rr6`, `SoloClap_vl3`, `vl4` |
  | Grand Piano (0.8) | Chordophones / Zithers / the grand piano set recorded with close microphones, no sustain pedal | files `JHPiano_NoSus_Close_<note>_vl2_rr1` and `_vl4_rr1`, every fourth semitone C1 – C7 (19 notes × 2 velocity layers) |
  | Upright Piano (0.8) | Chordophones / Zithers / the upright piano set, `Sustains` | files `Upright1_Sus_<note>_vl3_rr1`, every C and G (11 notes; the file names count octaves one lower, the build script measures the pitch) |

  The piano recordings are found by these file names in the pinned VCSL revision by
  `tools/samples/build_keys.py` (0.8), trimmed, mixed to mono, level-matched and faded like the other
  instruments (3.6 s in the bass down to 1.6 s at the top; 44.1 kHz, 16-bit FLAC).
* **Tine Piano and Reed Piano (0.8)** are Ohpe's own modelled electric pianos, generated by
  `tools/samples/build_keys.py` (part of Ohpe, AGPL-3.0-or-later; the generated audio is additionally
  dedicated to the public domain under CC0 1.0): tine / reed partials, a velocity-dependent pickup
  curve and a hammer thump, every sixth semitone, two velocity layers.

* **Ohpe's own synthesis** (Hybrid kit: kicks, toms, hi-hats, noise crash, cowbell, claves, conga,
  bongo, the synthetic layers of snare and rimshot; the short room tails of the Room and Hybrid
  claps; 0.5: every pad of the Boom kit and the pitched sub layers of the punchier Studio, Tight,
  Room and Lo-Fi kicks) – generated by the build script, part of Ohpe (AGPL-3.0-or-later); the generated audio is
  additionally dedicated to the public domain under CC0 1.0 so the bank stays uniformly CC0.
* **Changes made for Ohpe:** one velocity layer / round robin per pad (two layers for the marimba);
  microphones mixed per kit; resampled to 44.1 kHz; leading silence trimmed; shortened with a
  fade-out; DC removed; Lo-Fi kit band-limited, saturated, quantised and mono; drum pads
  peak-normalised to −1 dBFS; instruments mixed to mono, level-matched across notes, sustained
  instruments given crossfaded loops; 16-bit FLAC. The exact source files and processing are in
  `tools/samples/build_samples.py`; `resources/samples/bank.json` maps the files to kits, pads, keys
  and velocity ranges.
