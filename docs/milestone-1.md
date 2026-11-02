# Milestone 1 - boot, translation, video and audio

Date: 2026-11-01. Toolkit: neogeorecomp v0.2.0 (upstream `af96b2b`) with
the generic changes in `docs/runtime-changes.md`.

## ROM verification

The supplied set is MAME `kof98` (NGM-242). Files and sizes:

```
242-p1.p1   2,097,152   242-p2.sp2  4,194,304
242-c1..c8  8,388,608 each
242-s1.s1     131,072   242-m1.m1     262,144
242-v1..v4  4,194,304 each
```

`kof98_decrypt` reproduced MAME's `kof98_prot_device::decrypt_68k()` and
wrote `242-pd.bin` (5,242,880 bytes) with a valid `NEO-GEO` header. The
Windows and Linux outputs are byte-identical (SHA-256
`d5a70f8a421651d09a6daedb0ab13e327aa24a279866c797cb5a6cbe82248aad`).

## Translation

```
[m68krecomp] kof98: 976607 instructions emitted across routines
[m68krecomp] 27226 routines, 43795 dispatch entries
```

After two profile passes the table has 44,126 entries. `--verify` on
9,000 frames:

```
[verify] 6406652 blocks checked, 0 mismatches
[exec] native blocks 6406652, native instructions 165304749,
       interpreted instructions 573 (100.00% native), 0 uncovered entry points
```

## Video

Headless screenshots (frame 1200) show the KOF '98 ring and `INSERT COIN`;
frame 7000 shows the match with health bars and the stage. Fullscreen under
Xvfb at 1280x800 reports:

```
[window] output 1280x800 dst 107,0 1066x800 fullscreen=1
```

That is a 4:3 image pillarboxed into the 16:10 panel, not stretched.

## Audio

A 20-second native-rate capture was compared with MAME 0.289 at the same
55,555 Hz rate:

| Window | Spectral cosine | RMS (ours / MAME) |
|---|---:|---:|
| 8 s | 0.9935 | 2658 / 2652 |
| 12 s | 0.9869 | 2225 / 2219 |
| 16 s | 0.9932 | 2481 / 2481 |

The boot envelope is 0.979 correlated at a 0.95 s offset (the BIOS/attract
timing difference). No clipping. The same check for KOF '96, '97, 2000,
Garou and Metal Slug 3 found the same native-rate agreement; the earlier
"high-frequency" difference was an artifact of comparing a 55,555 Hz WAV
resampled with a generic linear tool against MAME's 48 kHz output. At a
matched 55,555 Hz, every band is within 0.3 dB.

## Packaging

* Windows: `dist/windows/KOF98-Windows-x64/KOF98.exe`, `kof98_decrypt.exe`,
  `SDL2.dll`, `vcruntime140*.dll`, docs. Tested headless and with `--verify`.
* Steam Deck: `dist/steamdeck/KOF98-SteamDeck-x86_64.AppImage` (13.6 MB
  squashfs + runtime). Launched under Xvfb at 1280x800; bundled SDL2 is
  selected by `AppRun`.

## Not tested

* Physical Steam Deck, SteamOS Gaming Mode, Steam Input, suspend/resume.
* Subjective listening on speakers/headphones.
* Two-player, every stage/character, service menu.
* A physical gamepad.
