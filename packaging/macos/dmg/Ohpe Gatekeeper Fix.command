#!/bin/bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Ohpe Gatekeeper Fix - double-click in Finder (or run in Terminal).
#
# For installed copies of Ohpe that macOS refuses to open ("damaged", "unidentified developer",
# plug-in not showing up in the host). For each Ohpe bundle found in the known install folders it
#   - removes the quarantine attribute (xattr -dr com.apple.quarantine),
#   - ad-hoc signs it (codesign --force --deep -s -) unless it already has a valid Developer ID
#     signature,
# then restarts the Audio Unit registry. Asks for your administrator password (sudo).
# Only touches these exact paths, and only bundles whose identifier starts with com.groovex.ohpe:
#   /Applications/Ohpe.app
#   /Library/Audio/Plug-Ins/VST3/Ohpe.vst3
#   /Library/Audio/Plug-Ins/Components/Ohpe.component
#   ~/Applications/Ohpe.app, ~/Library/Audio/Plug-Ins/VST3/Ohpe.vst3,
#   ~/Library/Audio/Plug-Ins/Components/Ohpe.component
# Safe to run any number of times.
#
# Options: --yes   do not wait for Return at the start / end (for scripted use)

set -u
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

ASSUME_YES=0
for a in "$@"; do
    case "$a" in
        --yes|-y) ASSUME_YES=1 ;;
    esac
done

# ------------------------------------------------------------------ language
LANG_DE=0
langs="$(defaults read -g AppleLanguages 2>/dev/null | tr -d ' \n"()' || true)"
if [ -n "${SUDO_USER:-}" ]; then
    ul="$(sudo -u "$SUDO_USER" defaults read -g AppleLanguages 2>/dev/null | tr -d ' \n"()' || true)"
    [ -n "$ul" ] && langs="$ul"
fi
case "${langs%%,*}" in de*) LANG_DE=1 ;; esac
[ -z "$langs" ] && case "${LANG:-}" in de*) LANG_DE=1 ;; esac

msg() { # msg "english" "deutsch"
    if [ "$LANG_DE" = 1 ]; then printf '%s\n' "$2"; else printf '%s\n' "$1"; fi
}

finish() {
    echo
    if [ "$ASSUME_YES" = 0 ] && [ -t 0 ]; then
        msg "Press Return to close this window." "Zum Schließen des Fensters Return drücken."
        read -r _ || true
    fi
    exit "${1:-0}"
}

# ------------------------------------------------------------------ admin rights
if [ "$(id -u)" != 0 ]; then
    echo
    msg "Ohpe Gatekeeper Fix" "Ohpe Gatekeeper-Fix"
    msg "This makes installed Ohpe copies (app, VST3, AU) openable on this Mac." \
        "Damit lassen sich installierte Ohpe-Kopien (App, VST3, AU) auf diesem Mac öffnen."
    msg "Please enter your administrator password (it is not shown while typing)." \
        "Bitte gib dein Administrator-Passwort ein (es wird beim Tippen nicht angezeigt)."
    exec sudo /bin/bash "$0" "$@"
fi

# ------------------------------------------------------------------ home of the real user
USER_HOME=""
if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
    USER_HOME="$(dscl . -read "/Users/$SUDO_USER" NFSHomeDirectory 2>/dev/null | awk '{print $2}')"
fi

CANDIDATES=(
    "/Applications/Ohpe.app"
    "/Library/Audio/Plug-Ins/VST3/Ohpe.vst3"
    "/Library/Audio/Plug-Ins/Components/Ohpe.component"
)
if [ -n "$USER_HOME" ] && [ -d "$USER_HOME" ]; then
    CANDIDATES+=(
        "$USER_HOME/Applications/Ohpe.app"
        "$USER_HOME/Library/Audio/Plug-Ins/VST3/Ohpe.vst3"
        "$USER_HOME/Library/Audio/Plug-Ins/Components/Ohpe.component"
    )
fi

is_ohpe_bundle() { # a real directory (no symlink) with an Ohpe bundle identifier
    local b="$1" id
    [ -d "$b" ] && [ ! -L "$b" ] || return 1
    case "$b" in
        */Ohpe.app|*/Ohpe.vst3|*/Ohpe.component) ;;
        *) return 1 ;;
    esac
    id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$b/Contents/Info.plist" 2>/dev/null || true)"
    case "$id" in com.groovex.ohpe*) return 0 ;; esac
    return 1
}

echo
found=0
failed=0
for b in "${CANDIDATES[@]}"; do
    is_ohpe_bundle "$b" || continue
    found=$((found + 1))
    msg "-> $b" "-> $b"
    xattr -dr com.apple.quarantine "$b" 2>/dev/null || true
    msg "   quarantine attribute removed" "   Quarantäne-Attribut entfernt"
    if codesign --verify --strict "$b" >/dev/null 2>&1 \
       && codesign -dv "$b" 2>&1 | grep -q '^Authority=Developer ID Application'; then
        msg "   Developer ID signature found - kept" "   Developer-ID-Signatur vorhanden - beibehalten"
    elif codesign --force --deep -s - "$b" >/dev/null 2>&1; then
        msg "   ad-hoc signed" "   ad-hoc signiert"
    else
        msg "   WARNING: signing failed" "   WARNUNG: Signieren fehlgeschlagen"
        failed=$((failed + 1))
    fi
done

if [ "$found" = 0 ]; then
    msg "No installed Ohpe found. Install Ohpe first (Ohpe-Installer.pkg), then run this again." \
        "Kein installiertes Ohpe gefunden. Erst Ohpe installieren (Ohpe-Installer.pkg), dann erneut ausführen."
    finish 0
fi

killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
msg "Audio Unit registry restarted." "Audio-Unit-Verzeichnis neu gestartet."
echo
if [ "$failed" = 0 ]; then
    msg "Done. Restart your music software so it rescans its plug-ins." \
        "Fertig. Starte deine Musiksoftware neu, damit sie die Plug-ins neu einliest."
    finish 0
fi
msg "Finished with $failed warning(s) - see above." "Mit $failed Warnung(en) beendet - siehe oben."
finish 1
