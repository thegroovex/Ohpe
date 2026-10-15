# Bandmaschine, Mixer-Dynamik und FX-Slot

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
 Master: Summe + Returns → Kompressor → Bandsättigung → Lautstärke → Limiter (Obergrenze) → Haupt
```

## Bandmaschine

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
* **Limiter** (`mix.master.limit`) mit einstellbarer **Obergrenze** (`mix.master.ceiling`,
  Standard −0,3 dBFS wie in 0.1).
* Die Pegelreduktion ist für Anzeigen lesbar: `Mixer::gainReduction (Kanal)`, Master = 8.
* **Delay-Sync** (0.5, `fxa.delay.sync`): Aus, 1/16, 1/8T, 1/8, 1/4T, 1/8., 1/4, 1/4., 1/2. Jede
  Einstellung außer Aus setzt die Zeit des Send-Delays aus dem Tempo (Host, Link oder intern) und
  ersetzt `fxa.delay.time` (höchstens 2 s).

Stehen alle neuen Regler auf ihrem Standard, klingt der Mixer bit-genau wie in 0.1.

## FX-Slot des Parts

`p1.fx.type`: Aus, Chorus, Phaser, Bitcrusher, Filter-Drive, Delay, Hall – ein Insert-Effekt
zwischen Part 1 und Mixer (und vor dem Band, wenn eine Spur Part 1 aufnimmt). Jeder Effekt
behält seine eigenen Parameter (`p1.fx.chorus.rate` …); Chorus, Phaser, Bitcrusher und
Filter-Drive haben einen eigenen Mischungsregler, Delay und Hall bekommen ihn vom Slot
(`p1.fx.delay.mix`, `p1.fx.reverb.mix`). *Aus* ist ein exakter Bypass. Weitere Parts bekommen
ihre Slots als `p2.fx.*` …, sobald es sie gibt.

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
(keine Speicherreservierung in `Core::process()`) und `tests/plugin/TapeExportTests.cpp` – Details
im englischen Dokument.
