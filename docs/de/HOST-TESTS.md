# Host-Tests

Ohpe – designed by groovex. English: [HOST-TESTING.md](../en/HOST-TESTING.md)

## Automatische Host-Matrix (jeder CI-Lauf)

`ohpe_hosttest` ist ein kleiner Host ohne Bildschirm auf Basis von JUCEs eigenem Plug-in-Hosting. Er lädt das
**gebaute** Plug-in-Bundle – das VST3 unter Linux, macOS und Windows, auf dem Mac zusätzlich die installierte Audio
Unit – genau wie eine DAW und lässt die CI scheitern bei Absturz, Hänger (Watchdog je Abschnitt), NaN / Inf /
entgleistem Sample (> +12 dBFS) oder Abweichung.

| Abschnitt | Was geprüft wird |
|---|---|
| `basic` | Ein Instrument im Bundle, MIDI-Eingang, 9 Stereo-Ausgangsbusse + 1 Eingangsbus, Editor vorhanden. Jede Parameter-ID aus `tests/core/golden/param_ids_v1.txt` ist unter ihrer Host-ID erreichbar, keine zwei IDs teilen sich eine (Automationsspuren bleiben, wo sie sind). Still im Leerlauf, eine Note klingt, der Zustand lädt in einer zweiten Instanz. |
| `lifecycle` | 8× Anlegen / Vorbereiten / Spielen / Freigeben / Zerstören (zwei Abtastraten, Puffergrößen, nur Hauptbus und alle Busse), Anlegen / Zerstören ohne Vorbereiten, zweimal Vorbereiten ohne Freigabe, zweimal Freigeben, mehrere Instanzen in anderer Reihenfolge zerstört. |
| `state` | Eine reiche Session – die Fixture mit Bandaudio, in jedem der vier Parts ein anderes Preset, Einstellungen in allen Parts, Mischer, Master-FX, Band und Metronom – gespeichert und geladen: `getStateInformation` ist **byte-gleich** in einer zweiten Instanz, in einer dritten mit anderer Blockgröße und nur dem Hauptbus und beim Zurückladen in dieselbe Instanz nach dem Spielen. Presets je Part, Bandaudio und die vom Host gesetzten Werte stehen im Zustand; der Host sieht die geladenen Werte; leere / beschädigte Zustände halten die Instanz nicht an. |
| `automation` | Eine Stichprobe von etwa 170 Parameter-IDs (jede 41. Klangeinstellung plus wichtige) wird 0 → 1 → Standard in 32-Sample-Schritten gefahren, während alle vier Parts spielen. Samplegenaue Rampen: die Lautstärke von Part 1 pro Sample automatisiert (1-Sample-Blöcke) – gleich auf zwei frischen Instanzen, vor Rampenbeginn ändert sich nichts, die Rampe erreicht den Klang, 1- und 64-Sample-Automation enden auf gleichem Pegel. Automatisierte Werte landen im gespeicherten Zustand und kommen für den Host zurück. |
| `bounce` | Offline-Renderings (nicht in Echtzeit) einer Session mit Bandwiedergabe und Noten, alle 18 Ausgangskanäle: zwei Instanzen und zwei Durchgänge einer Instanz sind **bit-gleich**. |
| `rates` | 44,1 / 48 / 96 kHz × Blöcke 32, 64 … 2048; ungerade und wechselnde Blockgrößen (1 … 2048) innerhalb einer Vorbereitung; 1-Sample-Blöcke; dieselbe Note klingt bei 44,1 und 96 kHz etwa gleich laut. |
| `bypass` | Der Bypass-Parameter des Hosts: still im Bypass, spielt danach wieder, Umschalten in jedem Block ohne Störungen. |
| `latency` | Die gemeldete Latenz je Abtastrate ist plausibel, bleibt beim Spielen konstant, und vor ihr erklingt nichts. |
| `multi` | Vier Instanzen, parallel auf eigenen Threads verarbeitet, klingen jede bit-gleich wie allein; eine Änderung in einer bleibt in dieser. |
| `editor` | Spielt ohne Editor; der Editor öffnet und schließt dreimal, während das Audio auf eigenem Thread läuft; spielt danach (unter Linux wird der Editor ohne Fenster angelegt und zerstört: der X-Server ohne Bildschirm hat keinen Fenstermanager zum Einbetten). |
| `transport` | Host-Transport gestoppt → läuft mit 120 BPM → Tempowechsel auf 90 → Loop-Sprünge → Stopp → läuft mit 140 ab Schlag 16 → Sprung zurück, bei eingeschaltetem Sequenzer-Run von Ohpe: das Metronom („Klick im Plug-in“) klickt auf jedem Host-Schlag auf 2 Samples genau (nach der gemeldeten Latenz) und schweigt, solange der Host steht. |

Unter Linux, macOS und Windows ist jeder Abschnitt ein eigener CTest (`vst3_host_test`, `vst3_host_<abschnitt>`);
der auval-Job auf dem Mac prüft alle Abschnitte an der installierten Audio Unit. Lokal:

```
build/ohpe_hosttest_artefacts/Release/ohpe_hosttest build/Ohpe_artefacts/Release/VST3/Ohpe.vst3 --section all --source .
build/ohpe_hosttest_artefacts/Release/ohpe_hosttest ~/Library/Audio/Plug-Ins/Components/Ohpe.component --section state --source .
```

`OHPE_HOSTTEST_DUMP=/tmp/ohpe` schreibt bei einem fehlgeschlagenen Vergleich beide Zustände
(`/tmp/ohpe.<tag>.A.bin` / `.B.bin`). Zusätzlich laufen pluginval (Strenge 5, Linux) und `auval -strict` (macOS).

## Manueller Testplan (Tester, vor einem Release)

Der automatische Host deckt ab, was ohne Bildschirm und Mensch läuft. Bitte geht diese Liste in **eurer DAW** durch
(jeder Host, der VST3- oder Audio-Unit-Instrumente lädt) und notiert DAW, Version, System und Plug-in-Format. Fehler
bitte als GitHub-Issue mit den Schritten und wenn möglich der Projektdatei melden.

### 1. Laden und Scannen
- [ ] Ein frischer Plug-in-Scan findet **Ohpe** als Instrument (VST3; auf dem Mac auch die Audio Unit).
- [ ] Ohpe auf eine Instrumentenspur legen: der Editor öffnet, der Startklang spielt vom MIDI-Keyboard.
- [ ] Den Editor beim Spielen mehrmals schließen und öffnen: keine Aussetzer, kein Absturz.
- [ ] Eine zweite und dritte Instanz einfügen: jede spielt ihren eigenen Klang.

### 2. Speichern und Laden
- [ ] In jedem der vier Parts ein anderes Preset laden, einige Regler ändern, ein paar Sekunden aufs Band aufnehmen.
- [ ] Projekt speichern, schließen, wieder öffnen: alle vier Parts, Preset-Namen, Reglerstellungen und das Bandaudio
      sind wieder da. Abspielen – es klingt gleich.
- [ ] Spur duplizieren (oder Instanz kopieren): die Kopie klingt identisch.
- [ ] Ein DAW-Preset der Instanz speichern (falls angeboten) und auf einer neuen Instanz laden: gleicher Klang.

### 3. Automation
- [ ] Automation eines Reglers aufnehmen, indem man ihn in Ohpes Editor bewegt: die Spur wird geschrieben und spielt ab.
- [ ] Eine Rampe in die Spur zeichnen (z. B. Lautstärke Part 1, Filter-Cutoff): der Klang folgt weich, auch im Loop.
- [ ] Je einen Parameter in allen vier Parts automatisieren: der richtige Part reagiert.
- [ ] Ein per MIDI gelernter Regler (MIDI-Learn in Ohpe) bewegt auch die Spur der DAW.

### 4. Transport und Tempo
- [ ] Ohpes Sequenzer-**Run** einschalten (solange er an ist, folgt Ohpe dem Host-Transport) und das Metronom mit „Klick im Plug-in“: es klickt auf den Schlägen der DAW. Starten, stoppen, einen
      Bereich loopen, in der Zeitleiste springen – es folgt und bleibt im Raster.
- [ ] Projekttempo ändern (auch ein Tempowechsel mitten im Song): tempo-synchrone LFOs, Delays und die Sequenzer folgen.
- [ ] Bei eingeschaltetem Run startet die Play-Taste der DAW das Sequenzer-Pattern; Stopp hält es an.

### 5. Offline-Bounce / Export
- [ ] Einen Songabschnitt zweimal schneller als Echtzeit exportieren: beide Dateien sind gleich (Nulltest: eine
      invertieren, summieren – Stille).
- [ ] In Echtzeit exportieren: klingt wie der Offline-Export.
- [ ] Mit auf eigene Spuren gelegten Aux-Ausgängen von Ohpe exportieren: jede Spur ist da.

### 6. Abtastrate, Puffergröße, Latenz
- [ ] Das Projekt zwischen 44,1, 48 und 96 kHz umschalten: Ohpe spielt weiter und klingt gleich.
- [ ] Die Audio-Puffergröße ändern (klein und groß, z. B. 32 und 2048): kein Knacken über das hinaus, was das System
      mit anderen Instrumenten zeigt, kein Absturz.
- [ ] Eine MIDI-Spur aufnehmen, dann eine Klickspur: mit eingeschaltetem Latenzausgleich der DAW ist Ohpe im Takt.

### 7. Bypass, Ausgänge, Einfrieren
- [ ] Die Instanz in der DAW auf Bypass schalten, während Noten klingen: Stille; Bypass aus: spielt wieder, keine
      hängenden Noten.
- [ ] Die Mehrfachausgänge aktivieren: jeder auf einen Aux-Ausgang gelegte Mischerkanal kommt auf einem eigenen
      DAW-Kanal an.
- [ ] Spur einfrieren / auftauen (oder „an Ort und Stelle rendern“): das eingefrorene Audio entspricht dem Live-Klang.

### 8. Belastung
- [ ] Ein Projekt mit 8 Instanzen spielt 30 Minuten ohne Aussetzer über die Grenzen des Systems hinaus.
- [ ] Eine große Session laden, während der Transport läuft: kein Absturz.
- [ ] Die DAW mit offenem Editor beenden: kein Absturz; der nächste Start lädt das Projekt normal.
