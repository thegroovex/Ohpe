# Robustheits- und Dauertests

Ohpe – designed by groovex. English: [SOAK-TEST.md](../en/SOAK-TEST.md)

## Automatisch (bei jedem CI-Lauf)

Die Kern-Tests (`ohpe_core_tests`, ohne Audiogerät) prüfen unter Linux, macOS und Windows:

| Test | Was er zeigt |
|---|---|
| `robustness: damaged …` | Abgeschnittene, beschädigte (gekippte / eingefügte / gelöschte / wiederholte Bytes), übergroße (4-MB-Texte, 50 000-fach verschachtelt) und typfalsche Dokumente bringen nichts zum Absturz, Hängen oder Werfen einer Ausnahme: Sitzungszustände und ihre Abschnitte (MIDI-Zuordnung, Sequenzer, Morph, Bewegung, Harmonie, Band-Einstellungen, Oberfläche), Part- / Projekt-Presets, `.ohpetape`-Dateien und das Band-Audio einer Sitzung, `.ohpewt`-Dateien (auch Base64), `pack.json` und Paketpläne, die Inhalts-Registry, Update-Einstellungen und -Manifeste, `settings.json`, die Nutzerdaten der Preset-Bibliothek, Sample- und Nutzer-Wavetable-Zuordnungen, Zucht-Sitzungen. Gelingt das Laden, liegen alle Parameter in ihrem Bereich und der Kern erzeugt endliche Werte. |
| `stress: every sample rate and buffer size …` | 44,1 / 48 / 88,2 / 96 / 192 kHz, Blöcke mit 1, 7, 333, 4096 Samples und wechselnde Blöcke (auch leere), alle acht Ausgangspaare: endlich, begrenzt, hörbar, keine Denormals am Ausgang. |
| `stress: silence tails …` | 60 s Hall- / Delay-Ausklang nach den Noten: keine Denormals, die letzten Sekunden kosten nicht mehr als die ersten. |
| `stress: NaN / Inf at the input …` | NaN- / Inf- / riesige Eingangswerte erreichen weder die Ausgänge noch das Band; danach spielt die Engine normal. |
| `stress: random patches …` | Jeder Parameter irgendwo in seinem Bereich: kein NaN / Inf an den Ausgängen. |
| `stress: soak …` | 10 Minuten Audio offline gerechnet, mit Sequenzern, Automation und einem Preset-Wechsel pro Minute: endlich, begrenzt, keine Denormals, der Ausgangsschutz musste nie eingreifen, danach klingt keine Stimme mehr. |
| `robustness: damaged host states …` (Plug-in-Tests) | `setStateInformation` mit abgeschnittenen, beschädigten, übergroßen und typfalschen Daten wirft nie; das Plug-in rechnet weiter. |

Die Mutationen haben einen festen Startwert und sind reproduzierbar. Lokal:

```
OHPE_FUZZ_ITERATIONS=20000 OHPE_FUZZ_SEED=42 build-core/tests/core/ohpe_core_tests -tc="robustness:*"
OHPE_SOAK_MINUTES=480 build-core/tests/core/ohpe_core_tests -tc="stress: soak*"      # 8 Stunden Audio, offline
```

Schutz in der Engine: Jeder Ausgangsblock wird geprüft; ein NaN / Inf schaltet diesen Block stumm und setzt
Klangerzeuger und Effekte zurück (wird gezählt, sollte nie vorkommen). Der Audioeingang wird bereinigt (NaN / Inf → 0,
begrenzt), bevor ihn etwas verwendet. JSON-Dokumente mit mehr als 256 Verschachtelungsebenen werden vor dem Einlesen
abgelehnt.

## Manueller 8-Stunden-Dauertest (vor einem Release)

Auf jeder ausgelieferten Plattform (je ein Rechner genügt), mit dem Release-Build.

1. **Vorbereiten.** Frischer Nutzerordner oder den aktuellen notieren. Andere Audioprogramme schließen. Audiogerät auf
   48 kHz, Puffer 128 (Audio-Einstellungen der Standalone-App). Uhrzeit, Version und Rechner notieren.
2. **Standalone, Klang.** Werksprojekt *Ambient Drift* laden, PLAY starten. Step-Sequenzer auf Part 1 und Arpeggiator
   auf Part 2 mit gehaltenem Akkord (Latch) einschalten. 30 s auf Bandspur 1 aufnehmen und als Schleife abspielen.
3. **Automation.** Einen Drehregler auf eine Filter-Grenzfrequenz legen und eine langsame Spur des Bewegungsrekorders
   darauf laufen lassen (Bewegungs-Ansicht), damit sich ein Parameter die ganze Zeit bewegt.
4. **8 Stunden laufen lassen.** Alle 2 Stunden, ohne die Wiedergabe anzuhalten: CPU-Anzeige notieren (Mittel und
   langsamster Block aus dem Tooltip), Speicher der App (Aktivitätsanzeige / Task-Manager / `top`), 30 s hören (kein
   Knacken, keine Aussetzer, keine Stille, kein zunehmendes Rauschen). Einmal das Preset wechseln (Bibliothek,
   beliebiges Projekt) und zurück.
5. **Abtastrate im Betrieb wechseln.** Einmal das Gerät auf 44,1 kHz und zurück auf 48 kHz stellen, den Puffer auf 64
   und zurück. Die Wiedergabe muss sauber weiterlaufen.
6. **Normal beenden.** Neu starten: Ohpe öffnet die Sitzung ohne Hinweis. Dann die App zwangsweise beenden (Prozess
   beenden) und neu starten: Der Hinweis *Die letzte Sitzung wurde nicht richtig beendet* erscheint; WIEDERHERSTELLEN
   holt die Sitzung zurück.
7. **Plug-in.** Schritte 2–4 für 2 Stunden in einem Plug-in-Host mit dem VST3 (unter macOS auch AU) wiederholen,
   einschließlich einmal das Host-Projekt speichern und wieder öffnen.

**Bestanden:** kein Absturz oder Hänger, kein hörbares Artefakt, CPU-Mittel innerhalb ±20 % der ersten Messung, Speicher
wächst nach der ersten Stunde um weniger als 50 MB, der Hinweis verhält sich wie beschrieben. Messwerte und Ergebnis ins
Release-Issue schreiben (Datum, Version, Plattform, Gerät, Puffer, Messwerte alle 2 Stunden).
