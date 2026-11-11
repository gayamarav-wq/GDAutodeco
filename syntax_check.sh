#!/usr/bin/env bash
# Type-checks the Geode glue (src/*.cpp, src/ui/*.cpp) against tests/stub - a hand-written stand-in for the Geode API.
# Proves INTERNAL consistency only (signatures match across my files, no typos/missing includes). It cannot prove
# that the real Geode SDK matches the stub; see README "API risks".
set -uo pipefail
cd "$(dirname "$0")/.."
fail=0
for f in src/*.cpp src/ui/*.cpp; do
  out=$(${CXX:-g++} -std=c++23 -fsyntax-only -Wall -Wextra -Wno-overloaded-virtual -Isrc -Itests/stub "$f" 2>&1)
  if [ -n "$out" ]; then echo "== $f"; echo "$out" | head -${LINES_MAX:-25}; fail=1; else echo "ok  $f"; fi
done
exit $fail
