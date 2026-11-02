The King of Fighters '98 - Windows x64
======================================

This package contains:
  KOF98.exe             native recompiled player
  kof98_decrypt.exe     prepares the original encrypted ROM set
  SDL2.dll              window, audio, keyboard and gamepad
  vcruntime140*.dll     MSVC 2022 runtime
  README.txt            this file
  LICENSE.txt, LICENSE-STATUS.txt, THIRD_PARTY_NOTICES.txt

No SNK ROMs or BIOS files are included. You must supply your own legally
obtained kof98.zip and neogeo.zip.

Quick start
-----------
1. Unzip both ROM sets into one folder, for example:

     roms\
       242-p1.p1 242-p2.sp2 242-c1.c1 .. 242-c8.c8
       242-s1.s1 242-m1.m1 242-v1.v1 .. 242-v4.v4
       sp-s2.sp1 sfix.sfix sm1.sm1 000-lo.lo

2. If the folder holds the original encrypted set, prepare it once:

     kof98_decrypt.exe --rom-path roms --out roms-dec

   (The player also does this automatically when it is given an encrypted
   folder with --rom-path or finds one beside the executable.)

3. Run:

     KOF98.exe --rom-path roms-dec

   or just KOF98.exe if the folder is named `roms` beside the executable.
   Use `--fullscreen` for a borderless 4:3 pillarboxed picture.

Controls
--------
  Arrows          move
  Z X C V         A B C D
  5               coin
  1               start
  3               select
  9 / F2          service / test
  F11             fullscreen
  Esc             quit

  Gamepad (Xbox layout): D-pad/left stick, A B X Y = A B C D,
  Start = start, Back = coin, left shoulder = select.

Useful options
--------------
  --headless            no window, run as fast as possible
  --frames N            stop after N frames
  --screenshot N:FILE   save a PNG
  --wav FILE            capture the YM2610 output
  --verify              replay every native block in Musashi
  --dump-misses FILE    collect runtime-only entry points
  --scale N             window scale (default 3)
  --fullscreen
  --mute

Verified in this build
----------------------
  9,000-frame scripted session with NG_RTC=1998-01-01: 0 mismatches,
  100.00% native, 0 uncovered entry points (6,401,389 blocks).

Licensing
---------
The upstream neogeorecomp toolkit is MIT. Musashi and z80 are MIT, ymfm is
BSD-3-Clause, SDL2 is zlib. The original project code is licensed under
PolyForm Noncommercial 1.0.0 (confirmed 2026-11-01). The King of Fighters
'98 and Neo Geo are trademarks of SNK Corporation. This project is not
affiliated with or endorsed by SNK.

