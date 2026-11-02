# License status

**Status: confirmed - PolyForm Noncommercial 1.0.0 for the original project
code (2026-11-01).**

The repository owner confirmed that the Noncommercial 1.0.0 variant is the
intended license for the user's original code in this project:

* the game host (`src/main.c`, `src/rom_select.c`, `src/game.c`, `game.h`,
  `src/recomp_main.c`);
* the ROM preparation tool (`tools/decrypt.c`);
* the packaging and build scripts;
* this project's documentation.

Required Notice: Copyright (c) 2026 the KOF '98 Recompilation authors.

What this license does **not** cover
------------------------------------
* `neogeorecomp/` is the upstream toolkit by sp00nznet, **MIT**. Its
  `LICENSE`, `NOTICE` and `LICENSES/` directory are preserved verbatim and
  are not relicensed.
* Musashi (MIT), superzazu/z80 (MIT) and ymfm (BSD-3-Clause) are vendored
  under `neogeorecomp/ext/`; their notices are preserved.
* `tools/decrypt.c` follows MAME's `prot_kof98.cpp` (BSD-3-Clause); the
  header records the reference. It is a build-time tool and is not linked
  into the player.
* SDL2 is zlib licensed; the Windows package and AppImage bundle it under
  that license.
* The game ROMs, the Neo Geo BIOS, decrypted ROMs, recompiled C, entry
  lists and gameplay screenshots are **not** part of the licensed source
  and are not distributed. Users supply their own legally obtained dumps.
* `src/game.c` describes a commercial game's ROM layout. It contains no SNK
  code or data, but the game it describes is SNK's copyrighted work.

PolyForm Noncommercial is not a free/open-source license and does not permit
commercial redistribution. If commercial terms are needed, contact the
project owner.

See `THIRD_PARTY_NOTICES.md` for the complete attribution list.
