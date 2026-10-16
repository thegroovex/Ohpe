#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Packs what the test, validation and installer jobs need from a build tree into one tar file
# (keeps executable bits and bundle symlinks, which artefact uploads lose): the built app /
# plug-ins, the test executables, CTest's test files and CMakeCache.txt. Object files, static
# libraries, generated binary-data sources and debug databases stay behind.
#
#   scripts/ci/pack-build.sh <build dir> <out.tar>
set -euo pipefail
BUILD="${1:?build dir}"
OUT="${2:?output tar}"
tar -cf "$OUT" \
    --exclude='CMakeFiles' --exclude='.ninja_*' --exclude='build.ninja' \
    --exclude='*.o' --exclude='*.obj' --exclude='*.a' --exclude='*.lib' --exclude='*.exp' \
    --exclude='*.pdb' --exclude='*.ilk' --exclude='juce_binarydata_*' --exclude='JuceLibraryCode' \
    --exclude='installer' \
    "$BUILD"
ls -l "$OUT"
tar -tf "$OUT" | grep -vE '/$' | sed 's/^/  /' | head -n 200
