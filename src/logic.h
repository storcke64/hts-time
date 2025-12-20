#ifndef LOGIC_H
#define LOGIC_H

#include <glib.h>

char* calculate_hts_string(int year);
long gregorian_from_hts(const char* hts_str);

#endif
