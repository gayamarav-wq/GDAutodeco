#!/usr/bin/env bash
# Builds and runs the off-game unit tests for the planning core. Needs only g++/clang++ (C++20).
# Run from the project root:  bash tests/run_tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
OUT="${TMPDIR:-/tmp}/autodeco_tests"
mkdir -p "$OUT"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Isrc ${EXTRA_FLAGS:-}"
$CXX $FLAGS tests/test_core.cpp src/core/Types.cpp src/core/Analysis.cpp src/core/Planner.cpp \
     src/core/Preset.cpp src/core/Tempo.cpp src/core/ObjectString.cpp -o "$OUT/test_core"
"$OUT/test_core"
