/**
 * str_compare.h - String comparison utilities
 *
 * Provides natural sorting (human-friendly alphanumeric ordering)
 * and other string comparison functions.
 */

#ifndef STR_COMPARE_H
#define STR_COMPARE_H

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

#endif // STR_COMPARE_H
