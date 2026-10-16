# Preset-Zucht

*Deutsch · English: [../en/BREEDING.md](../en/BREEDING.md)*

![Die Zucht-Ansicht](../images/ohpe-breeding-de.png)

Die **Zucht** kreuzt zwei Klänge, und du wählst die besten ihrer Kinder aus: Wähle zwei Eltern (deinen aktuellen
Klang, ein beliebiges Part-Preset der Bibliothek, ein Kind einer früheren Brut oder einen Wurf des Würfels), stelle
ein, wie stark die Kinder **mutieren** dürfen, und drücke **ZÜCHTEN**. Sechs Kinder erscheinen; ein Klick auf eines
lässt es hören, gib den guten ein **Herz** und drücke **WEITER** – die nächste Generation stammt von den behaltenen
ab. Jede Generation bleibt in einem Verlauf mit Rückgängig / Wiederholen, und **SPEICHERN** legt ein Kind als
Favorit in der Bibliothek ab.

## Die Zucht-Ansicht öffnen

| Wie | Ergebnis |
|---|---|
| **ZUFALL** halten (eine halbe Sekunde) oder rechts anklicken | öffnet die Zucht-Ansicht; Elternteil A = der aktuelle Klang |
| **ZÜCHTEN** in den Details der Bibliothek (Part-Presets) | schließt die Bibliothek (dein Klang kommt zurück), öffnet die Zucht mit A = aktueller Klang und B = diesem Preset und züchtet sofort |
| MIDI: `lib.breed` | öffnet / schließt die Ansicht (MIDI-Lernen im MIDI-Menü, *Fernsteuerung*) |
| MIDI: `lib.breed.go` / `lib.breed.next` | weiterzüchten / das nächste Kind hören |

Die Ansicht liegt wie die Bibliothek auf der Platte. **Behalten** (Enter) schließt sie mit dem zuletzt gehörten
Kind; **Abbrechen** (Esc) bringt den Klang von vor dem Öffnen zurück. Solange die Ansicht offen ist, würfelt ein
kurzer Druck auf **ZUFALL** einen neuen Elternteil B und züchtet, und **SPEICHERN** speichert das gewählte Kind.

## Eltern, Mutation, Generationen

| Bedienelement | Funktion |
|---|---|
| Schilder **A** / **B** | Menü: *Aktueller Klang*, *Zufälliges Preset*, *Gewähltes Kind*, *Preset ▸* (Part-Presets nach Kategorie; Favoriten mit Herz) |
| Würfel | ein zufälliges Part-Preset der Bibliothek wird Elternteil B |
| **MUTATION** | ziehen: 0 % = die Kinder sind reine Kreuzungen der Eltern (jeder Wert liegt zwischen den Werten der Eltern), 100 % = wild |
| **ZÜCHTEN** | eine neue Brut von sechs aus A und B |
| **WEITER** | die nächste Generation: zwei oder mehr behaltene Kinder werden miteinander gekreuzt (jedes Paar der Reihe nach), ein behaltenes Kind wird mutiert (mindestens 10 %), kein behaltenes Kind = wieder die Eltern |
| ‹ / › | eine Generation zurück / vor (eine neue Brut nach dem Zurückgehen verwirft den Vorwärts-Zweig); die letzten 24 Generationen bleiben |
| ♪ NOTE | jeder Klick auf ein Kind spielt ein kurzes C (ein / aus) |
| Schilder **ERBE** | je Gruppe (Engine, Filter, Hüllkurve, FX, Mod): *Auto* (zufällig je Kind), *A*, *B* oder *Mix* |

## Die Kinder

Jede Kachel zeigt den Namen des Kindes (die ersten Wörter der Eltern und Generation.Kind), ein
**Wellenform-Bildchen**, gefärbt nach seiner **Helligkeit** (violett = dunkel, rosa = mittel, gelb = hell), Pegel
und Helligkeit, seine Engine (bei einem Hybrid mit der Layer-Engine) und eine **Erbleiste**: ein Abschnitt je
Gruppe, weiß = von A, violett = von B, dazwischen = gemischt. Das gewählte Kind hat einen gelben Rahmen und
Lichtbalken (grün, solange seine Note klingt). Rechtsklick auf eine Kachel: *Anhören*, *Behalten*, *Zu Elternteil
A / B machen*, *In der Bibliothek speichern*.

Ein gespeichertes Kind ist ein gewöhnliches Part-Preset (Tag „Bred“, ein Kommentar nennt seine Eltern) und als
Favorit markiert, sodass der Filter *Favoriten* der Bibliothek es sofort findet.

## Wie ein Kind entsteht

Ein Klang wird als **Genom** behandelt: jeder Parameter des Parts, in Gruppen sortiert.

| Gruppe | Parameter |
|---|---|
| **Engine** | Engine-Wahl, Layer, die eigenen Klangparameter der Engines, Mono-Bass |
| **Filter** | Cutoff, Resonanz, Filtertyp, Ton / Helligkeit, Drive, Keytracking der Engines |
| **Hüllkurve** | Amp-, Mod- und dritte Hüllkurve, Tonhöhen-Hüllkurve, Glide, Anschlag |
| **FX** | die sechs Rack-Slots (Typen und Parameter), die 3D-Bühne |
| **Mod** | LFOs, die 16 Matrix-Verbindungen, Makros, MPE-Mengen, Life |
| Pegel | Lautstärke, Part-Gain, Layer-Pegel: gemischt, nie mutiert, danach pegelangeglichen |
| Fest | Stimmung, Stimmen, Pitch-Bend-Bereich, MIDI-Kanal, Skala / Akkord, MPE-Schalter, Morph-Block: von Elternteil A |

* **Kreuzung**: Je Gruppe übernimmt das Kind A, B oder eine Mischung (eine Mischposition je Gruppe, in der eigenen
  Kurve der Parameter, sodass sich eine Frequenz musikalisch mischt). Auswahlen und Schalter werden nie
  interpoliert – sie kommen von einem Elternteil; ganze Zahlen werden gerundet.
* **Stimmig über Engines hinweg**: Die Parameter einer Engine kommen vom Elternteil, das diese Engine spielt; die
  Parameter eines Effekts vom Elternteil, in dessen Slot dieser Effekt steckt; eine Matrix-Verbindung (Quelle, Via,
  Ziel, Kurve, Menge) wandert als Einheit, außer beide Eltern verbinden gleich. Zwei Eltern mit **verschiedenen
  Engines**: Das Kind spielt die Engine des einen; mischt die Engine-Gruppe, kann es die andere Engine als
  **Layer** behalten (ein Hybrid, mit gemäßigtem Layer-Pegel, ohne Kreuzmodulation).
* **Mutation**: kleine gaußsche Schritte auf stufenlosen Parametern (innerhalb 2 … 98 % ihres Bereichs), seltene
  Schritte auf Auswahlen und ganzen Zahlen, sehr selten ein anderer Effekttyp (über 50 %). Nie mutiert: Engine- und
  Sample-Wahl, die Verdrahtung der Verbindungen, Pegel. Resonanz, Feedback, Drive und gain-artige Parameter können
  höchstens einen kleinen Schritt über den lauteren Elternteil steigen.
* **Pegelangleich**: Jedes Kind wird offline gerendert (ein eigener Engine-Kern in einem Hintergrund-Thread, C4 + G4
  für 0,7 s); sein Part-Gain wird so gesetzt, dass es so laut spielt wie seine Eltern (höchstens 6 dB lauter als
  sein gemischter Gain, beliebig leiser), mit der Spitze unter -1,5 dBFS. Dasselbe Rendern liefert das Bildchen und
  die Helligkeit.
* **Reproduzierbar**: Jede Zufallsentscheidung kommt aus Ohpes eigenem Zahlengenerator; ein Startwert ergibt auf
  jeder Plattform dieselbe Brut.

Die Zucht-Sitzung – Eltern, Generationen, behaltene Kinder, die Gruppen-Wahl – wird mit dem Projekt gespeichert
(Plug-in-Zustand, Abschnitt `"breed"`). Der Zucht-Kern (`src/core/breed`) ist engine-unabhängig: Er liest nur die
Parameterbeschreibungen, sodass neue Engines und Effekte ohne Änderung mitzüchten.
