# Layer engine and cross-modulation (0.8)

*English · Deutsch: [../../de/engines/layer.md](../../de/engines/layer.md)*

Every part can play a **second engine** with every note: the **layer**. Its sound uses the
parameters that engine already has in the part (`pN.wavetable.*`, `pN.grain.*` …), so a part preset
can combine, say, a plucked string with a grain cloud, a reese with a vocal, a piano with its own
frozen recording. Presets that do this carry the category **Hybrid**.

| Parameter | ID | Range | What it does |
|---|---|---|---|
| Layer Engine | `pN.layer.engine` | Off, FM … Sampler | the second engine (Off, or the part's own engine = no layer) |
| Layer Level | `pN.layer.level` | 0–1 | level of the layer |
| Layer Tune | `pN.layer.tune` | −24 … +24 semitones | e.g. an octave or a fifth above |
| Layer Fine | `pN.layer.fine` | ±50 cents | detune against the main engine |
| Cross-Mod Mode | `pN.layer.xmode` | Ring, AM, Fold | how the layer modulates the main engine |
| Cross-Mod | `pN.layer.xmod` | 0–1 | amount of cross-modulation (0 = plain layering) |
| Layer Width | `pN.layer.width` | 0–1 | stereo width of the layer (0 mono, 0.5 as is, 1 doubled side) |
| Layer Own Envelope | `pN.layer.ownenv` | off / on | the layer uses its own amp envelope … |
| Layer Attack / Decay / Sustain / Release | `pN.layer.a` `.d` `.s` `.r` | | … with these times (e.g. a pluck that blooms into a slow cloud) |

**Cross-modulation:** *Ring* multiplies the main engine by the layer (metallic, inharmonic with
detuned layers), *AM* lets the layer's envelope shape the main engine (a grain layer chops a pad),
*Fold* folds the sum of both (harmonics that move with both engines). With MPE or aftertouch, the
target *Layer Blend* moves the balance between main engine and layer.

The layer plays the same notes, glide, pitch envelope, LFO and expression as the main engine; its
voices count against the same 16-voice limit per engine. With the layer off the part sounds exactly
as before 0.8 (tested). The page **Layer** follows the engine pages in the Synth mode.
