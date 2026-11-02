#!/usr/bin/env bash
# Build the Linux x86_64 player (Release). No packaging, no downloads.
#
# Usage: ./scripts/build-linux.sh [ROM_DIR]
# Default ROM_DIR is <repo>/roms-src (the unzipped MAME set); the build
# decrypts the program into build-linux/roms.  Set KOF98_PREGENERATED to a
# generated/ directory to reuse C from another build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROMS="${1:-$ROOT/roms-src}"
BUILD="$ROOT/build-linux"
JOBS="${JOBS:-4}"

echo "== KOF '98: Linux x86_64 build =="
echo "repo: $ROOT"
echo "roms: $ROMS"
EXTRA=()
if [ -n "${KOF98_PREGENERATED:-}" ]; then
    EXTRA+=("-DKOF98_PREGENERATED=$KOF98_PREGENERATED")
fi
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
      "-DKOF98_ROM_DIR=$ROMS" "${EXTRA[@]}"
cmake --build "$BUILD" --parallel "$JOBS" --target kof98
echo "built $BUILD/KOF98"
