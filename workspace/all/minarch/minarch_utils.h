/**
 * minarch_utils.h - Pure utility functions extracted from minarch.c
 *
 * These functions have no external dependencies and can be tested in isolation.
 * They perform string manipulation and other pure computations.
 *
 * For option-related functions, see minarch_options.h
 * For CPU frequency functions, see minarch_cpu.h
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
 *   MinArchUtils_getCoreName("fceumm_libretro.so", buf) -> "fceumm"
 *   MinArchUtils_getCoreName("gambatte_libretro.so", buf) -> "gambatte"
 *   MinArchUtils_getCoreName("pcsx_rearmed_libretro.so", buf) -> "pcsx_rearmed"
 */
void MinArchUtils_getCoreName(const char* in_name, char* out_name);

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
 *   MinArchUtils_replaceString(buf, "world", "there") -> 1, buf = "hello there"
 */
int MinArchUtils_replaceString(char* line, const char* search, const char* replace);

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
 *   MinArchUtils_escapeSingleQuotes(buf) -> "it'\''s cool"
 */
char* MinArchUtils_escapeSingleQuotes(char* str);

#endif /* MINARCH_UTILS_H */
