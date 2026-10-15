# Installing Ohpe on macOS

*English · Deutsch: [../de/INSTALLATION.md](../de/INSTALLATION.md)*

Ohpe for macOS comes as one disk image, `Ohpe-<version>-macOS-Installer.dmg`. It runs on
macOS 11 or newer, on Apple Silicon and Intel Macs (all binaries are universal).

| File on the disk image          | What it does |
|---------------------------------|--------------|
| `Ohpe-Installer.pkg`            | Installer with three parts you can pick individually |
| `Ohpe Gatekeeper Fix.command`   | Makes an installed Ohpe openable if macOS still blocks it |
| `Uninstall Ohpe.command`        | Removes Ohpe again; keeps your own presets unless you say otherwise |
| `Read me first.txt` / `Bitte zuerst lesen.txt` | The short version of this page |
| `LICENSE.txt`                   | GNU Affero General Public License v3.0 or later |

## Where to get it

There are no tagged releases yet. Every pull request and every manual run of the **CI** workflow
builds the installer on the macOS runner and attaches it to the run as the artifact
**Ohpe-macOS-Installer**: open the repository's *Actions* tab → *CI* → a green run → *Artifacts*.
GitHub delivers artifacts as a zip; unzip it to get the `.dmg` (and `SHA256SUMS.txt`).

## 1. Open the installer

The installer is not signed with an Apple Developer ID, so on the first double-click macOS says
it *cannot be opened* or comes *from an unidentified developer*. Use one of these ways – each only
affects this one file:

1. **Right-click → Open:** in Finder, right-click (or Control-click) `Ohpe-Installer.pkg`, choose
   **Open**, then **Open** again in the dialog.
2. **Open Anyway:** after the first blocked attempt open **System Settings → Privacy & Security**,
   scroll to the message about `Ohpe-Installer.pkg` and click **Open Anyway**.
3. **Terminal:** copy the pkg out of the disk image (for example into Downloads) and remove the
   quarantine flag:
   ```sh
   xattr -d com.apple.quarantine ~/Downloads/Ohpe-Installer.pkg
   ```
   Then double-click it as usual.

## 2. Choose what to install

Click **Customize** in the installer to switch parts on or off:

| Part            | Installed to |
|-----------------|--------------|
| Standalone app  | `/Applications/Ohpe.app` |
| VST3 plug-in    | `/Library/Audio/Plug-Ins/VST3/Ohpe.vst3` |
| Audio Unit      | `/Library/Audio/Plug-Ins/Components/Ohpe.component` |

The welcome, licence and read-me pages appear in English or German depending on your system
language. Quit your music software before installing and restart it afterwards so it rescans its
plug-ins.

## 3. How the Gatekeeper fix works

macOS blocks software that is downloaded and not notarised by Apple, and Apple Silicon Macs refuse
to run code that carries no signature at all. Every part of the installer therefore runs a small
`postinstall` script right after copying its bundle:

1. `xattr -dr com.apple.quarantine <bundle>` – removes the "downloaded from the internet" flag.
2. `codesign --force --deep -s - <bundle>` – an *ad-hoc* signature made on your Mac. It proves
   nothing about the author, but it satisfies Apple Silicon's "code must be signed" rule. If the
   bundle already carries a valid Developer ID signature (a signed build), this step is skipped so
   the real signature is kept.
3. For the Audio Unit: `killall -9 AudioComponentRegistrar` – restarts the Audio Unit registry so
   hosts see the new version without logging out (errors are ignored).

The script never stops the installation; problems are written to the installer log
(`/var/log/install.log`).

### If macOS still blocks Ohpe

Double-click **`Ohpe Gatekeeper Fix.command`** on the disk image. It asks for your administrator
password (sudo) and repeats steps 1–3 for every Ohpe copy it finds – and only for these exact
paths, and only bundles whose identifier starts with `com.groovex.ohpe`:

* `/Applications/Ohpe.app`, `/Library/Audio/Plug-Ins/VST3/Ohpe.vst3`,
  `/Library/Audio/Plug-Ins/Components/Ohpe.component`
* the same three below your home folder (`~/Applications`, `~/Library/Audio/Plug-Ins/…`)

Symbolic links are never followed. Running it again does no harm. Messages are in English or
German depending on your system language. If macOS also blocks the `.command` file itself,
right-click it → **Open**, or run it in Terminal:

```sh
bash "/Volumes/Ohpe/Ohpe Gatekeeper Fix.command"
```

## 4. Uninstall

Double-click **`Uninstall Ohpe.command`**. After confirming and entering your administrator
password it removes the app, the VST3, the Audio Unit (same paths and checks as the Gatekeeper
fix), forgets the installer receipts (`pkgutil --forget com.groovex.ohpe.app`, `….vst3`, `….au`)
and restarts the Audio Unit registry.

Your own presets (`~/Library/Application Support/groovex/Ohpe`) and the app settings
(`~/Library/Application Support/Ohpe.settings`) are **kept**; the script asks whether you want to
delete them as well (default: no). In Terminal:

```sh
sudo bash "/Volumes/Ohpe/Uninstall Ohpe.command" --yes           # no questions, keeps presets
sudo bash "/Volumes/Ohpe/Uninstall Ohpe.command" --yes --purge   # also deletes presets/settings
```

## For maintainers

### Building the installer

On a Mac, after a universal Release build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
packaging/macos/build-installer.sh            # -> dist/Ohpe-Installer.pkg, dist/Ohpe-<v>-macOS-Installer.dmg
```

The version comes from `project(Ohpe VERSION …)` in `CMakeLists.txt` (read from
`build/CMakeCache.txt`). Sources of the installer live in `packaging/macos/`:

| Path | Purpose |
|------|---------|
| `build-installer.sh` | component pkgs (pkgbuild) → product archive (productbuild) → DMG (hdiutil) |
| `distribution.xml.in` | choices app / vst3 / au, macOS 11+, arm64 + x86_64 without Rosetta |
| `resources/{en,de}.lproj/` | welcome, read-me, licence preamble, choice titles (`Localizable.strings`) |
| `scripts/postinstall.in` | Gatekeeper fix run by each component |
| `dmg/` | Gatekeeper fix and uninstall scripts, read-me files for the disk image |
| `validate-installer.sh` | end-to-end test (CI only: really installs into `/`) |
| `entitlements.plist` | hardened-runtime entitlements for Developer ID signing |

Component bundles are marked non-relocatable, so the installer always writes to the fixed folders
even if another copy of Ohpe exists elsewhere on the disk.

### What CI checks

The macOS job of the CI workflow (pull requests and manual runs) builds the installer and runs
`validate-installer.sh`: pkg structure (`pkgutil --expand`, `--payload-files`, EN/DE resources,
choices), DMG contents, a full `installer -pkg … -target /`, files/receipts/universal
binaries/signatures/no quarantine, `auval -strict` on the installed AU, the Gatekeeper fix on
quarantined bundles (twice), uninstall (presets kept), an AU-only install via
`-applyChoiceChangesXML` and `--purge`. The DMG is uploaded as artifact **Ohpe-macOS-Installer**.

### Signing and notarisation (optional)

Without secrets the installer is unsigned and relies on the ad-hoc fix above. With an Apple
Developer account, add these repository secrets (*Settings → Secrets and variables → Actions*);
each step runs only when its secrets are present:

| Secret | Content |
|--------|---------|
| `MACOS_CERTIFICATES_P12` | base64 of a `.p12` export holding *Developer ID Application* and *Developer ID Installer* certificates with their private keys (`base64 -i certs.p12 \| pbcopy`) |
| `MACOS_CERTIFICATES_PASSWORD` | password of the `.p12` |
| `MACOS_SIGN_APP_IDENTITY` | e.g. `Developer ID Application: Your Name (TEAMID)` – signs app, plug-ins (hardened runtime, `entitlements.plist`) and DMG |
| `MACOS_SIGN_INSTALLER_IDENTITY` | e.g. `Developer ID Installer: Your Name (TEAMID)` – signs the pkg |
| `MACOS_NOTARY_APPLE_ID` | Apple ID e-mail used for notarisation |
| `MACOS_NOTARY_TEAM_ID` | 10-character team ID |
| `MACOS_NOTARY_PASSWORD` | app-specific password for that Apple ID |

With all of them, CI imports the certificates into a temporary keychain, signs the bundles, the
pkg and the DMG, submits pkg and DMG with `xcrun notarytool submit --wait` and staples the tickets
(`xcrun stapler staple`). Notarisation is skipped (with a notice) unless both identities are set.
A signed bundle keeps its Developer ID signature during installation – the postinstall script only
ad-hoc signs bundles without one. Locally the same works with the environment variables
`OHPE_SIGN_APP_IDENTITY`, `OHPE_SIGN_INSTALLER_IDENTITY`, `OHPE_KEYCHAIN`, `OHPE_NOTARY_APPLE_ID`,
`OHPE_NOTARY_TEAM_ID` and `OHPE_NOTARY_PASSWORD`. Secrets are not available to pull requests from
forks, so those always build the unsigned variant.
