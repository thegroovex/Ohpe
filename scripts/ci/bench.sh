#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# p79: runs ohpe_bench (quick, 48 kHz / 256) on a CI runner, compares with tools/bench/baseline-<os>.csv and writes
# bench-<os>.csv (upload it to refresh the baseline). Informational: fails only when a scenario renders a non-finite or
# runaway output (exit code 2 of ohpe_bench), never on timing.
#
#   scripts/ci/bench.sh <build dir> <os: linux|macos|windows>
set -uo pipefail
BUILD="${1:?build dir}"
OS="${2:?os}"
EXE=""
for c in "$BUILD/tests/core/ohpe_bench" "$BUILD/tests/core/ohpe_bench.exe" "$BUILD/tests/core/Release/ohpe_bench.exe"; do
    [ -x "$c" ] && EXE="$c" && break
done
if [ -z "$EXE" ]; then echo "ohpe_bench not found in $BUILD"; exit 1; fi
"$EXE" --quick --baseline "tools/bench/baseline-$OS.csv" --write "bench-$OS.csv" | tee bench.txt
rc=${PIPESTATUS[0]}
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    { echo "### ohpe_bench ($OS CI runner, informational)"; echo '```'; cat bench.txt; echo '```'; } >> "$GITHUB_STEP_SUMMARY"
fi
exit $rc
