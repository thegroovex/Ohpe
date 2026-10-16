#!/usr/bin/env bash
# Ohpe - designed by groovex
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Runs every test Ohpe has. Run it before every push; CI runs the same steps.
#
#   scripts/test-all.sh                  core tests, plug-in tests, VST3 host test, pluginval
#   OHPE_CORE_ONLY=1 scripts/test-all.sh core tests only (no JUCE needed, seconds)
#
# Environment:
#   OHPE_JUCE_PATH   local JUCE 8.0.15 checkout (otherwise CMake downloads JUCE)
#   PLUGINVAL        path to the pluginval binary (otherwise looked up in PATH; skipped if missing,
#                    unless OHPE_REQUIRE_PLUGINVAL=1)
#   OHPE_BUILD_DIR   build directory for the full build (default: build)
#   OHPE_CMAKE_ARGS  extra CMake arguments, e.g. "-DOHPE_LTO=OFF" for faster local links
#   OHPE_STRICTNESS  pluginval strictness level (default: 5)

set -euo pipefail
cd "$(dirname "$0")/.."

GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
JOBS="${OHPE_JOBS:-}"
BUILD_ARGS=()
[ -n "$JOBS" ] && BUILD_ARGS=(--parallel "$JOBS")

step() { printf '\n==== %s\n' "$*"; }

# ------------------------------------------------------------------ core (no JUCE)
step "core: configure + build"
cmake -S . -B build-core "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DOHPE_BUILD_PLUGIN=OFF >/dev/null
cmake --build build-core --config Release "${BUILD_ARGS[@]}"
# The doctest binaries run one test case per process on all cores when Python 3 is available
# (scripts/ci/run-tests.py, the same runner CI uses); otherwise plain ctest.
PY="$(command -v python3 || true)"
run_tests() { # run_tests <build dir> <ctest exclude regex or empty> <doctest binaries to split>
    if [ -n "$PY" ]; then
        local ex=(); [ -n "$2" ] && ex=(-E "$2")
        "${RUN[@]}" "$PY" scripts/ci/run-tests.py --build-dir "$1" -C Release --split "$3" "${ex[@]}" ${JOBS:+-j "$JOBS"}
    else
        local ex=(); [ -n "$2" ] && ex=(-E "$2")
        "${RUN[@]}" ctest --test-dir "$1" -C Release --output-on-failure "${ex[@]}"
    fi
}
RUN=()

step "core: tests"
run_tests build-core "" core_tests

if [ "${OHPE_CORE_ONLY:-0}" = "1" ]; then
    echo; echo "CORE TESTS PASSED"; exit 0
fi

# ------------------------------------------------------------------ plug-in, UI, host test
BUILD="${OHPE_BUILD_DIR:-build}"
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
[ -n "${OHPE_JUCE_PATH:-}" ] && CMAKE_ARGS+=("-DOHPE_JUCE_PATH=${OHPE_JUCE_PATH}")
# shellcheck disable=SC2206
[ -n "${OHPE_CMAKE_ARGS:-}" ] && CMAKE_ARGS+=(${OHPE_CMAKE_ARGS})

step "plug-in: configure + build ($BUILD)"
cmake -S . -B "$BUILD" "${GEN[@]}" "${CMAKE_ARGS[@]}" >/dev/null
cmake --build "$BUILD" --config Release "${BUILD_ARGS[@]}"

# GUI tests need a display; use xvfb on headless Linux
RUN=()
if [ "$(uname)" = "Linux" ] && [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
    RUN=(xvfb-run -a)
fi

# core_tests passed above (same sources and flags) - not run a second time here
step "plug-in: tests (processor/editor, VST3 host)"
run_tests "$BUILD" '^core_tests$' plugin_tests

# ------------------------------------------------------------------ pluginval
PV="${PLUGINVAL:-$(command -v pluginval || true)}"
VST3="$BUILD/Ohpe_artefacts/Release/VST3/Ohpe.vst3"
if [ -n "$PV" ] && [ -x "$PV" ]; then
    step "pluginval (strictness ${OHPE_STRICTNESS:-5}) on $VST3"
    "${RUN[@]}" "$PV" --strictness-level "${OHPE_STRICTNESS:-5}" --validate-in-process \
        --timeout-ms 600000 --output-dir "$BUILD/pluginval" --validate "$VST3"
elif [ "${OHPE_REQUIRE_PLUGINVAL:-0}" = "1" ]; then
    echo "pluginval not found (set PLUGINVAL)" >&2; exit 1
else
    step "pluginval not found - skipped (set PLUGINVAL=/path/to/pluginval)"
fi

echo; echo "ALL TESTS PASSED"
