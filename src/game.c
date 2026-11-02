/*
 * game.c - The King of Fighters '98 (MAME set "kof98", NGM-242) ROM layout,
 * shared by the player and the recompiler.
 *
 * The supplied archive is the original MVS set.  Its program is encrypted by
 * SNK's early PROGSF1 scheme, so tools/decrypt.c reproduces MAME's
 * kof98_prot_device::decrypt_68k() into the build's roms/ directory:
 *
 *   242-pd.bin  5 MB  decrypted program in 68000 byte order:
 *                       $000000-$0FFFFF  fixed bank
 *                       $100000-$4FFFFF  four 1 MB banked pages
 *   C1..C8      8 x 8 MB CHA256 graphics, odd/even bitplane pairs
 *   242-s1.s1   128 KB fixed-layer tiles
 *   242-m1.m1   256 KB Z80 sound driver
 *   V1..V4      16 MB ADPCM-A samples
 *
 * The board also has an ALTERA protection latch.  Its state changes when
 * the program writes $0090 or $00F0 to $20AAAA; reads from $100/$102 then
 * return the values the real chip returns.  The generic ng_cart_prot_t
 * hook in the runtime handles that window, so no title-specific address
 * lives in neogeorecomp.
 *
 * P ROMs in the archive are stored byte-swapped (MAME's
 * ROM_LOAD16_WORD_SWAP); the prep tool already emits 68000 order, so the
 * loader does not swap 242-pd.bin again.
 */
#include "game.h"

static int s_prot_state;

/* The ALTERA overlay, from MAME prot_kof98.cpp.  State 0 and 2 return the
 * real cart header words ($4E45 "NE", $4F2D "O-"), state 1 the protection
 * values the program checks for. */
static uint16_t kof98_prot_read(uint32_t addr) {
    int odd = (int)((addr >> 1) & 1u);
    if (s_prot_state == 1) return odd ? 0x00FD : 0x00C2;
    return odd ? 0x4F2D : 0x4E45;
}

static void kof98_prot_write(uint32_t addr, uint16_t value) {
    (void)addr;
    if (value == 0x0090) s_prot_state = 1;
    else if (value == 0x00F0) s_prot_state = 2;
}

static void kof98_prot_reset(void) { s_prot_state = 0; }

static const ng_cart_prot_t k_kof98_prot = {
    .read_start = 0x000100, .read_end = 0x000103,
    .write_start = 0x20AAAA, .write_end = 0x20AAAB,
    .read = kof98_prot_read,
    .write = kof98_prot_write,
    .reset = kof98_prot_reset,
};

const ng_game_t k_kof98 = {
    .name = "kof98",
    .title = "The King of Fighters '98",
    .prom_size = 0x500000,          /* 1 MB fixed + 4 MB banked */
    .prom = {
        {"242-pd.bin", 0x000000, 0x000000, 0x500000, 0},
    },
    .crom_size = 0x4000000,         /* 64 MB after pairing */
    .crom_pairs = {
        {"242-c1.c1", "242-c2.c2"},
        {"242-c3.c3", "242-c4.c4"},
        {"242-c5.c5", "242-c6.c6"},
        {"242-c7.c7", "242-c8.c8"},
    },
    .srom = "242-s1.s1",
    .m1 = "242-m1.m1",
    .vrom = {"242-v1.v1", "242-v2.v2", "242-v3.v3", "242-v4.v4"},
    .prot = &k_kof98_prot,
};
