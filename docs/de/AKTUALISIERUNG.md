# Updates direkt in Ohpe

Ohpe kann eine neue Version selbst finden, laden und öffnen – in der Standalone-App und im Plug-in. Werks-Inhalte
(Preset- und Sample-Pakete) kommen auf demselben Weg dazu, ohne Neuinstallation.

![Die Update-Ansicht](../images/ohpe-update-de.png)

## Die Update-Ansicht öffnen

Ein Klick auf die **Versionsbeschriftung** über dem Schriftzug (z. B. `V0.7.0`) oder auf den Schriftzug selbst. Gibt es
eine neuere Version, zeigt die Beschriftung einen kleinen **gelben Leuchtbalken** und `UPDATE 0.8.0`. **Esc** oder
**SCHLIESSEN** schließt die Ansicht; die Bibliothek, die Pro-Ansicht oder die Zucht schließen sie ebenfalls.

## Was sie zeigt

| Bereich | Inhalt |
|---|---|
| Oberste Zeile | installierte Version, das letzte Ergebnis (gelb = etwas zu tun, rot = ein Fehler) |
| Links | die angebotene Version: Nummer, Kanal, Datum und ihre **Release-Hinweise in der UI-Sprache** (mit dem Mausrad blättern) |
| **JETZT PRÜFEN** | sucht sofort nach einer neuen Version |
| **UPDATE AUF x.y.z** | lädt den Installer für dein System im Hintergrund; ein **violetter** Balken zeigt den Fortschritt, **ABBRECHEN** hält an |
| **INSTALLIEREN** | nach dem Download: öffnet den geprüften Installer (macOS-Installer-Image; Windows-Archive werden im Explorer gezeigt) |
| **DIESE VERSION ÜBERSPRINGEN** | diese Version wird nicht mehr angeboten – eine neuere schon |
| **SPÄTER ERINNERN** | die automatische Prüfung schweigt drei Tage zu dieser Version |
| Einstellungen | **Automatisch prüfen (einmal am Tag)** – aus, bis du es einschaltest; **Kanal** Stabil / Vorschau; zuletzt geprüft |
| Inhaltspakete | Werks-Preset- / Sample-Pakete des Releases: **INSTALLIEREN**, **UPDATE**, **ZURÜCK** |
| **VORIGE VERSION** | lädt das neueste Release, das älter ist als deines (Zurücksetzen) – Installation wie oben |

## So funktioniert es

* **Wo Ohpe sucht.** Ohpe liest die öffentliche Release-Liste des Projekts auf GitHub. Kein Konto, keine Telemetrie:
  nichts über dich, dein System oder deine Musik wird gesendet. Ohne Verbindung sagt die Ansicht das; sonst passiert nichts.
* **Wann.** Nur wenn du JETZT PRÜFEN drückst – oder, wenn du es einschaltest, automatisch **höchstens einmal am Tag**,
  solange ein Ohpe-Fenster offen ist, und **nie, während Transport oder Band laufen**.
* **Kanäle.** *Stabil* bietet nur Releases an, *Vorschau* auch Vorab-Versionen. Versionen vor 1.0 starten mit Vorschau.
* **Download.** Der Download läuft in einem eigenen Thread. Eine abgebrochene Verbindung wird mit wachsenden Pausen
  erneut versucht, und der Download **macht dort weiter, wo er stoppte** (auch nach einem Neustart von Ohpe).
* **Prüfung.** Jede Datei wird gegen die mit dem Release veröffentlichte **SHA-256-Prüfsumme** (und ihre Größe) geprüft.
  Eine Datei, die die Prüfung nicht besteht, wird **gelöscht und nie geöffnet**. Direkt vor INSTALLIEREN wird erneut
  geprüft. Prüfungen von Code-Signatur / Notarisierung folgen, sobald die Installer signiert sind.
* **Installieren.** Die Standalone-App bietet an, sich zu beenden, damit der Installer das Programm ersetzen kann. Im
  Plug-in-Host bittet Ohpe, das Projekt zu speichern und den Host zu beenden, bevor der Installer weitermacht.

## Inhaltspakete

Ein Release kann Inhaltspakete anbieten (Werks-Presets, Samples). INSTALLIEREN lädt das Paket, prüft es und entpackt es
in einen eigenen Ordner; seine Presets erscheinen in der Bibliothek neben der Werksbank (nur lesbar). Eine neuere Version
behält die vorige als Sicherung – **ZURÜCK** holt sie zurück (ein Schritt). Jeder Pfad im Paket muss im Paketordner
bleiben; ein Paket, das woanders hinschreiben will, wird als Ganzes abgelehnt.

**Deine Presets, eigenen Wavetables, Bounces und Einstellungen bleiben unberührt** – sie liegen in anderen Ordnern.

Pakete, die du als Datei hast (`.ohpepack`), installierst du mit **PAKET INSTALLIEREN …** oder indem du sie auf das
Fenster ziehst, und verwaltest sie (Version, Größe, Entfernen) unter **Einstellungen › Inhaltspakete** – siehe
[Inhaltspakete](INHALTSPAKETE.md).

## Dateien

| Was | Wo (im Benutzerdaten-Ordner `groovex/Ohpe`) |
|---|---|
| Update-Einstellungen (pro Benutzer, nicht pro Projekt) | `Settings/update.json` |
| Downloads (Teil-Dateien enden auf `.part`) | `Updates/` |
| Inhaltspakete, ihr Verzeichnis und Sicherungen | `Content/<Paket>/`, `Content/installed.json`, `Content/.backup/` |

Der Benutzerdaten-Ordner ist `~/Library/Application Support/groovex/Ohpe` auf macOS, `%APPDATA%\groovex\Ohpe` auf
Windows und `~/.config/groovex/Ohpe` auf Linux. Die Umgebungsvariable `OHPE_DATA_DIR` verlegt ihn (portable
Installationen, Tests).

Der Updater ist kein Klangregler: Er hat keine Host-Parameter und keine MIDI-Zuordnung.

## Für Maintainer: ein Update veröffentlichen

Der Workflow `release-assets` (mit Release-Tag und CI-Run-ID starten) hängt die Installer an und schreibt
`ohpe-update.json` mit `tools/update/make_manifest.py`: jedes Asset mit Plattform, Art, Größe und SHA-256, den
Release-Text getrennt an seinen Überschriften `## English` / `## Deutsch`, und die Inhaltspakete aus
`packaging/content/<id>/` (eine `pack.json` mit `id`, `version`, `kind`, `name.en`, `name.de`, `minApp` und ein Ordner
`files/`). Releases ohne Manifest funktionieren weiter: Ohpe liest ihren Text und `SHA256SUMS.txt`.

Siehe auch: [Installation](INSTALLATION.md) · [Bedienung](BEDIENUNG.md) · [Presets](PRESETS.md).
