#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

// --- Constants Aligned with HTS Logic (2023 CE = A40000) ---
// HTS Start Year: 2023 - 40000 = -37977 CE.
const int EPOCH_LENGTH = 50000;
const int SUPER_CYCLE_LENGTH = 1300000; // 26 Epochs * 50,000 years
const int EPOCH_START_CE = -37977;

// --- NEW CONSTANT ---
const char * const APP_VERSION = "0.1.0"; // Should match version in meson.build

// --- Function Prototypes ---
void show_help(void);
// NOTE: Added 'int *supercycle_num' to the prototype
int calculate_hts_components(int year_ce, char *epoch_char, int *hts_year, int *hts_day, int *supercycle_num);
void format_output(char *buffer, size_t buffer_size, int hts_year, char epoch_char, int hts_day, struct tm *local_time_info, const char *format_string);

// --- Core HTS Logic ---

/**
 * Calculates HTS components (Supercycle, Epoch, Year) from a Gregorian CE year.
 * Returns 0 on success.
 */
int calculate_hts_components(int year_ce, char *epoch_char, int *hts_year, int *hts_day, int *supercycle_num) {
    long years_since_epoch_start;
    int epoch_index;
    long years_within_supercycle;

    years_since_epoch_start = (long)year_ce - EPOCH_START_CE;

    if (years_since_epoch_start < 0) {
        fprintf(stderr, "Error: Gregorian year is before the HTS start of %d CE.\n", EPOCH_START_CE);
        return -1;
    }

    // 1. Supercycle Calculation
    *supercycle_num = (years_since_epoch_start / SUPER_CYCLE_LENGTH) + 1;

    // Years within the current Supercycle
    years_within_supercycle = years_since_epoch_start % SUPER_CYCLE_LENGTH;

    // 2. Epoch Index Calculation
    epoch_index = years_within_supercycle / EPOCH_LENGTH;

    // Assign the Epoch letter (A-Z cycle)
    *epoch_char = 'A' + (epoch_index % 26);

    // 3. HTS Year Calculation (00000 to 49999)
    *hts_year = years_since_epoch_start % EPOCH_LENGTH;

    return 0;
}

// --- Output Formatting ---

/**
 * Replaces HTS format specifiers (%Z, %E, %Y, %D) and common C time specifiers.
 */
void format_output(char *buffer, size_t buffer_size, int hts_year, char epoch_char, int hts_day, struct tm *local_time_info, const char *format_string) {
    char temp_buffer[buffer_size];
    char *p;
    char *out;

    // Use strftime to handle standard time specifiers (like %A, %r, %H, etc.)
    strftime(temp_buffer, buffer_size, format_string, local_time_info);

    // Manually replace HTS specifiers
    p = temp_buffer;
    out = buffer;

    while (*p && (out - buffer < buffer_size - 1)) {
        if (*p == '%' && *(p + 1)) {
            p++;

            // HTS Specifiers
            if (*p == 'Z') { // HTS Epoch and Year (e.g., A40002)
                out += snprintf(out, buffer_size - (out - buffer), "%c%05d", epoch_char, hts_year);
            } else if (*p == 'E') { // HTS Epoch Letter (e.g., A)
                out += snprintf(out, buffer_size - (out - buffer), "%c", epoch_char);
            } else if (*p == 'Y') { // HTS Year (00000-49999)
                out += snprintf(out, buffer_size - (out - buffer), "%05d", hts_year);
            } else if (*p == 'D') { // HTS Day of Year (001-366)
                out += snprintf(out, buffer_size - (out - buffer), "%03d", hts_day);
            }
            // Copy standard specifiers
            else {
                *out++ = '%';
                *out++ = *p;
            }
        } else {
            *out++ = *p;
        }
        p++;
    }
    *out = '\0'; // Null-terminate the final string
}

// --- Main Program Logic ---

int main(int argc, char *argv[]) {
    int i;
    char *format_string;
    int show_precision;
    int show_supercycle;
    int show_supercycle_num;
    time_t timer;
    struct tm *local_time_info;
    int hts_year;
    char epoch_char;
    int hts_day;
    int supercycle_num;
    char output_buffer[256];

    format_string = NULL;
    show_precision = 0;
    show_supercycle = 0;
    show_supercycle_num = 0;

    // 1. Argument Parsing
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("hts-time version %s\n", APP_VERSION);
            return 0; // Exit immediately
        }
        else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--unix-std") == 0) {
            execlp("date", "date", NULL);
            perror("Failed to execute native 'date' command");
            return 1;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--precision") == 0) {
            show_precision = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--supercycle") == 0) {
            show_supercycle = 1;
        } else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--supercycle-num") == 0) {
            show_supercycle_num = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) {
            if (i + 1 < argc) {
                format_string = argv[i + 1];
                i++;
            }
        }
    }

    // 2. Get Current Time and Calculate HTS Components (Run before output)
    time(&timer);
    local_time_info = localtime(&timer);

    if (local_time_info == NULL) {
        fprintf(stderr, "Error: Could not get local time.\n");
        return 1;
    }

    hts_day = local_time_info->tm_yday + 1;

    // Call the updated function with the new supercycle argument
    if (calculate_hts_components(local_time_info->tm_year + 1900, &epoch_char, &hts_year, &hts_day, &supercycle_num) != 0) {
        return 1;
    }

    // 2.5. Execute Supercycle Number Output (-S)
    if (show_supercycle_num) {
        printf("%d\n", supercycle_num);
        return 0; // Exit immediately
    }

    // 2.6. Execute Verbose Supercycle Output (-s)
    if (show_supercycle) {
        printf("--- HTS Supercycle Information ---\n");
        printf("Current Supercycle Number: %d\n", supercycle_num);
        printf("Current Epoch Letter: %c\n", epoch_char);
        printf("Epoch Start Year (CE): %d\n", EPOCH_START_CE);
        printf("Epoch Length: %d years\n", EPOCH_LENGTH);
        printf("Supercycle Length: %d years\n", SUPER_CYCLE_LENGTH);
        return 0;
    }

    // 3. Output Generation (Original logic)

    if (format_string != NULL) {
        format_output(output_buffer, sizeof(output_buffer), hts_year, epoch_char, hts_day, local_time_info, format_string);
        printf("%s\n", output_buffer);
    } else {
        if (show_precision) {
            // Calculate time of day fraction
            double secs_in_day = (double)local_time_info->tm_hour * 3600.0 +
                                 (double)local_time_info->tm_min * 60.0 +
                                 (double)local_time_info->tm_sec;
            double hts_fractional_year = (double)hts_year + (secs_in_day / 86400.0);

            // Default precision output
            snprintf(output_buffer, sizeof(output_buffer),
                "HTS: %c%05.3f Epoch %c (%d-%02d-%02d %02d:%02d:%02d)",
                epoch_char, hts_fractional_year, epoch_char,
                local_time_info->tm_year + 1900,
                local_time_info->tm_mon + 1,
                local_time_info->tm_mday,
                local_time_info->tm_hour,
                local_time_info->tm_min,
                local_time_info->tm_sec
            );
            printf("%s\n", output_buffer);

        } else {
            // Default clean output
            snprintf(output_buffer, sizeof(output_buffer),
                "HTS: %c%05d Epoch %c (%d-%02d-%02d %02d:%02d:%02d)",
                epoch_char, hts_year, epoch_char,
                local_time_info->tm_year + 1900,
                local_time_info->tm_mon + 1,
                local_time_info->tm_mday,
                local_time_info->tm_hour,
                local_time_info->tm_min,
                local_time_info->tm_sec
            );
            printf("%s\n", output_buffer);
        }
    }

    return 0;
}

// --- Help Message (Updated for new options) ---

void show_help(void) {
    printf("\nhts-time - Storcke Human Time System (HTS) Utility\n");
    printf("Calculates and displays the current date based on the HTS start of %d CE.\n", EPOCH_START_CE);
    printf("\nUSAGE:\n");
    printf("  hts-time [OPTIONS]\n");
    printf("\nOPTIONS:\n");
    printf("  -h, --help            Show this help message.\n");
    printf("  -v, --version         Show the application version and exit.\n");
    printf("  -x, --unix-std        Output the time identically to the native 'date' command.\n");
    printf("  -p, --precision       Display the HTS Year with the fractional time (decimal precision).\n");
    printf("  -s, --supercycle      Display verbose HTS epoch and cycle information.\n");
    printf("  -S, --supercycle-num  Output ONLY the current Supercycle number and exit.\n");
    printf("  -f, --format <FORMAT> Specify a custom output format.\n");
    printf("\nHTS FORMAT SPECIFIERS (Used with -f):\n");
    printf("  %%Z  HTS Epoch and Year (e.g., A40002)\n");
    printf("  %%E  HTS Epoch Letter (e.g., A)\n");
    printf("  %%Y  HTS Year (00000-49999)\n");
    printf("  %%D  HTS Day of Year (001-366)\n");
    printf("\nEXAMPLE:\n");
    printf("  hts-time -f \"It is %%A, HTS Epoch %%Z at %%r.\"\n");
    printf("  hts-time -S\n");
    printf("\n");
}
