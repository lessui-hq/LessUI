/**
 * minarch_utils.h - Pure utility functions extracted from minarch.c
 *
 * These functions have no external dependencies and can be tested in isolation.
 * They perform string manipulation, option searching, and other pure computations.
 */

#ifndef MINARCH_UTILS_H
#define MINARCH_UTILS_H

/**
 * Extracts core name from a libretro core filename.
 *
 * Core files are named like "core_libretro.so" - this extracts "core".
 * The output buffer must be large enough to hold the core name.
 *
 * @param in_name Input filename (e.g., "fceumm_libretro.so")
 * @param out_name Output buffer for core name (e.g., "fceumm")
 *
 * @example
 *   MinArch_getCoreName("fceumm_libretro.so", buf) -> "fceumm"
 *   MinArch_getCoreName("gambatte_libretro.so", buf) -> "gambatte"
 *   MinArch_getCoreName("pcsx_rearmed_libretro.so", buf) -> "pcsx_rearmed"
 */
void MinArch_getCoreName(const char* in_name, char* out_name);

/**
 * Option value structure for option list searching.
 *
 * Represents a single option with multiple possible values.
 */
typedef struct {
	const char* key; // Option key (e.g., "video_scale")
	const char** values; // Array of possible values
	int count; // Number of values
	int value; // Current value index
} MinArchOption;

/**
 * Finds the index of a value in an option's value list.
 *
 * Searches the option's values array for a matching string.
 * Returns 0 (default) if value is NULL or not found.
 *
 * @param opt Option to search
 * @param value Value string to find
 * @return Index of value in values array, or 0 if not found
 *
 * @example
 *   // Given option with values ["1x", "2x", "3x"]
 *   MinArch_getOptionValueIndex(opt, "2x") -> 1
 *   MinArch_getOptionValueIndex(opt, "4x") -> 0 (not found)
 *   MinArch_getOptionValueIndex(opt, NULL) -> 0
 */
int MinArch_getOptionValueIndex(const MinArchOption* opt, const char* value);

/**
 * Finds the nearest frequency index in a sorted frequency array.
 *
 * Used by auto CPU scaling to find the closest available frequency
 * to a target value. Assumes frequencies are sorted ascending.
 *
 * @param frequencies Array of available frequencies (kHz)
 * @param count Number of frequencies in array
 * @param target_khz Target frequency to find
 * @return Index of nearest frequency, or 0 if count <= 0
 *
 * @example
 *   // Given frequencies [600000, 800000, 1000000, 1200000]
 *   MinArch_findNearestFrequency(freq, 4, 750000) -> 1 (800000)
 *   MinArch_findNearestFrequency(freq, 4, 1100000) -> 2 (1000000)
 */
int MinArch_findNearestFrequency(const int* frequencies, int count, int target_khz);

/**
 * Performs string replacement in place (recursive).
 *
 * Replaces all occurrences of search with replace in line.
 * The line buffer must be large enough to hold the result.
 *
 * @param line String to modify (modified in place)
 * @param search Substring to find
 * @param replace Replacement string
 * @return Number of replacements made
 *
 * @warning Buffer must be large enough for expansions
 *
 * @example
 *   char buf[256] = "hello world";
 *   MinArch_replaceString(buf, "world", "there") -> 1, buf = "hello there"
 */
int MinArch_replaceString(char* line, const char* search, const char* replace);

/**
 * Escapes single quotes for shell command safety.
 *
 * Replaces ' with '\'' which safely handles quotes in bash strings.
 *
 * @param str String to escape (modified in place)
 * @return The modified string (same pointer as input)
 *
 * @example
 *   char buf[256] = "it's cool";
 *   MinArch_escapeSingleQuotes(buf) -> "it'\''s cool"
 */
char* MinArch_escapeSingleQuotes(char* str);

#endif /* MINARCH_UTILS_H */
