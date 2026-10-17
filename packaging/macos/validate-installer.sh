#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# End-to-end check of the macOS installer built by build-installer.sh. Used by CI on the macOS
# runner. It REALLY INSTALLS into / (sudo) and uninstalls again, so it only runs on CI
# (CI=true) or when OHPE_VALIDATE_ON_THIS_MAC=1 is set.
#
#   packaging/macos/validate-installer.sh [dist-folder]   (default: dist)
#
# Steps: pkg structure (pkgutil --expand, --payload-files, choices), DMG contents, full install
# (installer -pkg ... -target /), files + receipts + signatures, auval -strict on the installed AU,
# Gatekeeper fix script (twice, on quarantined bundles), uninstall (keeps user presets),
# AU-only install via choice changes, uninstall --purge.

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
cd "$ROOT"
DIST="${1:-dist}"

[ "$(uname -s)" = Darwin ] || { echo "macOS only" >&2; exit 1; }
if [ "${CI:-}" != true ] && [ "${OHPE_VALIDATE_ON_THIS_MAC:-}" != 1 ]; then
    echo "This installs Ohpe into / and removes it again. Set OHPE_VALIDATE_ON_THIS_MAC=1 to run." >&2
    exit 1
fi

step() { printf '\n==== %s\n' "$*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "ok   $*"; }

PKG="$DIST/Ohpe-Installer.pkg"
DMG="$(ls "$DIST"/Ohpe-*-macOS-Installer.dmg 2>/dev/null | head -n1 || true)"
[ -f "$PKG" ] || fail "missing $PKG"
[ -n "$DMG" ] && [ -f "$DMG" ] || fail "missing DMG in $DIST"
VERSION="$(cat "$DIST/VERSION.txt")"

APP=/Applications/Ohpe.app
VST3=/Library/Audio/Plug-Ins/VST3/Ohpe.vst3
AU=/Library/Audio/Plug-Ins/Components/Ohpe.component
BUNDLES=("$APP" "$VST3" "$AU")
RECEIPTS=(com.groovex.ohpe.app com.groovex.ohpe.vst3 com.groovex.ohpe.au)
TMP="$(mktemp -d)"
MNT="$TMP/mnt"
cleanup() { hdiutil detach "$MNT" -force >/dev/null 2>&1 || true; }
trap cleanup EXIT

# Only the system-wide copies may be visible to auval / hosts during this test.
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/Ohpe.component" "$HOME/Library/Audio/Plug-Ins/VST3/Ohpe.vst3"

# ------------------------------------------------------------------ structure
step "pkg structure"
pkgutil --expand "$PKG" "$TMP/expanded"
[ -f "$TMP/expanded/Distribution" ] || fail "no Distribution in pkg"
for c in app vst3 au; do
    d="$TMP/expanded/ohpe-$c.pkg"
    # pkgutil --expand leaves Payload as an archive and unpacks Scripts into a folder
    # (older versions keep Scripts as a gzip'ed cpio archive) - accept both.
    [ -e "$d/Payload" ] || { find "$TMP/expanded" | head -n 40; fail "component ohpe-$c.pkg has no Payload"; }
    if [ -d "$d/Scripts" ]; then
        SCRIPTS_DIR="$d/Scripts"
    elif [ -f "$d/Scripts" ]; then
        mkdir -p "$TMP/scripts-$c"
        (cd "$TMP/scripts-$c" && gzip -dc "$d/Scripts" | cpio -i --quiet)
        SCRIPTS_DIR="$TMP/scripts-$c"
    else
        find "$TMP/expanded" | head -n 40
        fail "component ohpe-$c.pkg has no Scripts"
    fi
    grep -q "com.groovex.ohpe.$c" "$TMP/expanded/Distribution" || fail "Distribution lacks com.groovex.ohpe.$c"
    grep -q "version=\"$VERSION\"" "$d/PackageInfo" || fail "ohpe-$c.pkg version is not $VERSION"
    [ -x "$SCRIPTS_DIR/postinstall" ] || fail "ohpe-$c.pkg has no executable postinstall"
    grep -q "xattr -dr com.apple.quarantine" "$SCRIPTS_DIR/postinstall" || fail "ohpe-$c.pkg postinstall lacks the Gatekeeper fix"
    pass "component ohpe-$c.pkg (payload, postinstall, version $VERSION)"
done
for lang in en de; do
    for f in welcome.html readme.html license.txt Localizable.strings; do
        [ -f "$TMP/expanded/Resources/$lang.lproj/$f" ] || fail "missing Resources/$lang.lproj/$f"
    done
    grep -q "GNU AFFERO GENERAL PUBLIC LICENSE" "$TMP/expanded/Resources/$lang.lproj/license.txt" \
        || fail "$lang licence is not the AGPL"
done
pass "EN + DE welcome, readme, licence"

step "payload files"
check_payload() { # check_payload component.pkg expected-path
    pkgutil --payload-files "$1" > "$TMP/payload.txt"
    local rel="${2#./}"
    sed -e 's#^\./##' -e 's#^/##' "$TMP/payload.txt" > "$TMP/payload-rel.txt"
    grep -qxF "$rel" "$TMP/payload-rel.txt" || { cat "$TMP/payload.txt"; fail "$2 not in payload of $1"; }
    grep -qxF "$rel/Contents/MacOS/Ohpe" "$TMP/payload-rel.txt" || fail "$2 binary not in payload of $1"
    pass "$(basename "$1") installs $2 ($(wc -l < "$TMP/payload.txt" | tr -d ' ') entries)"
}
COMP="build/installer/components"
check_payload "$COMP/ohpe-app.pkg"  "./Applications/Ohpe.app"
check_payload "$COMP/ohpe-vst3.pkg" "./Library/Audio/Plug-Ins/VST3/Ohpe.vst3"
check_payload "$COMP/ohpe-au.pkg"   "./Library/Audio/Plug-Ins/Components/Ohpe.component"

step "choices"
if installer -showChoicesXML -pkg "$PKG" -target / > "$TMP/choices.xml" 2> "$TMP/choices.err"; then
    for c in app vst3 au; do
        grep -q "<string>$c</string>" "$TMP/choices.xml" || { cat "$TMP/choices.xml"; fail "choice '$c' missing"; }
    done
    pass "choices app, vst3, au selectable (installer -showChoicesXML)"
else
    # some installer versions refuse -showChoicesXML here; the Distribution file says the same,
    # and the AU-only install below exercises the choices for real
    echo "note: installer -showChoicesXML failed:"; cat "$TMP/choices.err" "$TMP/choices.xml"
    for c in app vst3 au; do
        grep -q "<choice id=\"$c\"" "$TMP/expanded/Distribution" || fail "choice '$c' missing in Distribution"
    done
    pass "choices app, vst3, au present in Distribution"
fi

# ------------------------------------------------------------------ DMG
step "disk image"
hdiutil verify "$DMG" >/dev/null
mkdir -p "$MNT"
hdiutil attach "$DMG" -nobrowse -readonly -mountpoint "$MNT" >/dev/null
for f in "Ohpe-Installer.pkg" "Ohpe Gatekeeper Fix.command" "Uninstall Ohpe.command" \
         "Read me first.txt" "Bitte zuerst lesen.txt" "LICENSE.txt"; do
    [ -e "$MNT/$f" ] || fail "DMG lacks '$f'"
done
[ -x "$MNT/Ohpe Gatekeeper Fix.command" ] || fail "Gatekeeper fix is not executable"
[ -x "$MNT/Uninstall Ohpe.command" ] || fail "uninstaller is not executable"
cmp -s "$MNT/Ohpe-Installer.pkg" "$PKG" || fail "pkg in the DMG differs from the built pkg"
pass "DMG contents"
# p43: volume icon = the app icon, custom-icon flag set on the volume root
cmp -s "$MNT/.VolumeIcon.icns" resources/icon/Ohpe.icns || fail "DMG volume icon missing or not resources/icon/Ohpe.icns"
FI="$(xattr -px com.apple.FinderInfo "$MNT" 2>/dev/null | tr -d ' \n' || true)"
[ "${FI:16:2}" = "04" ] || fail "DMG volume root lacks the custom-icon flag (FinderInfo: ${FI:-none})"
pass "DMG volume icon"
FIX="$MNT/Ohpe Gatekeeper Fix.command"
UNINSTALL="$MNT/Uninstall Ohpe.command"

# the read-me's Terminal tip works on a quarantined copy of the pkg
cp "$MNT/Ohpe-Installer.pkg" "$TMP/Ohpe-Installer.pkg"
xattr -w com.apple.quarantine "0081;00000000;Ohpe-CI;" "$TMP/Ohpe-Installer.pkg"
xattr -d com.apple.quarantine "$TMP/Ohpe-Installer.pkg"
if xattr -p com.apple.quarantine "$TMP/Ohpe-Installer.pkg" >/dev/null 2>&1; then fail "quarantine not removed"; fi
pass "xattr -d com.apple.quarantine on the pkg"

# ------------------------------------------------------------------ full install
installed_ok() {
    local b="$1"
    [ -d "$b" ] || fail "$b not installed"
    lipo "$b/Contents/MacOS/Ohpe" -verify_arch arm64 x86_64 || fail "$b is not universal"
    codesign --verify --strict "$b" || fail "$b signature invalid"
    # p43: own app icon in every bundle
    cmp -s "$b/Contents/Resources/AppIcon.icns" "$ROOT/resources/icon/Ohpe.icns" || fail "$b lacks the Ohpe app icon"
    [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "$b/Contents/Info.plist")" = "AppIcon.icns" ] || fail "$b: CFBundleIconFile is not AppIcon.icns"
    if xattr -p com.apple.quarantine "$b" >/dev/null 2>&1; then fail "$b still quarantined"; fi
    pass "$b (universal, signed, no quarantine)"
}

step "install everything (installer -pkg ... -target /)"
sudo installer -pkg "$TMP/Ohpe-Installer.pkg" -target / -verboseR
for b in "${BUNDLES[@]}"; do installed_ok "$b"; done
for r in "${RECEIPTS[@]}"; do
    pkgutil --pkg-info "$r" >/dev/null || fail "receipt $r missing"
done
pass "receipts ${RECEIPTS[*]}"

step "auval -strict on the installed Audio Unit"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 3
auval -a | grep -i ohpe || true
auval -strict -v aumu Ohpe Grvx

step "Gatekeeper fix on quarantined bundles (twice - idempotent)"
for b in "${BUNDLES[@]}"; do sudo xattr -w com.apple.quarantine "0081;00000000;Ohpe-CI;" "$b"; done
for run in 1 2; do
    echo "-- Gatekeeper fix run $run"
    sudo /bin/bash "$FIX" --yes
    for b in "${BUNDLES[@]}"; do installed_ok "$b"; done
done
auval -strict -v aumu Ohpe Grvx >/dev/null || fail "AU fails auval after the Gatekeeper fix"
pass "AU still passes auval -strict after the fix"

step "uninstall (keeps user presets)"
PRESETS="$HOME/Library/Application Support/groovex/Ohpe/Presets"
mkdir -p "$PRESETS"
echo '{}' > "$PRESETS/ci-test.ohpepreset"
sudo /bin/bash "$UNINSTALL" --yes
for b in "${BUNDLES[@]}"; do [ ! -e "$b" ] || fail "$b still present after uninstall"; done
for r in "${RECEIPTS[@]}"; do
    if pkgutil --pkg-info "$r" >/dev/null 2>&1; then fail "receipt $r still present"; fi
done
[ -f "$PRESETS/ci-test.ohpepreset" ] || fail "uninstall deleted user presets without being asked"
pass "app, plug-ins and receipts removed; user presets kept"

# ------------------------------------------------------------------ AU-only install
step "install only the Audio Unit (choice changes)"
cat > "$TMP/au-only.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>attributeSetting</key><integer>0</integer>
        <key>choiceAttribute</key><string>selected</string>
        <key>choiceIdentifier</key><string>app</string>
    </dict>
    <dict>
        <key>attributeSetting</key><integer>0</integer>
        <key>choiceAttribute</key><string>selected</string>
        <key>choiceIdentifier</key><string>vst3</string>
    </dict>
</array>
</plist>
EOF
sudo installer -pkg "$TMP/Ohpe-Installer.pkg" -target / -applyChoiceChangesXML "$TMP/au-only.xml"
installed_ok "$AU"
[ ! -e "$APP" ] || fail "app installed although deselected"
[ ! -e "$VST3" ] || fail "VST3 installed although deselected"
pass "only the AU was installed"

step "uninstall --purge"
sudo /bin/bash "$UNINSTALL" --yes --purge
[ ! -e "$AU" ] || fail "AU still present"
[ ! -e "$HOME/Library/Application Support/groovex/Ohpe" ] || fail "--purge kept the user data"
pass "AU removed, user data purged"

echo
echo "macOS installer validation passed (version $VERSION)"
