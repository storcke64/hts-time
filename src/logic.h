#ifndef LOGIC_H
#define LOGIC_H

#include <glib.h>

/**
 * calculate_hts_string:
 * @year: The Gregorian year (e.g., 2025)
 * * Returns: A string in HTS notation (e.g., "A40002").
 * Must be freed with g_free().
 */
char* calculate_hts_string(int year);

/**
 * gregorian_from_hts:
 * @hts_str: A string in HTS notation (e.g., "A40000")
 * * Returns: The corresponding Gregorian year (e.g., 2023).
 */
long gregorian_from_hts(const char *hts_str);

#endif
