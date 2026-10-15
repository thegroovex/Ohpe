# Wavetable-Engine

*Deutsch · English: [../../en/engines/wavetable.md](../../en/engines/wavetable.md)*

Wavetable spielt eine Tabelle aus acht Einzelzyklen (Frames) und gleitet stufenlos zwischen ihnen.
Sechs Tabellen sind eingebaut (beim Start von Ohpe berechnet, keine Sampledateien): **Klassisch**
(Sinus → Dreieck → Sägezahn → Rechteck → schmaler werdender Puls), **Harmonisch** (ein additiver
Obertonaufbau, der von Frame zu Frame wächst), **Formant** (vokalartig a – e – i – o – u),
**Digital** (dünne, kammartige Spektren), **Glas** (glockenartige Obertöne) und **Körnig**
(geräuschhafte Spektren, die heller werden).

Jede Tabelle liegt in bandbegrenzten Kopien vor, eine pro Oktave; die Engine blendet je nach Ton
und Stärke der Phasenverbiegung zwischen ihnen über, so bleiben auch hohe Töne frei von Aliasing.
**Warp** ist eine weiche Phasenverzerrung (*Biegen*: eine Ausbuchtung pro Zyklus, *Drehen*: zwei),
die die Welle heller macht, ohne Spiegelfrequenzen zu erzeugen; deshalb nimmt sie in den obersten
Oktaven ab. Drei Unisono-Oszillatoren, ein Sub-Sinus und ein resonanzfähiger Tiefpass ergänzen
die Stimme.

| Regler | Parameter | Bereich | Wirkung |
|---|---|---|---|
| 1 | Position | 0–1 | Position in der Tabelle (Frame-Überblendung) |
| 2 | Warp | 0–1 | Stärke der Phasenverzerrung |
| 3 | Klang | 0–1 | Tiefpass-Cutoff (≈ 40 Hz – 20 kHz); Mod-Hüllkurve und LFO *Klangfarbe* bewegen ihn |
| 4 | Unisono | 0–1 | zwei verstimmte Seitenoszillatoren, Verstimmung und Stereobreite |
| – | Tabelle | Liste | Klassisch, Harmonisch, Formant, Digital, Glas, Körnig |
| – | Warp-Art | Liste | Biegen, Drehen |
| – | Hüllk. > Position | −1 – +1 | wie weit die Mod-Hüllkurve die Position verschiebt |
| – | Resonanz | 0–1 | Filterresonanz |
| – | Sub | 0–1 | Sinus eine Oktave tiefer |
| – | Key-Tracking | 0–1 | Cutoff folgt der Tastatur |

Parameter-IDs: `p1.wavetable.pos`, `.warp`, `.tone`, `.unison`, `.table`, `.warpmode`, `.envpos`,
`.reso`, `.sub`, `.keytrack`.

**Ausgangspunkte:** Fläche – Formant, Position 0,3, Hüllk. > Position 0,4, langsamer Amp-Attack;
Bass – Klassisch, Position 0,45, Unisono 0, Sub 0,5, Klang 0,5; heller Lead – Digital, Warp 0,4,
Unisono 0,5.
