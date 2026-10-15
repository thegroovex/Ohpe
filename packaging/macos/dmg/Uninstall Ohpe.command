#!/bin/bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Uninstall Ohpe - double-click in Finder (or run in Terminal).
#
# Removes the Ohpe app and plug-ins and forgets the installer receipts (pkgutil --forget).
# Your own presets and settings are KEPT unless you agree to delete them (or pass --purge).
# Asks for your administrator password (sudo). Only touches these exact paths, and only bundles
# whose identifier starts with com.groovex.ohpe:
#   /Applications/Ohpe.app
#   /Library/Audio/Plug-Ins/VST3/Ohpe.vst3
#   /Library/Audio/Plug-Ins/Components/Ohpe.component
#   ~/Applications/Ohpe.app, ~/Library/Audio/Plug-Ins/VST3/Ohpe.vst3,
#   ~/Library/Audio/Plug-Ins/Components/Ohpe.component
# User data (only with --purge or after answering "y"):
#   ~/Library/Application Support/groovex/Ohpe      (presets)
#   ~/Library/Application Support/Ohpe.settings     (standalone app settings)
#
# Options:
#   --yes     no questions (keeps user data unless --purge is also given)
#   --purge   also delete your presets and settings

set -u
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

ASSUME_YES=0
PURGE=0
for a in "$@"; do
    case "$a" in
        --yes|-y) ASSUME_YES=1 ;;
        --purge)  PURGE=1 ;;
    esac
done

RECEIPTS=(com.groovex.ohpe.app com.groovex.ohpe.vst3 com.groovex.ohpe.au)

# ------------------------------------------------------------------ language
LANG_DE=0
langs="$(defaults read -g AppleLanguages 2>/dev/null | tr -d ' \n"()' || true)"
if [ -n "${SUDO_USER:-}" ]; then
    ul="$(sudo -u "$SUDO_USER" defaults read -g AppleLanguages 2>/dev/null | tr -d ' \n"()' || true)"
    [ -n "$ul" ] && langs="$ul"
fi
case "${langs%%,*}" in de*) LANG_DE=1 ;; esac
[ -z "$langs" ] && case "${LANG:-}" in de*) LANG_DE=1 ;; esac

msg() { if [ "$LANG_DE" = 1 ]; then printf '%s\n' "$2"; else printf '%s\n' "$1"; fi; }

interactive() { [ "$ASSUME_YES" = 0 ] && [ -t 0 ]; }

finish() {
    echo
    if interactive; then
        msg "Press Return to close this window." "Zum Schließen des Fensters Return drücken."
        read -r _ || true
    fi
    exit "${1:-0}"
}

ask_yes() { # ask_yes "english question" "deutsche Frage" -> 0 for yes (default: no)
    local answer
    interactive || return 1
    if [ "$LANG_DE" = 1 ]; then printf '%s [j/N] ' "$2"; else printf '%s [y/N] ' "$1"; fi
    read -r answer || return 1
    case "$answer" in [yYjJ]*) return 0 ;; esac
    return 1
}

# ------------------------------------------------------------------ admin rights
if [ "$(id -u)" != 0 ]; then
    echo
    msg "Uninstall Ohpe" "Ohpe deinstallieren"
    msg "This removes the Ohpe app, the VST3 and the Audio Unit from this Mac." \
        "Damit werden die Ohpe-App, das VST3 und die Audio Unit von diesem Mac entfernt."
    if [ "$ASSUME_YES" = 0 ] && [ -t 0 ]; then
        if ! ask_yes "Continue?" "Fortfahren?"; then
            msg "Cancelled - nothing was changed." "Abgebrochen - nichts wurde geändert."
            finish 0
        fi
    fi
    msg "Please enter your administrator password (it is not shown while typing)." \
        "Bitte gib dein Administrator-Passwort ein (es wird beim Tippen nicht angezeigt)."
    exec sudo /bin/bash "$0" --confirmed "$@"
fi

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

is_ohpe_bundle() {
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

# Was the confirmation already given before sudo re-ran the script?
CONFIRMED=0
for a in "$@"; do [ "$a" = "--confirmed" ] && CONFIRMED=1; done
if [ "$CONFIRMED" = 0 ] && interactive; then
    if ! ask_yes "Remove Ohpe from this Mac?" "Ohpe von diesem Mac entfernen?"; then
        msg "Cancelled - nothing was changed." "Abgebrochen - nichts wurde geändert."
        finish 0
    fi
fi

echo
removed=0
failed=0
for b in "${CANDIDATES[@]}"; do
    is_ohpe_bundle "$b" || continue
    if rm -rf "$b"; then
        msg "removed  $b" "entfernt $b"
        removed=$((removed + 1))
    else
        msg "WARNING: could not remove $b" "WARNUNG: konnte $b nicht entfernen"
        failed=$((failed + 1))
    fi
done
[ "$removed" = 0 ] && msg "No installed Ohpe app or plug-in found." "Keine installierte Ohpe-App und kein Plug-in gefunden."

for r in "${RECEIPTS[@]}"; do
    if pkgutil --pkg-info "$r" >/dev/null 2>&1; then
        pkgutil --forget "$r" >/dev/null 2>&1 && msg "receipt forgotten: $r" "Beleg entfernt: $r"
    fi
done

killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

# ------------------------------------------------------------------ user data
if [ -n "$USER_HOME" ]; then
    DATA=("$USER_HOME/Library/Application Support/groovex/Ohpe"
          "$USER_HOME/Library/Application Support/Ohpe.settings")
    have_data=0
    for d in "${DATA[@]}"; do [ -e "$d" ] && have_data=1; done
    if [ "$have_data" = 1 ]; then
        if [ "$PURGE" = 0 ] && ask_yes "Also delete your own Ohpe presets and settings?" \
                                       "Auch deine eigenen Ohpe-Presets und Einstellungen löschen?"; then
            PURGE=1
        fi
        if [ "$PURGE" = 1 ]; then
            for d in "${DATA[@]}"; do
                [ -e "$d" ] && [ ! -L "$d" ] && rm -rf "$d" && msg "removed  $d" "entfernt $d"
            done
            rmdir "$USER_HOME/Library/Application Support/groovex" 2>/dev/null || true
        else
            msg "Your presets and settings were kept:" "Deine Presets und Einstellungen wurden behalten:"
            for d in "${DATA[@]}"; do [ -e "$d" ] && echo "  $d"; done
        fi
    fi
fi

echo
if [ "$failed" = 0 ]; then
    msg "Ohpe has been uninstalled." "Ohpe wurde deinstalliert."
    finish 0
fi
finish 1
