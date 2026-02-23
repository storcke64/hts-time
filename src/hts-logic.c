/*
 * Copyright 2023-2026 Antonio Storcke (Inventor)
 * Implementation of HTS Logic using GLib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "logic.h"

char* calculate_hts_string(int year) {
    // Baseline: 2023 CE = A40,000
    long total_hts_years = (long)year + 37977;

    int epoch_index = (int)(total_hts_years / 50000);
    int year_in_epoch = (int)(total_hts_years % 50000);

    // Cycle through A-Z
    char epoch_letter = 'A' + (epoch_index % 26);

    // g_strdup_printf automatically allocates the exact memory needed
    return g_strdup_printf("%c%d", epoch_letter, year_in_epoch);
}

long gregorian_from_hts(const char* hts_str) {
    if (hts_str == NULL || strlen(hts_str) < 2) return 0;

    int epoch_offset = (hts_str[0] - 'A') * 50000;
    int year_in_epoch = atoi(hts_str + 1);

    return (long)(epoch_offset + year_in_epoch - 37977);
}

/* --- New HTS Deep Time Tools --- */

char* calculate_future_hts(int current_year, int offset_years) {
    // Calculates notation for a point forward in time
    return calculate_hts_string(current_year + offset_years);
}

char* calculate_past_hts(int current_year, int offset_years) {
    // Calculates notation for a point backward in time
    return calculate_hts_string(current_year - offset_years);
}

long calculate_duration(const char *hts_start, const char *hts_end) {
    // Calculates the absolute number of years between two HTS notations
    return gregorian_from_hts(hts_end) - gregorian_from_hts(hts_start);
}
