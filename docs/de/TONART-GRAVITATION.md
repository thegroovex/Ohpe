# Tonart-Gravitation (p66)

[English](../en/KEY-GRAVITY.md)

Jeder Part hat direkt nach seinen Engines (jeder der acht) eine Stufe **Tonart-Gravitation**: Sie zieht jeden Teilton
des Klangs zur aktuellen Tonart und zum gehaltenen Akkord. Ein leicht verstimmtes Pad rastet in der Tonart ein, eine
Melodie lehnt sich an die Akkordtöne, und bei hohen Werten werden aus Rauschen und Drum-Loops tonale Texturen, die den
Akkord spielen. Sie liegt im Effekt-Modus auf den letzten beiden Encoder-Seiten **Tonart-Gravitation** und in der
Pro-Ansicht als Karte **Tonart-Gravitation**.

| Regler | Parameter | Wirkung |
|---|---|---|
| Gravitation | `pN.grav.amt` | 0 = aus (der Part klingt genau wie ohne die Stufe: bit-identisch, keine Latenz, keine CPU) … 100 % |
| Ziel | `pN.grav.target` | **Skala** (die Töne der Tonart), **Akkord** (nur die Töne des gehaltenen Akkords) oder **Akkord+Skala** (die Akkordtöne ziehen stärker, die Skala füllt dazwischen) |
| Glide | `pN.grav.glide` | 5 ms … 2 s: wie schnell die Teiltöne bei einem Akkord- oder Tonartwechsel zu den neuen Tönen fließen |
| Kurve | `pN.grav.curve` | −100 % (sanft: die Teiltöne neigen sich nur zu den Tönen) … +100 % (magnetisch: schon wenig Gravitation rastet sie ein) |
| Formant | `pN.grav.formant` | Formant-Erhalt: die Klangfarbe (spektrale Hüllkurve) bleibt, während sich die Teiltöne bewegen |
| Mischung | `pN.grav.mix` | Mischung aus gezogenem Klang und Original (zeitgleich) |
| Qualität | `pN.grav.quality` | **Geringe Latenz** (≈ 11 ms), **Normal** (≈ 21 ms), **Hoch** (≈ 43 ms, löst tiefe Töne am besten auf) |

## Woher die Töne kommen

* **Tonart / Skala**: Tonart und Skala des Instruments (KEY auf der Platte) oder die Tonart des Hosts, wenn er eine
  sendet.
* **Akkord**: die gerade auf allen vier Parts gehaltenen Töne, benannt wie in der Akkordanzeige (ein C mit E und G ist
  C-Dur, einzelne Fremdtöne zählen nicht). Ist nichts gehalten, der letzte Akkord aus dem Harmonie-Gedächtnis; vor dem
  ersten Akkord der Tonika-Dreiklang der Tonart.
* Jede Oktave zählt: ein Teilton geht zur nächsten Zieltonhöhe darüber oder darunter.

## Wie es arbeitet

Ein Kurzzeit-Spektrum (Hann-Fenster, vierfache Überlappung) findet die Spitzen des Klangs; die genaue Frequenz jeder
Spitze ergibt sich aus ihrer Phase von Frame zu Frame. Jede Spitze nimmt die Bins um sich mit (phasengekoppelt, der
Teilton bleibt ein Teilton) und wandert zu ihrer Zieltonhöhe – teilweise, nach Gravitation und Kurve. Teilton-Spuren
gleiten bei einem Akkordwechsel zum neuen Ziel. Anschläge gehen mit ihren eigenen Phasen durch, Drums behalten ihren
Punch. Über 50 % klingt die Energie zwischen den Zieltönen ab, und was auf einem Zielton landet, übernimmt dessen
laufende Phase – aus Rauschen wird ein Akkord.

* **Latenz**: ein Analyse-Frame (siehe oben). Der Host erfährt sie, und alle Parts – und das Drum-Kit – warten auf die
  langsamste laufende Stufe, so bleibt alles im Takt. Steht Gravitation eine Sekunde auf 0, hält die Stufe an und die
  Latenz verschwindet wieder.
* **Echtzeit**: beim Spielen wird kein Speicher angefordert; jede Blockgröße geht (auch ein Sample).
* **CPU**: etwa 1,5 % eines Kerns pro Part (48 kHz, jede Qualität); vier Parts etwa 6 % eines Kerns.
* **Abtastraten**: 44,1 – 192 kHz (der Frame wächst mit der Rate, die Tonhöhenauflösung bleibt gleich).

## Modulation und MIDI

Alle sieben Regler sind Host-Parameter und per MIDI lernbar. Matrix-Ziele (angehängt): 90 **Tonart-Gravitation**,
91 **Gravitation Glide**, 92 **Gravitation Kurve**, 93 **Gravitation Formant**, 94 **Gravitation Mischung** – z. B.
Modrad → Tonart-Gravitation, um einen Drum-Loop in den Akkord zu spielen. Siehe [Modulation](MODULATION.md).

## Werksklänge

*Gravity Noise Choir* (reines Rauschen in den gehaltenen Akkord gezogen), *Gravity Hat Pad* (ein Hi-Hat-Cluster zum Pad
in der Tonart geschmolzen), *Gravity Bell Lock* und *Gravity Drum Wash* zeigen die Stufe; in der Bibliothek findest du
sie über das Namenspräfix „Gravity …“ (nach „Gravity“ suchen – Tags sind nur Kategorien). Das Modrad nimmt bei allen
vier die Gravitation zurück.
