/*
 * Copyright 2023-2026 Antonio Storcke (Inventor)
 * HTS Logic Header Definitions
 */

#ifndef LOGIC_H
#define LOGIC_H

#include <glib.h>

char* calculate_hts_string(int year);
long gregorian_from_hts(const char *hts_str);
char* calculate_future_hts(int current_year, int offset_years);
char* calculate_past_hts(int current_year, int offset_years);
long calculate_duration(const char *hts_start, const char *hts_end);

#endif
