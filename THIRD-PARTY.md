# Third-party material in Ohpe

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
