/**
 * launcher_str_compare.h - String comparison utilities for Launcher
 *
 * Provides natural sorting (human-friendly alphanumeric ordering)
 * and other string comparison functions used by the launcher.
 */

#ifndef LAUNCHER_STR_COMPARE_H
#define LAUNCHER_STR_COMPARE_H

/**
 * Skips leading article ("The ", "A ", "An ") for sorting purposes.
 *
 * No-Intro convention moves articles to end for sorting, so
 * "The Legend of Zelda" sorts under "L", not "T".
 *
 * @param s String to check
 * @return Pointer past the article, or original pointer if no article
 */
const char* skip_article(const char* s);

/**
 * Natural string comparison (case-insensitive).
 *
 * Compares strings in a human-friendly way where numeric sequences
 * are compared by their numeric value rather than lexicographically.
 *
 * Also skips leading articles ("The ", "A ", "An ") so that
 * "The Legend of Zelda" sorts under "L", not "T". This matches
 * the No-Intro naming convention.
 *
 * Examples:
 *   "Game 2" < "Game 10"   (unlike strcmp where "Game 10" < "Game 2")
 *   "a1b" < "a2b" < "a10b"
 *   "The Legend of Zelda" sorts with "Legend..." not "The..."
 *   "A Link to the Past" sorts with "Link..." not "A..."
 *
 * @param s1 First string to compare
 * @param s2 Second string to compare
 * @return Negative if s1 < s2, 0 if equal, positive if s1 > s2
 */
int strnatcasecmp(const char* s1, const char* s2);

/**
 * Natural string comparison for pre-sorted keys.
 *
 * Use this when comparing strings that have already had articles stripped
 * (e.g., Entry->sort_key). Avoids redundant article stripping that would
 * otherwise happen in strnatcasecmp().
 *
 * For a directory with 5000 entries, sorting requires ~60k comparisons.
 * Using this function instead of strnatcasecmp() eliminates ~120k
 * unnecessary skip_article() calls.
 *
 * @param s1 First pre-sorted string
 * @param s2 Second pre-sorted string
 * @return Negative if s1 < s2, 0 if equal, positive if s1 > s2
 */
int strnatcasecmp_presorted(const char* s1, const char* s2);

#endif // LAUNCHER_STR_COMPARE_H
