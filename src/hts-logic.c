#include "logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* calculate_hts_string(int year) {
    // Basic HTS calculation logic
    return g_strdup_printf("1A%d", year + 13000);
}

long gregorian_from_hts(const char* hts_str) {
    // Basic Reverse logic
    if (strlen(hts_str) < 3) return 0;
    return atol(hts_str + 2) - 13000;
}
