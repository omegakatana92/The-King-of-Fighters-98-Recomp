The King of Fighters '98 - Steam Deck AppImage
==============================================

This AppImage contains the native Linux x86_64 player, built from the
neogeorecomp static recompiler.  It contains no SNK ROMs or BIOS files.

ROMs
----
Put your own legally obtained `kof98.zip` and `neogeo.zip` files, unzipped,
in one of these places (or pass `--rom-path DIR`):

  * a `roms` folder beside the AppImage
  * `~/.local/share/KOF98/roms`
  * `~/Emulation/roms/kof98`

If the folder holds the original encrypted set (242-p1.p1 / 242-p2.sp2),
the bundled `kof98_decrypt` helper prepares a decrypted copy in
`~/.local/share/KOF98/roms` on first launch.

Controls
--------
  Arrows / left stick     move
  Z X C V (or Xbox A B X Y)  Neo Geo A B C D
  5 / Back                coin
  1 / Start               start
  3 / left shoulder       select
  9                       service
  F2                      test switch
  F11                     toggle fullscreen
  Esc                     quit

The AppImage starts fullscreen.  Steam Input should map the controller to
the standard Xbox layout; the runtime uses SDL2's GameController API.

Display and audio
-----------------
The Neo Geo picture (320x224, 4:3) is pillarboxed into the Deck's 1280x800
16:10 panel, never stretched.  The YM2610 runs at 55,555 Hz; when the
system audio device only offers 48 kHz or 44.1 kHz, a 64-tap anti-alias
filter band-limits the signal before SDL's converter.

No ROM data is included.  The King of Fighters '98 and Neo Geo are
trademarks of SNK Corporation.  This project is not affiliated with or
endorsed by SNK.
