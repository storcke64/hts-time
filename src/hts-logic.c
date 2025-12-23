/*
 * Copyright 2023-2025 Antonio Storcke (Inventor)
 * Licensed under the Apache License, Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logic.h"

char* calculate_hts_string(int year) {
    long total_hts_years = (long)year + 37977;
    int epoch_index = (int)(total_hts_years / 50000);
    int year_in_epoch = (int)(total_hts_years % 50000);
    char epoch_letter = 'A' + (epoch_index % 26);
    char *result = malloc(64);
    sprintf(result, "%c%d", epoch_letter, year_in_epoch);
    return result;
}

long gregorian_from_hts(const char* hts_str) {
    if (hts_str == NULL || strlen(hts_str) < 2) return 0;
    int epoch_offset = (hts_str[0] - 'A') * 50000;
    int year_in_epoch = atoi(hts_str + 1);
    return (long)(epoch_offset + year_in_epoch - 37977);
}
