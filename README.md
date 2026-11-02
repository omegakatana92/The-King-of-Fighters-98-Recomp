# The King of Fighters '98 - native recompilation

**Status: alpha, playable.** The game boots through a real MVS system ROM,
runs its attract mode, accepts a coin and start, reaches character select,
and plays a match on statically recompiled 68000 code. No emulator core is
linked into the player.

| Area | Result |
|---|---|
| ROM set | MAME `kof98` (NGM-242), original PROGSF1 program + ALTERA protection |
| Boot | Real `sp-s2.sp1` MVS BIOS; no BIOS HLE |
| 68000 | 44,126 recompiled entry points; **100.00% native**, 0 uncovered in a 9,000-frame scripted session |
| Correctness | `--verify`: 6,406,652 blocks, **0 mismatches** |
| Video | 320x224, 4:3 pillarboxed on 16:10 (1280x800 Steam Deck) |
| Audio | YM2610 at 55,555 Hz; native-rate capture matches MAME 0.289 (spectral cosine 0.987-0.994) |
| Windows | Release `KOF98.exe` built and tested |
| Steam Deck | Native Linux x86_64 AppImage built and launched under Xvfb at 1280x800 |
| License | PolyForm Noncommercial 1.0.0 (confirmed 2026-11-01) |

## What this is

The 68000 program is translated ahead of time by `neogeorecomp`'s
`m68krecomp` and compiled into native C. The runtime models the MVS board:
the 68000 bus, work/backup RAM, LSPC video, palette, sprite generator, I/O,
the Z80 sound CPU and the YM2610. Musashi is kept only as the fallback
interpreter and as the `--verify` oracle.

The KOF '98 cartridge has two extra pieces the generic runtime now models:

* the **PROGSF1 program encryption**, reproduced from MAME's
  `kof98_prot_device::decrypt_68k()` by `tools/decrypt.c`;
* the **ALTERA protection latch**, which answers reads from `$100`/`$102`
  differently after writes to `$20AAAA`. The runtime exposes a generic
  `ng_cart_prot_t` read/write/reset hook; all KOF '98 addresses and values
  live in `src/game.c`, not in the toolkit.

No ROM, BIOS, decrypted ROM, generated C or game-art screenshot is
committed or distributed.

## Requirements

* **Windows**: Windows 11 x64, Visual Studio 2022/2026 with the C++
  workload, CMake 3.21+, SDL2 2.30+ (optional; without it the build is
  headless-only).
* **Linux / Steam Deck**: gcc/g++, CMake 3.21+, make, `libsdl2-dev`,
  `wget` (for `appimagetool` when packaging).
* Your own legally obtained `kof98.zip` and `neogeo.zip`.

## Prepare your ROMs

The build reads an **unzipped** folder. Put both sets in one directory:

```powershell
mkdir roms-src
# Windows: right-click each zip -> Extract All, or use 7-Zip
```

```
roms-src/
  242-p1.p1  242-p2.sp2  242-c1.c1 .. 242-c8.c8
  242-s1.s1  242-m1.m1   242-v1.v1 .. 242-v4.v4
  sp-s2.sp1  sfix.sfix  sm1.sm1  000-lo.lo
```

The build runs `kof98_decrypt` automatically and writes the prepared set to
`build-windows/roms` (or `build-linux/roms`). You can also run it by hand:

```powershell
build-windows\Release\kof98_decrypt.exe --rom-path roms-src --out build-windows\roms
```

The tool checks the two P ROM sizes and the `NEO-GEO` header after
decryption. It never writes outside `--out` and never downloads anything.
Passing an encrypted folder to `--rom-path` also prepares it in the
per-user cache on first use.

## Build - Windows x64

```powershell
cmake -S . -B build-windows -G "Visual Studio 18 2026" -A x64 `
      -DKOF98_ROM_DIR="$PWD\roms-src" `
      -DSDL2_DIR="C:\path\to\SDL2-2.32.6\cmake"
cmake --build build-windows --config Release --parallel
```

Output: `build-windows/Release/KOF98.exe` (plus `SDL2.dll`).

## Build - Linux x86_64 and Steam Deck AppImage

```bash
./scripts/build-linux.sh "$PWD/roms-src"
# then package:
./packaging/appimage/build-appimage.sh "$PWD/roms-src"
```

The AppImage is written to
`dist/steamdeck/KOF98-SteamDeck-x86_64.AppImage`. It bundles SDL2 and the
`kof98_decrypt` helper. On first launch it looks for a `roms` folder beside
the AppImage, `~/.local/share/KOF98/roms`, or `~/Emulation/roms/kof98`. If
it finds the original encrypted set, it prepares a decrypted copy in the
per-user data directory.

## Run

```powershell
build-windows\Release\KOF98.exe --rom-path build-windows\roms
# AppImage:
./KOF98-SteamDeck-x86_64.AppImage
```

Without `--rom-path` the player searches the current directory, the
executable directory, `KOF98_ROM_DIR`, and the per-user locations. On a
desktop build with no folder found it opens a native folder picker.

### Controls

| Action | Keyboard | Gamepad (Xbox layout) |
|---|---|---|
| Move | arrows | D-pad / left stick |
| A B C D | Z X C V | A B X Y |
| Start | 1 | Start |
| Coin | 5 | Back |
| Select | 3 | Left shoulder |
| Service / test | 9 / F2 | - |
| Fullscreen | F11 | - |
| Quit | Esc | - |

Steam Input should use the standard Xbox layout; the runtime reads pads
through SDL2's GameController API and supports hot-plug and two players.

## Verification commands

```powershell
# 9,000 frames: BIOS, title, coin/start, character select, a match.
# NG_RTC fixes the calendar/RNG seed so the session is reproducible.
$env:NG_RTC="1998-01-01"
build-windows\Release\KOF98.exe --headless --verify `
    --rom-path build-windows\roms --frames 9000 --input tests\play.txt
```

Recorded result:

```
[verify] 6406652 blocks checked, 0 mismatches
[exec] native blocks 6406652, native instructions 165304749,
       interpreted instructions 573 (100.00% native), 0 uncovered entry points
```

Video and audio checks:

```powershell
build-windows\Release\KOF98.exe --headless --rom-path build-windows\roms `
    --frames 6000 --input tests\play.txt `
    --screenshot 1200:shots/title.png --screenshot 8000:shots/match.png `
    --wav shots/game.wav
```

The `audio-check` CMake target runs a 12,000-frame scripted capture and
fails on music-sized silent gaps.

## Native coverage

The recompiler discovers entry points from the vectors, the cartridge
header, call targets, jump tables and code pointers. The first 9,000-frame
`--verify` run found 312 runtime-only entries; after adding them and
rebuilding, the same session reaches 100.00% native with 0 uncovered
entries. `--dump-misses` and the `profile` target repeat that process.

## Known limitations

* **Not tested on physical Steam Deck hardware** or SteamOS Gaming Mode.
  The AppImage was launched under Xvfb at 1280x800, and the Windows build
  was run windowed; no physical gamepad or Steam Input configuration was
  available in this environment.
* **Two-player and every stage/character** are not exhaustively tested.
  The scripted session covers title, how-to-play, character select and the
  first match.
* The AppImage needs FUSE or `--appimage-extract-and-run` on systems where
  FUSE is unavailable.
* ROM preparation requires the exact MAME `kof98` set. A different dump
  fails the size/header check rather than producing a silent bad image.

## Project layout

```
CMakeLists.txt                 configure, decrypt, recompile, player, tests
src/game.c, game.h             verified ng_game_t + ALTERA protection
src/main.c                     player (ng_main)
src/rom_select.c/.h            ROM folder discovery and first-run decrypt
src/recomp_main.c              generator (ng_recomp_main)
tools/decrypt.c                PROGSF1 decryptor (MAME reference)
neogeorecomp/                  vendored toolkit (MIT) + generic runtime fixes
tests/coin_start.txt, play.txt scripted input
packaging/appimage/            AppImage build script, desktop entry, icon
packaging/windows/             SDL2 license text and package README
docs/                          verification notes
dist/windows/                  Windows package (no ROMs)
dist/steamdeck/                AppImage (no ROMs)
```

## Licensing

* The vendored `neogeorecomp/` toolkit remains **MIT**; its `LICENSE`,
  `NOTICE` and `LICENSES/` are preserved.
* Musashi (MIT), superzazu/z80 (MIT), ymfm (BSD-3-Clause) and SDL2 (zlib)
  keep their own terms.
* `tools/decrypt.c` follows MAME's `prot_kof98.cpp` (BSD-3-Clause); the
  header records the reference and the file is not linked into the player.
* The original project code is licensed under **PolyForm Noncommercial
  1.0.0** (confirmed 2026-11-01). The upstream toolkit is **not**
  relicensed.
* No SNK ROMs, BIOS images, decrypted ROMs, generated C or gameplay art are
  distributed. *The King of Fighters '98* and *Neo Geo* are trademarks of
  SNK Corporation. This project is not affiliated with or endorsed by SNK.
