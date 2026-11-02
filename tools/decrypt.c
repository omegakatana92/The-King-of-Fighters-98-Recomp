/*
 * decrypt.c - prepare The King of Fighters '98 ROMs for the recompiler.
 *
 * The MAME kof98 set stores the 68000 program on the NEO-MVS PROGSF1 board
 * with an early SNK encryption and an ALTERA protection chip.  MAME's
 * kof98_prot_device::decrypt_68k() is the reference for the transformation
 * below; this tool reproduces it so the recompiler sees plain 68000 code.
 *
 * Input (the user's own unzipped MAME set):
 *
 *   242-p1.p1   2 MB  fixed program, ROM_LOAD16_WORD_SWAP
 *   242-p2.sp2  4 MB  banked program, ROM_LOAD16_WORD_SWAP
 *
 * Output (${out}/):
 *
 *   242-pd.bin  5 MB  decrypted image in 68000 byte order:
 *                       $000000-$0FFFFF  fixed bank
 *                       $100000-$4FFFFF  banked area
 *   plus the untouched C/S/M/V ROMs and the four BIOS files, so the same
 *   directory can be passed to the player with --rom-path.
 *
 * No ROM data is embedded here and nothing is written outside --out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

static int read_file(const char *dir, const char *name, uint8_t **out, size_t *size) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open %s\n", path); return -1; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return -1; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)n ? (size_t)n : 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "error: cannot read %s\n", path);
        free(buf); fclose(f); return -1;
    }
    fclose(f);
    *out = buf; *size = (size_t)n;
    return 0;
}

static int write_file(const char *dir, const char *name, const uint8_t *data, size_t size) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "error: cannot write %s\n", path); return -1; }
    if (fwrite(data, 1, size, f) != size) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

static int copy_file(const char *src_dir, const char *dst_dir, const char *name) {
    uint8_t *data; size_t size;
    if (read_file(src_dir, name, &data, &size)) return -1;
    int rc = write_file(dst_dir, name, data, size);
    free(data);
    return rc;
}

/* ROM_LOAD16_WORD_SWAP: the dump holds each 16-bit word little-endian. */
static void swap16(uint8_t *p, size_t n) {
    for (size_t i = 0; i + 1 < n; i += 2) {
        uint8_t t = p[i]; p[i] = p[i + 1]; p[i + 1] = t;
    }
}

/* MAME kof98_prot_device::decrypt_68k(), byte for byte. */
static void decrypt_kof98(uint8_t *src) {
    static const uint32_t sec[] = { 0x000000, 0x100000, 0x000004, 0x100004,
                                    0x10000a, 0x00000a, 0x10000e, 0x00000e };
    static const uint32_t pos[] = { 0x000, 0x004, 0x00a, 0x00e };
    const size_t dst_size = 0x200000;
    uint8_t *dst = (uint8_t *)malloc(dst_size);
    if (!dst) { fprintf(stderr, "error: out of memory\n"); exit(1); }
    memcpy(dst, src, dst_size);

    for (size_t i = 0x800; i < 0x100000; i += 0x200) {
        for (size_t j = 0; j < 0x100; j += 0x10) {
            for (size_t k = 0; k < 16; k += 2) {
                memcpy(&src[i + j + k], &dst[i + j + sec[k / 2] + 0x100], 2);
                memcpy(&src[i + j + k + 0x100], &dst[i + j + sec[k / 2]], 2);
            }
            if (i >= 0x080000 && i < 0x0c0000) {
                for (size_t k = 0; k < 4; k++) {
                    memcpy(&src[i + j + pos[k]], &dst[i + j + pos[k]], 2);
                    memcpy(&src[i + j + pos[k] + 0x100], &dst[i + j + pos[k] + 0x100], 2);
                }
            } else if (i >= 0x0c0000) {
                for (size_t k = 0; k < 4; k++) {
                    memcpy(&src[i + j + pos[k]], &dst[i + j + pos[k] + 0x100], 2);
                    memcpy(&src[i + j + pos[k] + 0x100], &dst[i + j + pos[k]], 2);
                }
            }
        }
        memcpy(&src[i + 0x000000], &dst[i + 0x000000], 2);
        memcpy(&src[i + 0x000002], &dst[i + 0x100000], 2);
        memcpy(&src[i + 0x000100], &dst[i + 0x000100], 2);
        memcpy(&src[i + 0x000102], &dst[i + 0x100100], 2);
    }
    memmove(&src[0x100000], &src[0x200000], 0x400000);
    free(dst);
}

static const char *k_extra[] = {
    "242-c1.c1", "242-c2.c2", "242-c3.c3", "242-c4.c4",
    "242-c5.c5", "242-c6.c6", "242-c7.c7", "242-c8.c8",
    "242-s1.s1", "242-m1.m1",
    "242-v1.v1", "242-v2.v2", "242-v3.v3", "242-v4.v4",
    "sp-s2.sp1", "sfix.sfix", "sm1.sm1", "000-lo.lo",
};

int main(int argc, char **argv) {
    const char *rom = NULL, *out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom-path") && i + 1 < argc) rom = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    }
    if (!rom || !out) {
        fprintf(stderr, "usage: %s --rom-path DIR --out DIR\n", argv[0]);
        return 2;
    }
    MKDIR(out);

    uint8_t *p1 = NULL, *p2 = NULL;
    size_t n1 = 0, n2 = 0;
    if (read_file(rom, "242-p1.p1", &p1, &n1) ||
        read_file(rom, "242-p2.sp2", &p2, &n2)) {
        free(p1); free(p2);
        return 1;
    }
    if (n1 != 0x200000 || n2 != 0x400000) {
        fprintf(stderr,
                "error: unexpected P ROM sizes: 242-p1.p1 %zu (want 2097152), "
                "242-p2.sp2 %zu (want 4194304)\n", n1, n2);
        free(p1); free(p2);
        return 1;
    }

    uint8_t *src = (uint8_t *)malloc(0x600000);
    if (!src) { free(p1); free(p2); return 1; }
    memcpy(src, p1, n1);
    memcpy(src + n1, p2, n2);
    free(p1); free(p2);
    swap16(src, 0x600000);
    decrypt_kof98(src);

    if (memcmp(src + 0x100, "NEO-GEO", 7) != 0) {
        fprintf(stderr, "error: decrypted image has no NEO-GEO header; "
                        "check that the dump is the MAME kof98 set\n");
        free(src);
        return 1;
    }
    if (write_file(out, "242-pd.bin", src, 0x500000)) { free(src); return 1; }
    free(src);

    for (size_t i = 0; i < sizeof(k_extra) / sizeof(k_extra[0]); i++)
        if (copy_file(rom, out, k_extra[i])) return 1;

    fprintf(stderr, "[kof98_decrypt] wrote %s/242-pd.bin and %zu companion files\n",
            out, sizeof(k_extra) / sizeof(k_extra[0]));
    return 0;
}
