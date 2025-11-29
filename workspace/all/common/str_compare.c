/**
 * str_compare.c - String comparison utilities
 *
 * Provides natural sorting (human-friendly alphanumeric ordering).
 */

#include "str_compare.h"
#include <ctype.h>
#include <string.h>

// Articles to skip for sorting (order matters: "An " before "A ")
static const char* articles[] = {"The ", "An ", "A ", NULL};

/**
 * Skips leading article ("The ", "A ", "An ") for sorting purposes.
 *
 * No-Intro convention moves articles to end for sorting, so
 * "The Legend of Zelda" sorts under "L", not "T".
 *
 * @param s String to check
 * @return Pointer past the article, or original pointer if no article
 */
const char* skip_article(const char* s) {
	if (!s)
		return s;

	for (int i = 0; articles[i]; i++) {
		const char* article = articles[i];
		const char* p = s;

		// Case-insensitive prefix match
		while (*article && tolower((unsigned char)*p) == tolower((unsigned char)*article)) {
			p++;
			article++;
		}

		// If we matched the whole article, skip it
		if (!*article)
			return p;
	}

	return s;
}

/**
 * Natural string comparison (case-insensitive).
 *
 * Algorithm:
 * 1. Skip leading articles ("The ", "A ", "An ") for sorting
 * 2. Skip leading zeros in numeric sequences
 * 3. Compare digit sequences by length first (longer = larger)
 * 4. If same length, compare digit by digit
 * 5. Non-numeric characters compared case-insensitively
 */
int strnatcasecmp(const char* s1, const char* s2) {
	if (!s1 && !s2)
		return 0;
	if (!s1)
		return -1;
	if (!s2)
		return 1;

	// Skip leading articles for sorting
	s1 = skip_article(s1);
	s2 = skip_article(s2);

	while (*s1 && *s2) {
		// Both are digits - compare as numbers
		if (isdigit((unsigned char)*s1) && isdigit((unsigned char)*s2)) {
			// Skip leading zeros
			while (*s1 == '0')
				s1++;
			while (*s2 == '0')
				s2++;

			// Count digit lengths
			const char* n1 = s1;
			const char* n2 = s2;
			while (isdigit((unsigned char)*n1))
				n1++;
			while (isdigit((unsigned char)*n2))
				n2++;

			int len1 = n1 - s1;
			int len2 = n2 - s2;

			// Longer number is greater
			if (len1 != len2)
				return len1 - len2;

			// Same length - compare digit by digit
			while (s1 < n1) {
				if (*s1 != *s2)
					return *s1 - *s2;
				s1++;
				s2++;
			}
			// Numbers are equal, continue with rest of string
			continue;
		}

		// Compare characters case-insensitively
		int c1 = tolower((unsigned char)*s1);
		int c2 = tolower((unsigned char)*s2);

		if (c1 != c2)
			return c1 - c2;

		s1++;
		s2++;
	}

	// Handle end of strings
	if (*s1)
		return 1;
	if (*s2)
		return -1;
	return 0;
}
