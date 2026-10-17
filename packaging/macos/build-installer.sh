#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Builds the macOS installer from an existing (universal) Release build:
#   <out>/Ohpe-Installer.pkg                     product archive with three selectable components
#   <out>/Ohpe-<version>-macOS-Installer.dmg     pkg + Gatekeeper fix + uninstaller + read-me files
#
#   packaging/macos/build-installer.sh [--artefacts DIR] [--version X.Y.Z] [--out DIR] [--no-dmg]
#
#   --artefacts  JUCE artefacts folder (default: build/Ohpe_artefacts/Release)
#   --version    installer version (default: CMAKE_PROJECT_VERSION from build/CMakeCache.txt,
#                otherwise the project(... VERSION ...) line of CMakeLists.txt)
#   --out        output folder (default: dist)
#
# Optional signing / notarisation - every step is skipped when its variable is empty:
#   OHPE_SIGN_APP_IDENTITY        "Developer ID Application: Name (TEAMID)" - signs app, plug-ins
#                                 and DMG with the hardened runtime (otherwise: ad-hoc signature)
#   OHPE_SIGN_INSTALLER_IDENTITY  "Developer ID Installer: Name (TEAMID)" - signs the pkg
#   OHPE_KEYCHAIN                 keychain that holds the identities (default: search list)
#   OHPE_NOTARY_APPLE_ID, OHPE_NOTARY_TEAM_ID, OHPE_NOTARY_PASSWORD (app-specific password)
#                                 notarise + staple pkg and DMG (needs both identities)
# The work folder is build/installer (override with OHPE_INSTALLER_WORK).

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
cd "$ROOT"

ARTEFACTS="build/Ohpe_artefacts/Release"
VERSION=""
OUT="dist"
MAKE_DMG=1
while [ $# -gt 0 ]; do
    case "$1" in
        --artefacts) ARTEFACTS="$2"; shift 2 ;;
        --version)   VERSION="$2"; shift 2 ;;
        --out)       OUT="$2"; shift 2 ;;
        --no-dmg)    MAKE_DMG=0; shift ;;
        -h|--help)   sed -n '2,25p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

step() { printf '\n==== %s\n' "$*"; }
die()  { echo "error: $*" >&2; exit 1; }

[ "$(uname -s)" = Darwin ] || die "the macOS installer can only be built on macOS"

# ------------------------------------------------------------------ version
if [ -z "$VERSION" ] && [ -f build/CMakeCache.txt ]; then
    VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' build/CMakeCache.txt | head -n1)"
fi
if [ -z "$VERSION" ]; then
    VERSION="$(sed -nE 's/^project\(Ohpe VERSION ([0-9]+(\.[0-9]+)*).*/\1/p' CMakeLists.txt | head -n1)"
fi
[ -n "$VERSION" ] || die "could not determine the version"
echo "Ohpe installer version $VERSION"

APP_ID="${OHPE_SIGN_APP_IDENTITY:-}"
PKG_ID="${OHPE_SIGN_INSTALLER_IDENTITY:-}"
KEYCHAIN_ARGS=()
[ -n "${OHPE_KEYCHAIN:-}" ] && KEYCHAIN_ARGS=(--keychain "$OHPE_KEYCHAIN")
NOTARISE=0
if [ -n "${OHPE_NOTARY_APPLE_ID:-}" ] && [ -n "${OHPE_NOTARY_TEAM_ID:-}" ] && [ -n "${OHPE_NOTARY_PASSWORD:-}" ]; then
    if [ -n "$APP_ID" ] && [ -n "$PKG_ID" ]; then
        NOTARISE=1
    else
        echo "notarisation credentials present but no Developer ID identities - notarisation skipped"
    fi
fi

WORK="${OHPE_INSTALLER_WORK:-build/installer}"
rm -rf "$WORK"
mkdir -p "$WORK/components" "$OUT"
WORK="$(cd "$WORK" && pwd)"
OUT="$(cd "$OUT" && pwd)"

# kind | bundle in the artefacts folder | install folder (relative to /) | package identifier
COMPONENTS=(
    "app|Standalone/Ohpe.app|Applications|com.groovex.ohpe.app"
    "vst3|VST3/Ohpe.vst3|Library/Audio/Plug-Ins/VST3|com.groovex.ohpe.vst3"
    "au|AU/Ohpe.component|Library/Audio/Plug-Ins/Components|com.groovex.ohpe.au"
)

sign_bundle() {
    local b="$1"
    if [ -n "$APP_ID" ]; then
        codesign --force --timestamp --options runtime --entitlements "$HERE/entitlements.plist" \
                 ${KEYCHAIN_ARGS[@]+"${KEYCHAIN_ARGS[@]}"} --sign "$APP_ID" "$b"
        echo "signed (Developer ID): $b"
    else
        codesign --force --deep -s - "$b"
        echo "signed (ad hoc): $b"
    fi
    codesign --verify --strict --verbose=2 "$b"
}

# ------------------------------------------------------------------ component packages
for entry in "${COMPONENTS[@]}"; do
    IFS='|' read -r kind src dest ident <<<"$entry"
    step "component $kind ($ident)"
    [ -d "$ARTEFACTS/$src" ] || die "missing $ARTEFACTS/$src - build Release first"
    bundle="$(basename "$src")"
    root="$WORK/root-$kind"
    mkdir -p "$root/$dest"
    ditto "$ARTEFACTS/$src" "$root/$dest/$bundle"
    xattr -cr "$root/$dest/$bundle"
    lipo "$root/$dest/$bundle/Contents/MacOS/Ohpe" -verify_arch arm64 x86_64 \
        || echo "warning: $bundle is not universal (arm64 + x86_64)"
    sign_bundle "$root/$dest/$bundle"

    scripts="$WORK/scripts-$kind"
    mkdir -p "$scripts"
    sed -e "s#@BUNDLE@#$dest/$bundle#g" -e "s#@KIND@#$kind#g" "$HERE/scripts/postinstall.in" > "$scripts/postinstall"
    chmod 755 "$scripts/postinstall"

    plist="$WORK/component-$kind.plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
        # always install to the fixed folder - never "relocate" onto a copy found elsewhere
        # (plug-in entries have no such key yet, so delete + add instead of set)
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
        /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$plist" 2>/dev/null || true
        i=$((i + 1))
    done

    pkgbuild --root "$root" --component-plist "$plist" --identifier "$ident" --version "$VERSION" \
             --install-location / --scripts "$scripts" --ownership recommended \
             "$WORK/components/ohpe-$kind.pkg"
    pkgutil --payload-files "$WORK/components/ohpe-$kind.pkg" > "$WORK/payload-$kind.txt"
    echo "payload: $(wc -l < "$WORK/payload-$kind.txt" | tr -d ' ') entries"
done

# ------------------------------------------------------------------ resources (EN + DE)
step "installer resources"
RES="$WORK/resources"
for lang in en de; do
    mkdir -p "$RES/$lang.lproj"
    for f in welcome.html readme.html; do
        sed "s/@VERSION@/$VERSION/g" "$HERE/resources/$lang.lproj/$f" > "$RES/$lang.lproj/$f"
    done
    cp "$HERE/resources/$lang.lproj/Localizable.strings" "$RES/$lang.lproj/"
    plutil -lint "$RES/$lang.lproj/Localizable.strings"
    cat "$HERE/resources/$lang.lproj/license-preamble.txt" LICENSE > "$RES/$lang.lproj/license.txt"
done
sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml.in" > "$WORK/distribution.xml"

# ------------------------------------------------------------------ product archive
step "productbuild"
PKG="$OUT/Ohpe-Installer.pkg"
rm -f "$PKG"
SIGN_ARGS=()
if [ -n "$PKG_ID" ]; then
    SIGN_ARGS=(--sign "$PKG_ID" --timestamp)
    [ -n "${OHPE_KEYCHAIN:-}" ] && SIGN_ARGS+=(--keychain "$OHPE_KEYCHAIN")
fi
productbuild --distribution "$WORK/distribution.xml" --resources "$RES" \
             --package-path "$WORK/components" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG"
if [ -n "$PKG_ID" ]; then
    pkgutil --check-signature "$PKG"
else
    echo "pkg left unsigned (OHPE_SIGN_INSTALLER_IDENTITY not set)"
fi

notarise() {
    local f="$1"
    step "notarise $(basename "$f")"
    xcrun notarytool submit "$f" --apple-id "$OHPE_NOTARY_APPLE_ID" --team-id "$OHPE_NOTARY_TEAM_ID" \
          --password "$OHPE_NOTARY_PASSWORD" --wait --timeout 30m
    xcrun stapler staple "$f"
    xcrun stapler validate "$f"
}
if [ "$NOTARISE" = 1 ]; then notarise "$PKG"; fi

echo "$VERSION" > "$OUT/VERSION.txt"
if [ "$MAKE_DMG" = 0 ]; then
    echo "done: $PKG"
    exit 0
fi

# ------------------------------------------------------------------ DMG
step "disk image"
STAGE="$WORK/dmg"
mkdir -p "$STAGE"
cp "$PKG" "$STAGE/"
for f in "Ohpe Gatekeeper Fix.command" "Uninstall Ohpe.command" "Read me first.txt" "Bitte zuerst lesen.txt"; do
    cp "$HERE/dmg/$f" "$STAGE/"
done
chmod 755 "$STAGE/Ohpe Gatekeeper Fix.command" "$STAGE/Uninstall Ohpe.command"
cp LICENSE "$STAGE/LICENSE.txt"

# p43: the volume shows the app icon (.VolumeIcon.icns + the custom-icon Finder flag on the volume root)
cp resources/icon/Ohpe.icns "$STAGE/.VolumeIcon.icns"

DMG="$OUT/Ohpe-$VERSION-macOS-Installer.dmg"
RWDMG="$WORK/Ohpe-rw.dmg"
RWMNT="$WORK/dmg-mount"
rm -f "$DMG" "$RWDMG"
ok=0
for attempt in 1 2 3; do
    if hdiutil create -volname "Ohpe" -srcfolder "$STAGE" -fs HFS+ -format UDRW -ov "$RWDMG"; then
        ok=1; break
    fi
    echo "hdiutil failed (attempt $attempt) - retrying"
    sleep 5
done
[ "$ok" = 1 ] || die "hdiutil create failed"
mkdir -p "$RWMNT"
hdiutil attach "$RWDMG" -nobrowse -noautoopen -mountpoint "$RWMNT" >/dev/null
# FinderInfo flags (bytes 8-9) = kHasCustomIcon 0x0400
xattr -wx com.apple.FinderInfo "0000000000000000040000000000000000000000000000000000000000000000" "$RWMNT"
if command -v SetFile >/dev/null 2>&1; then SetFile -a C "$RWMNT" || true; fi
sync
detached=0
for attempt in 1 2 3; do
    if hdiutil detach "$RWMNT" >/dev/null 2>&1 || hdiutil detach "$RWMNT" -force >/dev/null 2>&1; then
        detached=1; break
    fi
    sleep 3
done
[ "$detached" = 1 ] || die "hdiutil detach failed"
hdiutil convert "$RWDMG" -format UDZO -imagekey zlib-level=9 -ov -o "$DMG"
rm -f "$RWDMG"

if [ -n "$APP_ID" ]; then
    codesign --force --timestamp ${KEYCHAIN_ARGS[@]+"${KEYCHAIN_ARGS[@]}"} --sign "$APP_ID" "$DMG"
fi
if [ "$NOTARISE" = 1 ]; then notarise "$DMG"; fi

hdiutil verify "$DMG"
shasum -a 256 "$PKG" "$DMG" | sed "s#$OUT/##" > "$OUT/SHA256SUMS.txt"
cat "$OUT/SHA256SUMS.txt"
echo "done: $PKG"
echo "      $DMG"
