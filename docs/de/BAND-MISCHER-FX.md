# Bandmaschine, Mixer-Dynamik und FX-Rack

*Deutsch · English: [../en/TAPE-MIXER-FX.md](../en/TAPE-MIXER-FX.md) · Parametertabellen:
[../PARAMETERS.md](../PARAMETERS.md)*

Ohpe 0.2 bringt eine Achtspur-Bandmaschine, Dynamik im Mixer, einen Effekt-Slot für den Part,
einen Stereo-Audioeingang im Plug-in und den Offline-Export des Bands als WAV-Dateien.

```
 Eingang ───┐                  ┌─ Bandspur 1..8 ───┬─ Mixer-Kanal (Quelle „Tape N“) ──┐
 Part 1 ─ FX-Slot ─┬───────────┤   (Aufnahme von   │                                   ├─ Master
 (Parts 2-4, Drum) │           │   Eingang / Part) └─ Direktausgang → Out 1..8        │
                   └─ Mixer-Kanal 1 ──────────────────────────────────────────────────┘
 Kanal:  Quelle → EQ → Kompressor-lite → Fader / Panorama → Haupt oder Out N (+ Sends A / B)
 Master: Summe + Returns → Kompressor → Bandsättigung → Lautstärke → Tiefpass → Limiter (Obergrenze) → Haupt
```

## Bandmaschine

### Erste Aufnahme in 30 Sekunden

![Tape-Seite während der Aufnahme](../images/ohpe-tape-recording-de.png)

1. **TAPE** drücken (Modus-Tasten rechts). Das Display zeigt das Transportfeld und die acht Spuren.
2. **Spur wählen**: auf ihre Zeile klicken (am Anfang ist Spur 1 gewählt). Ihre Quelle ist *Part 1* –
   eine andere auf Encoder 2 wählen (*Quelle*: Eingang, Part 1–4, Drum).
3. **REC** drücken (rote Taste unten links – oder **R** in der Standalone-App). Die gewählte Spur wird
   aufnahmebereit, die Aufnahme beginnt, Band und Sequencer laufen. Das Display zeigt **● REC**
   (rot, atmend) und die Zeit.
4. **Spielen** – auf der Klaviatur, einem MIDI-Keyboard oder mit dem Sequencer.
5. **STOPP** drücken (oder **Leertaste**). Noch einmal **STOPP**: zurück zum Anfang. **START**: du hörst
   deine Aufnahme. **Rückgängig** im Display nimmt den ganzen Durchgang zurück.

Weitere Aufnahmen: eine andere Spur aufnahmebereit schalten (Punkt anklicken oder *Aufnahmebereit*
auf Encoder 1) und wieder **REC** drücken; mit *Overdub* (Seite Transport) kommt eine Aufnahme zur Spur
hinzu, statt sie zu ersetzen. **REC** bei laufendem Band = einsteigen, **REC** noch einmal = aussteigen.

### Transport

| Taste / Regler | Wirkung | MIDI-Trigger |
|---|---|---|
| **REC** | Aufnahme mit einem Druck (schaltet die gewählte Spur bereit, wenn keine bereit ist, startet das Band); während der Aufnahme: aussteigen | `tape.tx.rec` |
| **START** (Gehäuse) | Sequencer und Band starten / stoppen zusammen; mit *Start startet Tape* aus nur der Sequencer | `tape.tx.play` |
| **▶** (Tape-Seite) | das Band startet / stoppt (und der Sequencer mit, solange *Start startet Tape* an ist) | `tape.tx.tplay` |
| **STOPP** | alles hält an (die Aufnahme endet); im Stillstand: zum Anfang | `tape.tx.stop` |
| **\|◀** | zum Anfang | `tape.edit.rtz` |
| **SCHLEIFE** | Schleife an / aus (ohne Bereich: die aufgenommene Länge oder vier Takte) | `tape.tx.loop` |
| Bereit-Punkt / *Aufnahmebereit* | schaltet eine Spur bereit / ab; der Trigger wirkt auf die Bearbeitungsspur | `tape.tx.arm` |
| *Start startet Tape* | Einstellung, ab Werk an: START lässt Sequencer und Band zusammen laufen | `tape.tx.link` |

Trigger lösen beim Drücken aus (CC ≥ 64), Taster am Controller arbeiten also als Transporttasten. Sie
gehören nicht zu Presets.

**Acht Stereospuren** mit gemeinsamem Laufwerk. Das Band ist standardmäßig 10 Minuten lang; die
Länge (10 s … 60 min) ist eine Einstellung der Session (`TapeMachine::setLengthSeconds`,
gespeichert im Abschnitt `tape` des Zustands), kein Parameter. Speicher wird nur dort belegt, wo
aufgenommen wurde (Blöcke zu 4096 Frames) – ein langes, leeres Band kostet fast nichts.

| Was | Wie |
|---|---|
| Wiedergabe / Aufnahme | `tape.play`, `tape.rec` (Punch-in / -out während der Wiedergabe). Aufnahme braucht mindestens eine scharf geschaltete Spur (`tape.trN.arm`). |
| Quelle je Spur | `tape.trN.src`: Eingang (Plug-in-Eingangsbus), Part 1–4, Drum. |
| Overdub / Ersetzen | `tape.overdub` an: die neue Aufnahme wird addiert; aus: sie ersetzt das Vorhandene (das alte Material ist beim Ersetzen nicht zu hören). |
| Mithören | Eine scharf geschaltete Spur mit Quelle *Eingang* spielt den Eingang über die Spur (hörbar und routbar); Parts hört man auf ihren eigenen Mixer-Kanälen. |
| Varispeed | `tape.speed` 0,5 … 2,0 (1,0 in der Reglermitte), gleitet beim Laufen weich, 4-Punkt-Interpolation. Die Tonhöhe folgt der Geschwindigkeit. |
| Rückwärts | `tape.reverse` – der Kopf läuft rückwärts; am Bandanfang (oder -ende) bleibt er stehen und ist still. |
| Schleife | `tape.loop`, `tape.loopin`, `tape.loopout` (Sekunden, ganze Frames). |
| Positionieren | `TapeMachine::locate (Sekunden)` (gilt ab dem nächsten Audioblock). |
| Bandcharakter | `tape.sat` (weiche Sättigung, sanfter Höhenverlust), `tape.wow`, `tape.flutter` (Geschwindigkeitsmodulation für alle Spuren gemeinsam, wie bei einem Laufwerk), `tape.hiss` (Bandrauschen). Alles auf 0 = exakte, unveränderte Wiedergabe. |
| Spurpegel | `tape.trN.level` (dB). |
| Direktausgang | `tape.trN.out`: *Nur Mixer* oder Out 1–8. Die Spur spielt dann zusätzlich auf diesem Ausgang (nach Pegel und Bandcharakter); ist der Ausgang nicht aktiv, spielt sie auf Haupt. Mixer-Kanäle können Bandspuren ebenfalls als Quelle wählen (Kanäle 6–8 starten mit den Spuren 1–3). |

### Schnitt

Alle Schnitte arbeiten auf einer Spur, nehmen Zeiten in Sekunden (oder Frames) und sind je ein
Undo-Schritt:

| Schnitt | Ergebnis |
|---|---|
| **Lift** (Start, Ende) | kopiert den Bereich in die Zwischenablage und lässt Stille zurück |
| **Cut** (Start, Ende) | kopiert den Bereich in die Zwischenablage und schließt die Lücke (späteres Material rückt nach vorn) |
| **Drop** (bei) | legt die Zwischenablage bei `bei` über das Vorhandene |
| **Splice** (bei) | fügt die Zwischenablage bei `bei` ein, späteres Material rückt nach hinten (das Bandende schneidet ab) |
| Spur löschen / alles löschen | Stille (rückgängig machbar) |

**Undo / Redo sind unbegrenzt.** Ein Aufnahmedurchgang (vom Drücken bis zum Loslassen der
Aufnahme) ist ein Schritt. Ein neuer Schnitt nach Undo verwirft die Redo-Schritte. Während der
Aufnahme werden Schnitte abgelehnt (Rückgabe `false`).

### Wie es funktioniert (Echtzeit)

* Jede Spur ist eine Seitentabelle aus Blöcken zu 4096 Frames; leere Seiten sind Stille.
* Der Audio-Thread reserviert nie Speicher: Der Message-Thread hält einen Vorrat von 256 leeren
  Blöcken gefüllt (`housekeeping()`, vom Plug-in mit 20 Hz aufgerufen) und übergibt sie über eine
  wartefreie Queue.
* Übernommene Blöcke sind eingefroren. Eine Aufnahme in einen eingefrorenen Block kopiert ihn
  zuerst in einen frischen (Copy-on-Write) – jeder Undo-Zustand bleibt unverändert. Undo-Zustände
  teilen sich alle unveränderten Blöcke und Spuren; ein Schritt kostet eine Seitenliste je
  geänderter Spur plus die wirklich geänderten Blöcke.
* Schnitte und Undo tauschen Seitentabellen auf dem Message-Thread unter einem
  Zwei-Flaggen-Ausschluss: Der Message-Thread wartet, bis der Audio-Thread das Band verlassen hat
  (höchstens einen Block); der Audio-Thread wartet nie – für die wenigen Millisekunden eines
  Schnitts ist das Band still.
* Gehen der Aufnahme je die freien Blöcke aus, werden Samples verworfen und gezählt
  (`dropouts()`) – nie auf dem Audio-Thread reserviert.

## Mixer-Dynamik

* **Kompressor-lite** je Kanal (`mix.chN.comp`, 0 = aus): ein Regler setzt Schwelle
  (−4 … −30 dB), Ratio (1,5 … 6) und die halbe Aufholverstärkung; nach dem EQ, vor dem Fader.
* **Master-Kompressor** (`mix.master.comp` an / aus, Schwelle, Ratio, Attack, Release,
  Aufholverstärkung), Stereo-gekoppelt, weiches Knie.
* **Master-Bandsättigung** (`mix.master.tapesat`, 0 = aus).
* **Master-Tiefpass** (0.6, `mix.master.lowpass`, 500 Hz … 20 kHz, 12 dB/Oktave; 20 kHz = aus): nach
  der Lautstärke, vor dem Limiter – macht den ganzen Mix dunkler, z. B. 4–5 kHz für Lo-Fi.
* **Limiter** (`mix.master.limit`) mit einstellbarer **Obergrenze** (`mix.master.ceiling`,
  Standard −0,3 dBFS wie in 0.1). Seit 0.8 schaut er 1,5 ms voraus (83 Samples Latenz bei 48 kHz mit seinem True-Peak-Interpolator, immer gleich –
  auch ausgeschaltet und an den Aux-Ausgängen – und dem Host gemeldet), misst echte Spitzen (Zwischen-Sample-Spitzen einer 4-fachen Windowed-Sinc-Interpolation, genau bis nahe Nyquist, z. B. bei einer gesättigten Hi-Hat) und senkt seine
  Verstärkung vor einer Spitze sanft ab, statt zu springen – laute Mischungen bleiben sauber, ohne
  Clipping-Verzerrung. Sein Release ist zweistufig: schnell nach einer einzelnen Spitze, gleitend langsamer, solange
  er dauernd arbeitet (kein Pumpen, und mehr Pegel macht ihn nie leiser).
* **Kompressor-Detektor** (0.8): Master-Kompressor, Kompressor-lite und der Rack-Kompressor halten eine Spitze
  12 ms, bevor sie loslassen – schnelle Einstellungen folgen nicht mehr der Wellenform eines Basses (keine
  Verzerrung im Sub).
* **Ducking** (0.8, Sidechain): `mix.duck.src` wählt den Auslöser-Kanal (Aus, Kanal 1 … Kanal 8), meist die
  Drums (Kanal 5). Dessen Tiefen (unter etwa 150 Hz – die Kick) drücken die anderen Kanäle um ihren eigenen
  Betrag `mix.chN.duck` (0 … 1, bis −24 dB bei vollem Schlag); `mix.duck.release` (0,03 … 1 s) bestimmt,
  wie schnell sie zurückkommen. Der Auslöser folgt seiner eigenen jüngsten Spitze, so erreicht jede Kick die
  eingestellte Tiefe, egal wie laut sie ist. Die Werks-Projekte ducken die Bässe am stärksten, Flächen
  weniger, Leads / Keys ein wenig.
* Die Pegelreduktion ist für Anzeigen lesbar: `Mixer::gainReduction (Kanal)`, Master = 8.
* **Delay-Sync** (0.5, `fxa.delay.sync`): Aus, 1/16, 1/8T, 1/8, 1/4T, 1/8., 1/4, 1/4., 1/2. Jede
  Einstellung außer Aus setzt die Zeit des Send-Delays aus dem Tempo (Host, Link oder intern) und
  ersetzt `fxa.delay.time` (höchstens 2 s).

Stehen alle neuen Regler auf ihrem Standard, klingt der Mixer bit-genau wie in 0.1.

## FX-Slot des Parts

`p1.fx.type`: Aus, Chorus, Phaser, Bitcrusher, Filter-Drive, Delay, Hall, Hyper, Verzerrung, Flanger, Kompressor, Multiband, EQ, Filter, Echo, Raum (0.7), Mitte/Seite (0.7) – ein Insert-Effekt
zwischen Part 1 und Mixer (und vor dem Band, wenn eine Spur Part 1 aufnimmt). Jeder Effekt
behält seine eigenen Parameter (`p1.fx.chorus.rate` …); Chorus, Phaser, Bitcrusher und
Filter-Drive haben einen eigenen Mischungsregler, Delay und Hall bekommen ihn vom Slot
(`p1.fx.delay.mix`, `p1.fx.reverb.mix`). *Aus* ist ein exakter Bypass. Seit 0.7 ist dieser Slot
Slot 1 im FX-Rack des Parts (unten).

**Chorus-Ensemble** (0.8): der Chorus hat vier weitere Parameter (angehängt, in jedem Slot jedes Racks):
*Stimmen* (1 … 4; 1 = der klassische Chorus mit zwei Abgriffen, 2 … 4 = Ensemble: gestaffelte Abgriffe mit
gleichmäßig verteilten LFO-Phasen, über das Stereofeld verteilt), *Drift* (ein langsames zufälliges Wandern
jedes Abgriffs – weniger statisch, eher wie analoge Ensembles), *Breite* (Stereobreite der Ensemble-Stimmen) und
*Tiefensperre* (20 Hz … 1 kHz, 12 dB/Oktave nur auf dem Chorus-Anteil: darunter bleibt der Klang trocken und
mono, ein Bass mit Chorus behält einen festen Sub). Die Standardwerte (1 Stimme, kein Drift, 20 Hz) ergeben den
Chorus von 0.2. Seit dem Punch-Durchgang von 0.8 ist jeder Werks-Chorus ein Ensemble, und E-Pianos, Orgeln,
Flächen, Plucks, Leads, Arps, Stimmen und die Mitten von Reese- / Neuro-Bässen tragen eines.

## FX-Rack (0.7)

Jeder Part hat ein **FX-Rack mit sechs Slots in Reihe**, der Master eines mit vier:

```
 Part ─ FX 1 ─ FX 2 ─ FX 3 ─ FX 4 ─ FX 5 ─ FX 6 ─▶ Mixerkanal / Band
 Master: Summe + Returns ─ MFX 1 ─ MFX 2 ─ MFX 3 ─ MFX 4 ─ Kompressor ─ Bandsättigung ─ Lautstärke ─ Tiefpass ─ Limiter
```

* Slot 1 ist der FX-Slot oben (`pN.fx.*`), Slots 2 – 6 sind `pN.fx2.*` … `pN.fx6.*`, die
  Master-Slots `mfx1.*` … `mfx4.*`. Jeder Slot hat dieselbe Typenliste und pro Typ einen eigenen
  Parameterblock (`p1.fx3.chorus.rate`), zwei Slots können also denselben Effekt mit
  verschiedenen Einstellungen tragen.
* Pro Slot: **Mischung** (`pN.fxK.mix`, trocken / nass des ganzen Slots, 100 % = der Ausgang des
  Effekts) und **Bypass** (`pN.fxK.bypass`). Mischung und Bypass gleiten über 10 ms – kein Klicken
  beim Umschalten.
* **Reihenfolge:** Die Slots laufen 1, 2, … ; ein Slot wird verschoben, indem die Parameter der
  betroffenen Slots umgeschrieben werden (Typ, Mischung, Bypass und alle Blöcke) – die
  Reihenfolge ist Teil des Presets, rückgängig zu machen und automatisierbar wie jeder Wert.
* **Kompatibilität:** Presets und Projekte aus 0.6 landen unverändert in Slot 1 und klingen
  bitgenau gleich (Slot-Mischung 100 %, die anderen Slots aus). Ein leerer oder umgangener Slot
  ist ein exakter Bypass.
* **Echtzeit:** Slot 1 hat jeden Typ sofort bereit (wie bisher). Slots 2 – 6 und die Master-Slots
  belegen den Speicher eines Effekts erst, wenn der Typ zum ersten Mal gewählt wird (das Plug-in
  erledigt das einen Augenblick später im Message-Thread; bis dahin reicht der Slot das Signal
  trocken durch).
* Die Namen im Host unterscheiden die Slots: „P2 FX3 Chorus-Tempo“, „Master-FX 1 Delay-Mischung“
  (der 0.2-Slot von Part 1 behält seine Namen).
* Die Modulationsmatrix kann die Mischung jedes Slots und vier Parameter seines Effekts
  bewegen – bei den 0.2-Typen die ersten vier, bei den Rack-Typen die unten als *Mod 1–4*
  genannten ([MODULATION.md](MODULATION.md)).
* Auf dem Bildschirm: die Ansichten **Rack** und **Master** im Effekte-Modus
  ([BEDIENUNG.md](BEDIENUNG.md#fx-rack-ansichten-07)).

### Effekttypen des Racks

Neben den sechs 0.2-Typen bietet jeder Slot neun Rack-Typen. Alles, was sich bewegt (Cutoffs,
Zeiten, Verstärkungen, Drive), gleitet Sample für Sample – Automation und Modulation zippern nie;
die nichtlinearen Stufen laufen mit Oversampling.

| Typ | Was er tut | Parameter | Mod 1–4 |
|---|---|---|---|
| **Hyper** | Unisono-Verbreiterer: bis zu sieben langsam driftende, verstimmte Kopien im Stereofeld, danach eine *Dimension*-Stufe aus über Kreuz gekoppelten kurzen Delays; *Neustart* startet die Bewegung mit jeder Note neu | Tempo, Verstimmung, Stimmen, Breite, Dimension, Neustart, Mischung | Tempo, Verstimmung, Breite, Dimension |
| **Verzerrung** | acht Modi – Röhre, Soft Clip, Hard Clip, Diode, lineare Faltung, Sinus-Faltung, asymmetrisch, Downsample – mit Bias, Vorfilter (Tief-, Band-, Hochpass) und Klangregelung danach | Modus, Drive, Bias, Vorfilter, Vorfilter-Frequenz, Klang, Ausgang, Mischung | Drive, Bias, Vorfilter-Frequenz, Klang |
| **Flanger** | Stereo-Flanger, frei oder zum Tempo synchron, positives oder negatives Feedback | Tempo, Sync, Tiefe, Verzögerung, Feedback, Stereo, Mischung | Tempo, Tiefe, Verzögerung, Feedback |
| **Kompressor** | Stereo-gekoppelter Kompressor mit weichem Knie und Aufholverstärkung; *Mischung* ergibt Parallelkompression | Schwelle, Ratio, Attack, Release, Knie, Aufholverstärkung, Mischung | Schwelle, Ratio, Attack, Aufholverstärkung |
| **Multiband** | drei Bänder mit Aufwärts- und Abwärtskompression; bei Tiefe 0 summieren die Weichen linear | Tiefe, aufwärts, abwärts, Zeit, Tiefen / Mitten / Höhen, Trennung tief / hoch, Ausgang, Mischung | Tiefe, Tiefen, Mitten, Höhen |
| **EQ** | Tiefen-Kuhschwanz, drei Glocken, Höhen-Kuhschwanz | Frequenzen, Gains, Güte, Ausgang | Band 1 Frequenz / Gain, Band 2 Frequenz / Gain |
| **Filter** | Tief- / Hochpass 12 oder 24 dB, Bandpass, Notch, Kamm + / −, Vokal-Formant; Drive davor; der Cutoff gleitet in Oktaven | Typ, Cutoff, Resonanz, Drive, Mischung | Cutoff, Resonanz, Drive, Mischung |
| **Echo** | Stereo-, Ping-Pong- oder Tap-Delay (rechter Tap im Verhältnis zum linken), frei oder synchron, gefiltertes Feedback | Modus, Sync, Zeit, Verhältnis, Feedback, Tiefensperre, Höhensperre, Breite, Mischung | Feedback, Zeit (frei), Höhensperre, Breite |
| **Raum** | Platten- oder Hallraum: Eingangsdiffusion und ein Feedback-Netz aus acht Leitungen mit sanfter Modulation | Modus, Größe, Nachhall, Dämpfung, Pre-Delay, Breite, Mischung | Größe, Nachhall, Dämpfung, Pre-Delay |

IDs, Bereiche und Vorgaben: [PARAMETERS.md](../PARAMETERS.md#07--rack-effect-types--effekttypen-des-racks).

### Mitte/Seite-Stereo (0.7)

**Mitte/Seite** (Typ 16, nach den neun 0.7-Rack-Typen an die Typenliste angehängt) bearbeitet das Stereobild. Der Effekt steht in
jedem Slot jedes Racks zur Wahl – den sechs Slots jedes Parts und den vier Master-Slots – mit einem
eigenen Parameterblock pro Slot (`p1.fx.midside.width`, `p2.fx4.midside.width`, `mfx1.midside.width` …).
Er zerlegt das Signal in Mitte M = (L + R) / 2 und Seite S = (L − R) / 2, formt beide und setzt sie
wieder zusammen:

| Parameter (`<Slot>.midside.*`) | Bereich | Wirkung |
|---|---|---|
| Stereobreite `width` | 0 … 200 % | Seite × Breite: 0 % = mono, 100 % = unverändert, 200 % = doppelt so breit |
| Mitte-Pegel `mid` / Seite-Pegel `side` | −24 … +12 dB | Pegel von M und von S |
| Balance `balance` | −100 … +100 % | Links-/Rechts-Balance |
| Bass mono `bassmono` + Mono unter `monofreq` | an / aus, 40 … 500 Hz | entfernt die Seite unterhalb der Frequenz (24 dB/Okt.): straffe, monosichere Tiefen, breite Höhen |
| Mikroversatz `haas` + Versatzzeit `haastime` | 0 … 100 %, 1 … 30 ms | mischt die um einige Millisekunden verzögerte Mitte in die Seite: breiter, ohne die Monosumme zu ändern |
| Mitte-Tilt `midtilt` / Seite-Tilt `sidetilt` | −6 … +6 dB | je ein Tilt-EQ für M und S um 650 Hz (+ = heller, − = dunkler) |

* **Monosicher:** Breite, Seite-Pegel, Bass mono, Seite-Tilt und Mikroversatz wirken nur auf S, und
  S hebt sich in der Monosumme L + R = 2 M auf – beim Zusammenlegen auf mono geht an der Mitte nichts
  verloren und nichts verfärbt sich.
* **Sauber:** Alle Pegel gleiten (20 ms), Drehen an den Encodern oder Modulation klickt also nie;
  nach der Vorbereitung des Slots wird kein Speicher reserviert. Stehen alle Parameter auf dem
  Standard (Breite 100 %, 0 dB, Balance Mitte, Bass mono aus, Mikroversatz 0), reicht der Effekt das
  Signal **bitgenau** durch.
* **Modulation:** Die Matrix-Ziele *FX k Param 1–4* sind Breite, Mitte-Pegel, Seite-Pegel und Balance.
* **Im Master-Rack** (`mfx1` … `mfx4`, vor Master-Kompressor und Limiter) formt er die ganze Mischung:
  *Bass mono* hält die Tiefen in der Mitte, *Breite* macht die Mischung schmaler oder breiter.
* Zeigen die Encoder einen Mitte/Seite-Slot, zeigt das Display das **Stereobild** dieses Parts nach
  seinem Rack bzw. des Master-Ausgangs bei einem Master-Slot (Goniometer, Korrelation, M-/S-Pegel;
  siehe [BEDIENUNG.md](BEDIENUNG.md)).

![FX-Slot Mitte/Seite mit Stereobild](../images/ohpe-midside-scope-de.png)

### Weitere FX-Typen (0.8)

Fünf Typen sind an die Typenliste jedes Slots angehängt (`type` 17 – 21, nach Mitte/Seite). Jeder
ist ein vollständiger Insert mit eigenen Reglern (`pN.fxK.<id>.*`, `mfxK.<id>.*`):

| Typ | Was er tut | Regler |
|---|---|---|
| **Kammfilter** (`comb`) | modulierter Kammfilter mit gedämpftem Feedback (negativ = hohl, ungerade Obertöne), Stereoversatz | `freq`, `feedback`, `damp`, `rate`, `depth`, `spread`, `mix` |
| **Formant** (`formant`) | drei Formant-Bandpässe, die a – e – i – o – u morphen, verschiebbar, mit LFO auf dem Vokal (frei oder tempo-synchron, startet mit jeder Note neu) | `vowel`, `shift`, `reso`, `rate`, `sync`, `depth`, `mix` |
| **Wobble** (`wobble`) | resonanter Tief- / Bandpass, zwischen zwei Cutoffs von einem tempo-synchronen LFO gefahren (Sinus, Dreieck, Sägezähne, Rechteck, S&H), der mit jeder Note neu startet (`retrig`) | `rate`, `sync`, `shape`, `low`, `high`, `reso`, `mode`, `drive`, `retrig`, `mix` |
| **Tremolo** (`tremolo`) | Tremolo oder Autopan, frei oder synchron | `rate`, `sync`, `depth`, `mode`, `shape` |
| **Stereobreite** (`width`) | Seitenpegel 0 – 200 %, Haas-artige Spreizung, die sich in der Mono-Summe aufhebt, mono unterhalb einer Frequenz (4. Ordnung) | `width`, `spread`, `haas` (ms), `mono` (20 Hz = aus), `output` |

Sync-Werte: Frei (Tempo in Hz), 1/1 … 1/32, punktiert, Triolen, dann 2/1 und 4/1 – vom Host, von Link
oder vom internen Tempo. Die Effekte sind eigenständige Klassen (`src/core/fx/InsertEffects2.h`).

## Audioeingang (Plug-in)

Das Plug-in hat einen Audio-Eingangsbus **„Input“** (Stereo oder Mono), standardmäßig aus,
damit Hosts ein normales Instrument sehen. Im Host aktivieren (Sidechain- / Eingangsrouting), um
externes Audio mit der Spurquelle *Eingang* aufzunehmen. Die Standalone-App öffnet noch kein
Eingangsgerät (dafür braucht das App-Bundle einen Mikrofon-Berechtigungseintrag; geplant).

## Bounce / Export

`tapeio::bounce (core, options)` (`src/plugin/TapeIO.h`, Message-Thread) rendert offline und
schneller als Echtzeit, während das Live-Audio weiterläuft:

* eine eigene Kopie des Instruments mit den aktuellen Parametern und dem übernommenen
  Bandinhalt, vom Bandanfang bis zum Ende des Inhalts plus Ausklang (Standard 2 s),
  Geschwindigkeit 1 vorwärts, ohne Aufnahme, Sequencer gestoppt;
* `<Name> Master.wav` – der Haupt-Bus (Kanäle auf Out 1–8 sind nicht enthalten);
* `<Name> Track 1.wav` … `Track 8.wav` – die acht Spuren nach Pegel und Bandcharakter, vor dem
  Mixer, alle gleich lang, damit sie in jedem Editor übereinander passen;
* 24-Bit-Stereo-WAV mit der aktuellen Abtastrate. Fehler kommen lokalisiert zurück
  (`BounceResult`).

## Bandaudio in Sessions

Der Plug-in-Zustand bleibt das JSON-Dokument aus 0.1; enthält das Band Audio, folgt nach einem
Null-Byte das Bandaudio: ein kleiner Kopf (`OHPETAPE`, Version, Abtastrate, Länge) und jeder
Block mit Audio als float32 (verlustfrei, Little-Endian). Stille Blöcke werden nicht
gespeichert. Eine Session ist damit in sich vollständig – nichts geht verloren, wenn ein Projekt
verschoben oder archiviert wird –, kostet aber etwa 23 MB je Stereo-Spurminute Aufnahme (lange
Aufnahmen bei Bedarf als WAV bouncen). Nicht gespeichert: Undo-Verlauf, Zwischenablage und eine
Aufnahme, die beim Speichern durch den Host gerade noch läuft. Ein Zustand ohne Bandaudio (0.1
oder leeres Band) lädt mit leerem Band. Ohpe 0.1 kann Zustände mit Bandaudio nicht lesen.

## Tests

`tests/core/TapeTests.cpp`, `tests/core/FxTests.cpp`, `tests/core/AllocationTests.cpp`
(keine Speicherreservierung in `Core::process()`), `tests/plugin/TapeExportTests.cpp`, seit 0.7
`tests/core/StereoTests.cpp` (Mitte/Seite: Breite 100 % = bitgenauer Bypass, Breite 0 = mono, Bass
mono unterhalb der Frequenz, Mikroversatz monokompatibel, kein NaN; FIFO; Parameter in jedem
Rack-Slot) und `tests/plugin/StereoDisplayTests.cpp` – Details im englischen Dokument.
