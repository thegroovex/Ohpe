# Cluster-Engine

*Deutsch · English: [../../en/engines/cluster.md](../../en/engines/cluster.md)*

Cluster spielt pro Taste einen ganzen Stapel von Oszillatoren: bis zu sieben bandbegrenzte
Sägezahn- / Puls-Oszillatoren, gegeneinander verstimmt und im Stereobild verteilt. Der Stapel kann
ein dichtes Unisono oder eine Akkordform sein – ein Finger spielt Oktaven, Quinten, Dreiklänge,
Septakkorde oder Quartenschichtungen. Eine langsame zufällige **Drift** hält den Stapel lebendig,
ein resonanzfähiger Stereo-Tiefpass formt ihn.

| Regler | Parameter | Bereich | Wirkung |
|---|---|---|---|
| 1 | Verstimmung | 0–1 | Verstimmung der Stapelstimmen (fein um 0, bis ± 55 Cent) |
| 2 | Breite | 0–1 | Stereobreite des Stapels |
| 3 | Klang | 0–1 | Tiefpass-Cutoff; Mod-Hüllkurve und LFO *Klangfarbe* bewegen ihn |
| 4 | Akkord | Liste | Unisono, Oktave, Quinte, Dur, Moll, Sus2, Sus4, Dur 7, Moll 7, Dominant 7, Add 9, Quarten |
| – | Stapelstimmen | 1–7 | Oszillatoren pro Ton (die ersten der Akkordform) |
| – | Form | 0–1 | Sägezahn → Rechteck → schmaler Puls |
| – | Seitenpegel | 0–1 | Pegel der Stapelstimmen gegenüber der Mittelstimme |
| – | Resonanz | 0–1 | Filterresonanz |
| – | Key-Tracking | 0–1 | Cutoff folgt der Tastatur |
| – | Drift | 0–1 | langsame zufällige Tonhöhenbewegung der Stapelstimmen |

Parameter-IDs: `p1.cluster.detune`, `.spread`, `.tone`, `.chord`, `.stack`, `.shape`, `.side`,
`.reso`, `.keytrack`, `.drift`.

**Ausgangspunkte:** großer Lead – Unisono, Verstimmung 0,35, Breite 0,7; Akkord-Stab – Moll 7,
Stapelstimmen 4, kurzer Amp-Decay, Klang 0,6 mit etwas Mod-Hüllkurve; orgelartig – Oktave,
Verstimmung 0,05, Form 0,5.
