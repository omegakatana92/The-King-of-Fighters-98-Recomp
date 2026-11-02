/*
 * main.c - The King of Fighters '98 player.
 *
 * The hardware model, interpreter fallback and window live in
 * neogeorecomp.  This file hands the runtime the ROM layout and the table
 * that kof98_recomp generated from the user's own dump at build time.
 */
#include "game.h"
#include "rom_select.h"
#include <stdlib.h>
#include <string.h>

#ifdef KOF98_HAVE_RECOMP
extern const ng_func_entry_t ng_recomp_table[];
extern const size_t ng_recomp_count;
#endif

int main(int argc, char **argv) {
    char rom_path[4096];
    int selected = kof98_maybe_select_rom(argc, argv, rom_path, sizeof(rom_path));
    if (selected < 0)
        return 0;

    char **args = (char **)malloc((size_t)(argc + 3) * sizeof(*args));
    if (!args) return 1;
    int n = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--select-rom")) continue;
        args[n++] = argv[i];
    }
    if (selected > 0) {
        args[n++] = "--rom-path";
        args[n++] = rom_path;
    }
    args[n] = NULL;

    int rc;
#ifdef KOF98_HAVE_RECOMP
    rc = ng_main(n, args, &k_kof98, ng_recomp_table, ng_recomp_count);
#else
    rc = ng_main(n, args, &k_kof98, NULL, 0);
#endif
    free(args);
    return rc;
}
