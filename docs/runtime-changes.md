# Runtime changes in the vendored toolkit

The vendored `neogeorecomp/` is upstream v0.2.0 (commit
`af96b2b4b39b42b392b17bcaa6a74a5f27088202`) plus the generic changes below.
None contains a KOF '98 address, value or branch; the game profile supplies
those through `ng_cart_prot_t`.

## 1. Stateful cartridge protection

`ng_cart_prot_t` (in `include/neogeorecomp/neogeo.h`) adds an inclusive
read/write window with `read`, `write` and `reset` callbacks. `ng_game_t`
and `ng_board_t` carry an optional pointer. `bus.c` resolves the window in
the slow path, leaves any overlapping page out of the fast table, and keeps
page 0 readable through `page0()` when the window lands there. This is what
KOF '98's ALTERA latch needs; future protected carts can use the same hook.

## 2. Standard bank-select clamp

For carts without an explicit `bank_map`, `write_banksel` now folds a
selector that points past the populated banked area back to bank 0, matching
MAME's `write_banksel()`. A 5 MB cart (1 MB fixed + 4 MB banked) therefore
treats selectors 4-7 as bank 0 instead of mirroring them.

## 3. Anti-alias filter for slower audio devices

`window_sdl.c` now runs a 64-tap Blackman-windowed sinc low-pass at
`0.45 x device rate` whenever the device rate is below the YM2610's
55,555 Hz. SDL's stream converter is a linear resampler; on a 48 kHz
Steam Deck/PipeWire device it folds the chip's upper band into the audible
range. The filter is bypassed when the device accepts 55,555 Hz. The
diagnostic `NG_AUDIO_DEVICE_RATE` requests a rate for testing.

## 4. Page-0 access hook

When a protection window falls in the fixed vector page, `bus_peek16`,
`dev_r8`/`dev_r16` and the recompiled fast path agree on the same page-0
contents. This keeps `--verify` and the native run identical.

The existing upstream fixes (M1 bank mask, per-frame `audio_sync`, no Z80
reset on BIOS/cart selection, `uint32_t` Z80 cycle delta, YM2610 port 3
read, wrap-safe sample ring) are retained.
