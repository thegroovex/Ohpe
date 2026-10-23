# Ohpe installieren

*Deutsch · English: [../en/INSTALL.md](../en/INSTALL.md)*

Ohpe läuft auf macOS und Windows; Windows steht in [Abschnitt 5](#5-windows). Ohpe für macOS kommt als ein Disk-Image, `Ohpe-<Version>-macOS-Installer.dmg`. Es läuft ab
macOS 11 auf Macs mit Apple Silicon und Intel (alle Programmdateien sind Universal-Binaries).

| Datei auf dem Disk-Image        | Wozu |
|---------------------------------|------|
| `Ohpe-Installer.pkg`            | Installer mit drei einzeln wählbaren Teilen |
| `Ohpe Gatekeeper Fix.command`   | Macht ein installiertes Ohpe startbar, falls macOS es noch blockiert |
| `Uninstall Ohpe.command`        | Entfernt Ohpe wieder; deine eigenen Presets bleiben, wenn du nichts anderes sagst |
| `Read me first.txt` / `Bitte zuerst lesen.txt` | Die Kurzfassung dieser Seite |
| `LICENSE.txt`                   | GNU Affero General Public License v3.0 oder neuer |

## Wo es den Installer gibt

Die neueste Version gibt es auf der [Release-Seite](https://github.com/thegroovex/Ohpe/releases):
`Ohpe-<Version>-macOS-Installer.dmg` für den Mac, `Ohpe-<Version>-Windows-app.zip` und
`Ohpe-<Version>-Windows-VST3.zip` für Windows, dazu `SHA256SUMS.txt` zum Prüfen. Ist Ohpe installiert, findet
und lädt es neue Versionen selbst ([AKTUALISIERUNG.md](AKTUALISIERUNG.md)).

Builds zwischen den Releases: jeder Push auf main, jeder manuelle Lauf des
**CI**-Workflows und jeder Pull Request mit dem Label `installer` baut den Installer auf dem macOS-Runner und hängt ihn als Artefakt
**Ohpe-macOS-Installer** an den Lauf: im Repository den Reiter *Actions* → *CI* → ein grüner
Lauf → *Artifacts*. GitHub liefert Artefakte als Zip; nach dem Entpacken liegt die `.dmg` (und
`SHA256SUMS.txt`) vor.

## 1. Installer öffnen

Der Installer ist nicht mit einer Apple-Developer-ID signiert. Beim ersten Doppelklick meldet
macOS deshalb, er *kann nicht geöffnet werden* bzw. stamme von einem *nicht verifizierten
Entwickler*. Nutze einen dieser Wege – jeder betrifft nur diese eine Datei:

1. **Dennoch öffnen** (jede macOS-Version; ab macOS 15 der einzige Weg aus dem Finder):
   1. `Ohpe-Installer.pkg` doppelklicken. macOS blockiert sie – **Fertig** (bzw. **OK**) klicken.
   2. **Systemeinstellungen → Datenschutz & Sicherheit** öffnen und nach unten zu **Sicherheit**
      scrollen. Dort steht, dass `Ohpe-Installer.pkg` blockiert wurde.
   3. **Dennoch öffnen** klicken, mit Passwort oder Touch ID bestätigen und im Dialog nochmals
      **Dennoch öffnen** (bzw. **Öffnen**) klicken. Der Installer startet.
2. **Rechtsklick → Öffnen** (macOS 14 und älter): im Finder mit rechter Maustaste (oder Ctrl-Klick)
   auf `Ohpe-Installer.pkg`, **Öffnen** wählen und im Dialog nochmals **Öffnen**.
3. **Terminal:** die pkg aus dem Disk-Image kopieren (z. B. nach Downloads) und das
   Quarantäne-Attribut entfernen:
   ```sh
   xattr -d com.apple.quarantine ~/Downloads/Ohpe-Installer.pkg
   ```
   Danach wie gewohnt doppelklicken.

## 2. Teile auswählen

Im Installer auf **Anpassen** klicken, um Teile an- oder abzuwählen:

| Teil            | Wird installiert nach |
|-----------------|-----------------------|
| Standalone-App  | `/Programme/Ohpe.app` (`/Applications`) |
| VST3-Plug-in    | `/Library/Audio/Plug-Ins/VST3/Ohpe.vst3` |
| Audio Unit      | `/Library/Audio/Plug-Ins/Components/Ohpe.component` |

Willkommens-, Lizenz- und Hinweisseite erscheinen je nach Systemsprache auf Deutsch oder
Englisch. Beende deine Musiksoftware vor der Installation und starte sie danach neu, damit sie
die Plug-ins neu einliest.

## 3. So funktioniert der Gatekeeper-Fix

macOS blockiert heruntergeladene Software, die nicht von Apple notarisiert ist, und Macs mit
Apple Silicon führen Code ohne jede Signatur gar nicht aus. Jeder Teil des Installers startet
deshalb direkt nach dem Kopieren ein kleines `postinstall`-Skript:

1. `xattr -dr com.apple.quarantine <Bundle>` – entfernt die Markierung „aus dem Internet geladen“.
2. `codesign --force --deep -s - <Bundle>` – eine *Ad-hoc*-Signatur, erstellt auf deinem Mac. Sie
   sagt nichts über den Urheber aus, erfüllt aber die Regel „Code muss signiert sein“ von Apple
   Silicon. Trägt das Bundle bereits eine gültige Developer-ID-Signatur (signierter Build), wird
   dieser Schritt übersprungen und die echte Signatur bleibt erhalten.
3. Bei der Audio Unit: `killall -9 AudioComponentRegistrar` – startet das Audio-Unit-Verzeichnis
   neu, damit Hosts die neue Version ohne Abmelden sehen (Fehler werden ignoriert).

Das Skript bricht die Installation nie ab; Probleme landen im Installer-Protokoll
(`/var/log/install.log`).

### Falls macOS Ohpe trotzdem blockiert

**`Ohpe Gatekeeper Fix.command`** auf dem Disk-Image doppelklicken. Das Skript fragt nach deinem
Administrator-Passwort (sudo) und wiederholt die Schritte 1–3 für jede gefundene Ohpe-Kopie –
ausschließlich für diese Pfade und nur für Bundles, deren Kennung mit `com.groovex.ohpe` beginnt:

* `/Applications/Ohpe.app`, `/Library/Audio/Plug-Ins/VST3/Ohpe.vst3`,
  `/Library/Audio/Plug-Ins/Components/Ohpe.component`
* dieselben drei in deinem Benutzerordner (`~/Applications`, `~/Library/Audio/Plug-Ins/…`)

Symbolischen Links wird nie gefolgt. Mehrfaches Ausführen schadet nicht. Die Meldungen erscheinen
je nach Systemsprache auf Deutsch oder Englisch. Blockiert macOS auch die `.command`-Datei:
Rechtsklick → **Öffnen**, oder im Terminal ausführen:

```sh
bash "/Volumes/Ohpe/Ohpe Gatekeeper Fix.command"
```

## 4. Deinstallieren

**`Uninstall Ohpe.command`** doppelklicken. Nach Bestätigung und Eingabe des
Administrator-Passworts entfernt es App, VST3 und Audio Unit (gleiche Pfade und Prüfungen wie beim
Gatekeeper-Fix), löscht die Installationsbelege (`pkgutil --forget com.groovex.ohpe.app`,
`….vst3`, `….au`) und startet das Audio-Unit-Verzeichnis neu.

Deine eigenen Presets (`~/Library/Application Support/groovex/Ohpe`) und die App-Einstellungen
(`~/Library/Application Support/Ohpe.settings`) **bleiben erhalten**; das Skript fragt, ob du sie
ebenfalls löschen willst (Vorgabe: nein). Im Terminal:

```sh
sudo bash "/Volumes/Ohpe/Uninstall Ohpe.command" --yes           # ohne Rückfragen, Presets bleiben
sudo bash "/Volumes/Ohpe/Uninstall Ohpe.command" --yes --purge   # löscht auch Presets/Einstellungen
```

## 5. Windows

Ohpe für Windows (Windows 10 oder 11, 64 Bit) kommt als zwei Zip-Dateien: `Ohpe-<Version>-Windows-app.zip`
enthält die Standalone-App `Ohpe.exe`, `Ohpe-<Version>-Windows-VST3.zip` den Plug-in-Ordner `Ohpe.vst3`.
Die Builds sind nicht mit einem Code-Signing-Zertifikat signiert, deshalb warnt Windows SmartScreen einmal.

1. **Download freigeben:** Rechtsklick auf die Zip → **Eigenschaften** → unten **Zulassen** ankreuzen →
   **OK**. Dann Rechtsklick → **Alle extrahieren…**.
2. **App:** `Ohpe.exe` an einen beliebigen Ort legen (z. B. `C:\Program Files\Ohpe\`) und doppelklicken.
3. **SmartScreen:** meldet Windows *Der Computer wurde durch Windows geschützt*, auf **Weitere
   Informationen** klicken, prüfen, dass die App `Ohpe.exe` heißt, dann **Trotzdem ausführen**. Windows merkt
   sich das; für diese Datei kommt die Warnung nicht wieder.
4. **VST3-Plug-in:** den ganzen Ordner `Ohpe.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren
   (Windows fragt nach Administratorrechten), dann die Musiksoftware ihre Plug-ins neu suchen lassen.

**Deinstallieren:** `Ohpe.exe` und den Ordner `Ohpe.vst3` löschen. Eigene Presets, Tabellen und Einstellungen
bleiben in `%APPDATA%\groovex\Ohpe`; diesen Ordner ebenfalls löschen, wenn sie weg sollen.

## Für Maintainer

### Installer bauen

Auf einem Mac nach einem universellen Release-Build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
packaging/macos/build-installer.sh            # -> dist/Ohpe-Installer.pkg, dist/Ohpe-<v>-macOS-Installer.dmg
```

Die Version stammt aus `project(Ohpe VERSION …)` in `CMakeLists.txt` (gelesen aus
`build/CMakeCache.txt`). Die Quellen des Installers liegen in `packaging/macos/`:

| Pfad | Zweck |
|------|-------|
| `build-installer.sh` | Komponenten-pkgs (pkgbuild) → Produktarchiv (productbuild) → DMG (hdiutil) |
| `distribution.xml.in` | Auswahl app / vst3 / au, macOS 11+, arm64 + x86_64 ohne Rosetta |
| `resources/{en,de}.lproj/` | Willkommen, Hinweise, Lizenz-Vorspann, Titel der Auswahl (`Localizable.strings`) |
| `scripts/postinstall.in` | Gatekeeper-Fix, ausgeführt von jeder Komponente |
| `dmg/` | Gatekeeper-Fix- und Deinstallations-Skript, Hinweisdateien für das Disk-Image |
| `validate-installer.sh` | End-to-End-Test (nur CI: installiert wirklich nach `/`) |
| `entitlements.plist` | Hardened-Runtime-Berechtigungen für die Developer-ID-Signatur |

Die Bundles sind als nicht verschiebbar markiert: Der Installer schreibt immer in die festen
Ordner, auch wenn anderswo auf der Platte schon eine Ohpe-Kopie liegt.

### Was CI prüft

Der macOS-Job des CI-Workflows (Pull Requests und manuelle Läufe) baut den Installer und startet
`validate-installer.sh`: Aufbau der pkg (`pkgutil --expand`, `--payload-files`, DE/EN-Ressourcen,
Auswahl), Inhalt des DMG, eine vollständige Installation `installer -pkg … -target /`,
Dateien/Belege/Universal-Binaries/Signaturen/keine Quarantäne, `auval -strict` auf der
installierten AU, den Gatekeeper-Fix auf Bundles mit Quarantäne (zweimal), die Deinstallation
(Presets bleiben), eine reine AU-Installation per `-applyChoiceChangesXML` und `--purge`. Das DMG
wird als Artefakt **Ohpe-macOS-Installer** hochgeladen.

### Signieren und Notarisieren (optional)

Ohne Secrets ist der Installer unsigniert und verlässt sich auf den Ad-hoc-Fix oben. Mit einem
Apple-Developer-Konto diese Repository-Secrets anlegen (*Settings → Secrets and variables →
Actions*); jeder Schritt läuft nur, wenn seine Secrets vorhanden sind:

| Secret | Inhalt |
|--------|--------|
| `MACOS_CERTIFICATES_P12` | Base64 eines `.p12`-Exports mit den Zertifikaten *Developer ID Application* und *Developer ID Installer* samt privaten Schlüsseln (`base64 -i certs.p12 \| pbcopy`) |
| `MACOS_CERTIFICATES_PASSWORD` | Passwort der `.p12` |
| `MACOS_SIGN_APP_IDENTITY` | z. B. `Developer ID Application: Dein Name (TEAMID)` – signiert App, Plug-ins (Hardened Runtime, `entitlements.plist`) und DMG |
| `MACOS_SIGN_INSTALLER_IDENTITY` | z. B. `Developer ID Installer: Dein Name (TEAMID)` – signiert die pkg |
| `MACOS_NOTARY_APPLE_ID` | Apple-ID (E-Mail) für die Notarisierung |
| `MACOS_NOTARY_TEAM_ID` | zehnstellige Team-ID |
| `MACOS_NOTARY_PASSWORD` | app-spezifisches Passwort dieser Apple-ID |

Sind alle gesetzt, importiert CI die Zertifikate in einen temporären Schlüsselbund, signiert
Bundles, pkg und DMG, reicht pkg und DMG mit `xcrun notarytool submit --wait` ein und heftet die
Tickets an (`xcrun stapler staple`). Ohne beide Identitäten wird die Notarisierung mit Hinweis
übersprungen. Ein signiertes Bundle behält seine Developer-ID-Signatur bei der Installation – das
postinstall-Skript signiert nur Bundles ohne eine solche ad hoc. Lokal funktioniert dasselbe mit
den Umgebungsvariablen `OHPE_SIGN_APP_IDENTITY`, `OHPE_SIGN_INSTALLER_IDENTITY`, `OHPE_KEYCHAIN`,
`OHPE_NOTARY_APPLE_ID`, `OHPE_NOTARY_TEAM_ID` und `OHPE_NOTARY_PASSWORD`. Pull Requests aus Forks
erhalten keine Secrets und bauen daher immer die unsignierte Variante.
