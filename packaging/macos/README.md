# macOS installer sources

*English · Deutsch: [docs/de/INSTALLATION.md](../../docs/de/INSTALLATION.md#für-maintainer)*

`build-installer.sh` turns a universal Release build into `Ohpe-Installer.pkg` and
`Ohpe-<version>-macOS-Installer.dmg`; `validate-installer.sh` is the end-to-end test CI runs.
Everything else here is input for those two scripts. Full description, including optional Developer
ID signing and notarisation: [docs/en/INSTALL.md](../../docs/en/INSTALL.md#for-maintainers).
