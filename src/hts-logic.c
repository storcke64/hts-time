#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "logic.h"

/**
 * Storcke Human Time System Calculation:
 * - Epoch Start: 40,000 years pre-2023 (37,977 BCE)
 * - Epoch Duration: 50,000 years
 * - Epoch Naming: A, B, C...
 */
char* calculate_hts_string(int gregorian_year) {
    // 1. Calculate absolute years since the very beginning of the system
    // 2023 Gregorian is defined as 40,000 HTS.
    // Offset = 40,000 - 2023 = 37,977
    long total_hts_years = gregorian_year + 37977;

    // 2. Determine the Epoch (0=A, 1=B, 2=C...)
    int epoch_index = total_hts_years / 50000;
    char epoch_letter = 'A' + (epoch_index % 26);

    // 3. Determine the year within that Epoch (0 to 49,999)
    int year_in_epoch = total_hts_years % 50000;

    // 4. Format the final string (e.g., A40,002)
    // We use a buffer to hold the resulting string
    char buffer[32];
    sprintf(buffer, "%c%d", epoch_letter, year_in_epoch);

    return strdup(buffer);
}

/**
 * Reverse Calculation: HTS Notation back to Gregorian
 * Expected format: [Letter][Number] (e.g., "A40002")
 */
long gregorian_from_hts(const char* hts_string) {
    if (hts_string == NULL || strlen(hts_string) < 2) return 0;

    char epoch_char = hts_string[0];
    int epoch_index = epoch_char - 'A';
    int year_in_epoch = atoi(&hts_string[1]);

    long total_hts_years = (epoch_index * 50000) + year_in_epoch;

    // Reverse the offset: Gregorian = HTS - 37,977
    return total_hts_years - 37977;
}
