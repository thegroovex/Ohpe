# Layer-Engine und Kreuzmodulation (0.8)

*Deutsch · English: [../../en/engines/layer.md](../../en/engines/layer.md)*

Jeder Part kann mit jeder Note eine **zweite Engine** spielen: den **Layer**. Sein Klang nutzt die
Parameter, die diese Engine im Part ohnehin hat (`pN.wavetable.*`, `pN.grain.*` …); ein Part-Preset
kann so etwa eine gezupfte Saite mit einer Grain-Wolke verbinden, einen Reese mit einer Stimme,
ein Klavier mit seiner eigenen eingefrorenen Aufnahme. Solche Presets tragen die Kategorie **Hybrid**.

| Parameter | ID | Bereich | Wirkung |
|---|---|---|---|
| Layer-Engine | `pN.layer.engine` | Aus, FM … Sampler | die zweite Engine (Aus oder die eigene Engine des Parts = kein Layer) |
| Layer-Pegel | `pN.layer.level` | 0–1 | Pegel des Layers |
| Layer-Stimmung | `pN.layer.tune` | −24 … +24 Halbtöne | z. B. eine Oktave oder Quinte höher |
| Layer-Feinstimmung | `pN.layer.fine` | ±50 Cent | Verstimmung gegen die Haupt-Engine |
| Kreuzmod.-Art | `pN.layer.xmode` | Ring, AM, Falten | wie der Layer die Haupt-Engine moduliert |
| Kreuzmodulation | `pN.layer.xmod` | 0–1 | Stärke der Kreuzmodulation (0 = einfach geschichtet) |
| Layer-Breite | `pN.layer.width` | 0–1 | Stereobreite des Layers (0 mono, 0,5 unverändert, 1 doppelte Seite) |
| Layer eigene Hüllkurve | `pN.layer.ownenv` | aus / an | der Layer nutzt eine eigene Amp-Hüllkurve … |
| Layer Attack / Decay / Sustain / Release | `pN.layer.a` `.d` `.s` `.r` | | … mit diesen Zeiten (z. B. ein Zupfton, der zu einer langsamen Wolke aufblüht) |

**Kreuzmodulation:** *Ring* multipliziert die Haupt-Engine mit dem Layer (metallisch, mit
verstimmtem Layer unharmonisch), *AM* lässt die Hüllkurve des Layers die Haupt-Engine formen (ein
Grain-Layer zerhackt eine Fläche), *Falten* faltet die Summe beider (Obertöne, die sich mit beiden
Engines bewegen). Mit MPE oder Aftertouch verschiebt das Ziel *Layer-Mischung* das Verhältnis von
Haupt-Engine und Layer.

Der Layer spielt dieselben Noten, dasselbe Glide, dieselbe Tonhöhen-Hüllkurve, denselben LFO und
Ausdruck wie die Haupt-Engine; seine Stimmen zählen zum selben Limit von 16 Stimmen pro Engine. Ist
der Layer aus, klingt der Part genau wie vor 0.8 (getestet). Die Seite **Layer** folgt im
Synth-Modus auf die Engine-Seiten.
