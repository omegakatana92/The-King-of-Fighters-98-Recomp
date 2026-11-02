#!/usr/bin/env bash
# Build the Linux x86_64 player and package it as a Steam Deck AppImage.
#
# Prerequisites: gcc, g++, cmake 3.21+, make, libsdl2-dev, wget.
# The user's ROMs are never bundled: the AppImage contains the recompiled
# player and the build-time decrypt tool only.
#
# Usage: ./packaging/appimage/build-appimage.sh [ROM_DIR]
# Default ROM_DIR=$ROOT/roms-src, output dist/steamdeck/...
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROMS="${1:-$ROOT/roms-src}"
BUILD="$ROOT/build-linux"
# mksquashfs on WSL's 9p mount can fail with "Out of memory" on the large
# player binary.  Assemble the AppDir and produce the image on the Linux
# filesystem, then copy the finished AppImage back to the project.
APPDIR="${KOF98_APPDIR:-$HOME/kof98-AppDir}"
OUT="$ROOT/dist/steamdeck/KOF98-SteamDeck-x86_64.AppImage"
OUT_LOCAL="$HOME/KOF98-SteamDeck-x86_64.AppImage"

echo "== configure/build (Release) =="
EXTRA=()
if [ -n "${KOF98_PREGENERATED:-}" ]; then
    EXTRA+=("-DKOF98_PREGENERATED=$KOF98_PREGENERATED")
fi
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
      "-DKOF98_ROM_DIR=$ROMS" "${EXTRA[@]}"
cmake --build "$BUILD" --parallel "$(nproc)" --target kof98 kof98_decrypt

echo "== assemble AppDir =="
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$APPDIR/usr/share/doc/kof98"
cp "$BUILD/KOF98" "$APPDIR/usr/bin/KOF98"
cp "$BUILD/kof98_decrypt" "$APPDIR/usr/bin/kof98_decrypt"
strip --strip-unneeded "$APPDIR/usr/bin/KOF98" 2>/dev/null || true
strip --strip-unneeded "$APPDIR/usr/bin/kof98_decrypt" 2>/dev/null || true

# Bundle the SDL2 the build linked against; SteamOS's system copy may differ.
SDL=$(ldd "$BUILD/KOF98" | awk '/libSDL2-2\.0\.so\.0/{print $3; exit}')
if [ -n "$SDL" ]; then cp "$SDL" "$APPDIR/usr/lib/"; fi

cp "$ROOT/packaging/appimage/kof98.png" "$APPDIR/kof98.png"
cp "$ROOT/packaging/appimage/kof98.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/kof98.png"
cp "$ROOT/packaging/appimage/kof98.desktop" "$APPDIR/kof98.desktop"
cp "$ROOT/packaging/appimage/kof98.desktop" "$APPDIR/usr/share/applications/kof98.desktop"
cp "$ROOT/packaging/appimage/README.txt" "$APPDIR/usr/share/doc/kof98/README.txt"
cp "$ROOT/LICENSE" "$APPDIR/usr/share/doc/kof98/LICENSE.txt"
cp "$ROOT/LICENSE-STATUS.md" "$APPDIR/usr/share/doc/kof98/LICENSE-STATUS.txt" 2>/dev/null || true
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$APPDIR/usr/share/doc/kof98/THIRD_PARTY_NOTICES.txt"
mkdir -p "$APPDIR/usr/share/doc/kof98/LICENSES"
cp "$ROOT/neogeorecomp/LICENSE" "$APPDIR/usr/share/doc/kof98/LICENSES/neogeorecomp.txt"
cp "$ROOT/neogeorecomp/NOTICE" "$APPDIR/usr/share/doc/kof98/LICENSES/neogeorecomp-NOTICE.txt"
cp "$ROOT/neogeorecomp/LICENSES/"*.txt "$APPDIR/usr/share/doc/kof98/LICENSES/"
cp "$ROOT/packaging/windows/SDL2.txt" "$APPDIR/usr/share/doc/kof98/LICENSES/SDL2.txt" 2>/dev/null || true

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

has_rom=0
select_rom=0
for a in "$@"; do
    [ "$a" = "--rom-path" ] && has_rom=1
    [ "$a" = "--select-rom" ] && select_rom=1
done

if [ "$has_rom" = 0 ] && [ "$select_rom" = 0 ]; then
    cand=""
    if [ -n "${KOF98_ROM_DIR:-}" ]; then
        cand="$KOF98_ROM_DIR"
    elif [ -n "${APPIMAGE:-}" ]; then
        for d in "$(dirname "$APPIMAGE")/roms" \
                 "$(dirname "$APPIMAGE")/roms/kof98" \
                 "$HOME/.local/share/KOF98/roms" \
                 "$HOME/.local/share/kof98/roms" \
                 "$HOME/Emulation/roms/kof98" \
                 "$HOME/roms/kof98" "$HOME/roms"; do
            if [ -d "$d" ]; then cand="$d"; break; fi
        done
    fi
    if [ -n "$cand" ]; then
        if [ -f "$cand/242-pd.bin" ]; then
            set -- --rom-path "$cand" "$@"
        elif [ -f "$cand/242-p1.p1" ]; then
            CACHE="$HOME/.local/share/KOF98/roms"
            if [ ! -f "$CACHE/242-pd.bin" ]; then
                mkdir -p "$CACHE"
                "$HERE/usr/bin/kof98_decrypt" --rom-path "$cand" --out "$CACHE" || exit 1
            fi
            set -- --rom-path "$CACHE" "$@"
        else
            set -- --rom-path "$cand" "$@"
        fi
    fi
fi

add_fs=1
for a in "$@"; do
    case "$a" in --headless|--fullscreen|--scale) add_fs=0;; esac
done
[ "$add_fs" = 1 ] && set -- --fullscreen "$@"

exec "$HERE/usr/bin/KOF98" "$@"
EOF
chmod +x "$APPDIR/AppRun"

echo "== appimagetool =="
mkdir -p "$ROOT/dist/steamdeck"
TOOL="${APPIMAGETOOL:-$HOME/appimagetool-x86_64.AppImage}"
if [ ! -x "$TOOL" ]; then
    TOOL="$BUILD/appimagetool.AppImage"
    wget -q -O "$TOOL" \
      https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$TOOL"
fi
rm -f "$OUT_LOCAL"
ARCH=x86_64 "$TOOL" --appimage-extract-and-run --no-appstream "$APPDIR" "$OUT_LOCAL"
chmod +x "$OUT_LOCAL"
cp "$OUT_LOCAL" "$OUT"
chmod +x "$OUT"
echo "wrote $OUT"
