#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "logic.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: hts-time [year] or hts-time -g [HTS]\n");
        return 1;
    }
    if (g_strcmp0(argv[1], "-g") == 0 && argc == 3) {
        printf("Gregorian: %ld\n", gregorian_from_hts(argv[2]));
    } else {
        char *hts = calculate_hts_string(atoi(argv[1]));
        printf("HTS: %s\n", hts);
        g_free(hts);
    }
    return 0;
}
