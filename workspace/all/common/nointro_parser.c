/**
 * nointro_parser.c - Parser for No-Intro ROM naming convention
 *
 * Implementation approach:
 * 1. Use tiny-regex-c for pattern matching (lightweight, no malloc)
 * 2. Parse tags from right to left (status flags first, then metadata)
 * 3. Handle article rearrangement after all tags are stripped
 * 4. Preserve title integrity (only strip recognized tag patterns)
 *
 * Tag parsing order (right to left):
 * 1. [status] - Square brackets at end (e.g., [b], [!])
 * 2. (license) - (Unl) for unlicensed
 * 3. (special) - Special flags (ST), (MB), etc.
 * 4. (additional) - Additional info (Disc 1), (Rumble Version)
 * 5. (dev_status) - (Beta), (Proto), (Sample)
 * 6. (version) - (v1.0), (Rev A)
 * 7. (language) - (En), (En,Ja,Fr)
 * 8. (region) - (USA), (Japan, USA), (World)
 *
 * Regex patterns needed:
 * - Region: \([A-Z][a-z]+(?:, [A-Z][a-z]+)*\)
 * - Language: \([A-Z][a-z](?:,[A-Z][a-z])*\)
 * - Version: \((?:v[0-9.]+|Rev [A-Z0-9]+)\)
 * - Dev status: \(Beta(?: [0-9]+)?|Proto|Sample)\)
 * - Status: \[[a-z!]\]
 */

#include "nointro_parser.h"
#include "utils.h"
#include <ctype.h>
#include <string.h>

void initNoIntroName(NoIntroName* parsed) {
	if (!parsed)
		return;

	parsed->title[0] = '\0';
	parsed->display_name[0] = '\0';
	parsed->region[0] = '\0';
	parsed->language[0] = '\0';
	parsed->version[0] = '\0';
	parsed->dev_status[0] = '\0';
	parsed->additional[0] = '\0';
	parsed->special[0] = '\0';
	parsed->license[0] = '\0';
	parsed->status[0] = '\0';
	parsed->has_tags = false;
}

/**
 * Removes file extension from filename.
 *
 * Handles multi-part extensions like .p8.png
 * Only removes extensions between 1-4 characters (plus dot)
 *
 * @param filename Input filename
 * @param out Output buffer (must be at least NOINTRO_MAX_TITLE)
 */
static void removeExtension(const char* filename, char* out) {
	char* tmp;
	const char* start = filename;

	// Extract just the filename if we have a full path
	tmp = strrchr(filename, '/');
	if (tmp) {
		start = tmp + 1;
	}

	// Copy filename (bounds-checked)
	strncpy(out, start, NOINTRO_MAX_TITLE - 1);
	out[NOINTRO_MAX_TITLE - 1] = '\0';

	// Remove file extension (only if it comes AFTER all tags)
	// Extensions appear at the very end: "Name (tags).ext"
	// We need to find the rightmost ')' or ']', then remove extension after it
	char* last_paren = strrchr(out, ')');
	char* last_bracket = strrchr(out, ']');
	char* last_tag = (last_paren > last_bracket) ? last_paren : last_bracket;

	// Find last dot (potential extension)
	tmp = strrchr(out, '.');

	// Only remove if the dot is after all tags (or there are no tags)
	if (tmp && (!last_tag || tmp > last_tag)) {
		int len = strlen(tmp);
		// Remove extensions 1-5 chars (covers .gb, .zip, .p8.png, .doom, etc.)
		if (len > 1 && len <= 6) {
			tmp[0] = '\0';
			// Check for multi-part extensions like .p8.png
			tmp = strrchr(out, '.');
			if (tmp && (!last_tag || tmp > last_tag)) {
				len = strlen(tmp);
				if (len > 1 && len <= 6) {
					tmp[0] = '\0';
				}
			}
		}
	}
}

/**
 * Extracts and removes a tag at the specified position.
 *
 * @param str String to extract from (modified in place)
 * @param open Opening character ('(' or '[')
 * @param close Closing character (')' or ']')
 * @param out Output buffer for extracted content (without brackets)
 * @param out_size Size of output buffer
 * @return true if tag was found and extracted, false otherwise
 */
static bool extractTag(char* str, char open, char close, char* out, size_t out_size) {
	char* close_pos = strrchr(str, close);
	if (!close_pos)
		return false;

	// Find matching opening bracket
	char* open_pos = close_pos - 1;
	while (open_pos > str && *open_pos != open) {
		open_pos--;
	}

	if (*open_pos != open)
		return false;

	// Extract content (without brackets)
	size_t content_len = close_pos - open_pos - 1;
	if (content_len >= out_size)
		content_len = out_size - 1;

	// Use memcpy to avoid strncpy warnings with zero-length or compile-time constant lengths
	if (content_len > 0)
		memcpy(out, open_pos + 1, content_len);
	out[content_len] = '\0';

	// Remove the entire tag from string (including brackets)
	// Trim any trailing whitespace before the tag
	char* trim_pos = open_pos;
	while (trim_pos > str && isspace((unsigned char)*(trim_pos - 1))) {
		trim_pos--;
	}
	*trim_pos = '\0';

	return true;
}

/**
 * TODO: Implement tag classification using regex patterns.
 *
 * For now, we'll use a simplified heuristic approach:
 * - Check for known patterns in order
 * - Use string matching for common cases
 *
 * This will be replaced with tiny-regex-c implementation.
 */
static void classifyTag(const char* tag, NoIntroName* parsed) {
	// Status flags (single character)
	if (strlen(tag) == 1) {
		strcpy(parsed->status, tag);
		parsed->has_tags = true;
		return;
	}

	// License (Unl)
	if (strcmp(tag, "Unl") == 0) {
		strcpy(parsed->license, tag);
		parsed->has_tags = true;
		return;
	}

	// Development status
	if (strstr(tag, "Beta") || strstr(tag, "Proto") || strstr(tag, "Sample")) {
		strcpy(parsed->dev_status, tag);
		parsed->has_tags = true;
		return;
	}

	// Version (starts with 'v' followed by digit, or 'Rev')
	if ((tag[0] == 'v' && strlen(tag) > 1 && isdigit((unsigned char)tag[1])) ||
	    prefixMatch("Rev ", (char*)tag)) {
		strcpy(parsed->version, tag);
		parsed->has_tags = true;
		return;
	}

	// Language (contains comma-separated two-letter codes)
	// e.g., "En", "En,Ja", "Fr,De,Es"
	if (strlen(tag) == 2 || (strlen(tag) > 2 && strchr(tag, ','))) {
		bool looks_like_lang = true;
		const char* p = tag;
		while (*p) {
			if (!(isupper(*p) && islower(*(p + 1)))) {
				if (*p != ',') {
					looks_like_lang = false;
					break;
				}
			}
			p += (*p == ',') ? 1 : 2;
		}
		if (looks_like_lang && parsed->language[0] == '\0') {
			strcpy(parsed->language, tag);
			parsed->has_tags = true;
			return;
		}
	}

	// Region (if not yet set and contains known region names)
	// Common regions: USA, Japan, Europe, World, Asia, Korea, etc.
	if (parsed->region[0] == '\0') {
		if (strstr(tag, "USA") || strstr(tag, "Japan") || strstr(tag, "Europe") ||
		    strstr(tag, "World") || strstr(tag, "Asia") || strstr(tag, "Korea") ||
		    strstr(tag, "China") || strstr(tag, "Australia") || strstr(tag, "Brazil") ||
		    strstr(tag, "Canada") || strstr(tag, "France") || strstr(tag, "Germany") ||
		    strstr(tag, "Spain") || strstr(tag, "Italy")) {
			strcpy(parsed->region, tag);
			parsed->has_tags = true;
			return;
		}
	}

	// Default: additional info (Disc 1, Rumble Version, etc.)
	if (parsed->additional[0] == '\0') {
		strcpy(parsed->additional, tag);
		parsed->has_tags = true;
	}
}

void parseNoIntroName(const char* filename, NoIntroName* parsed) {
	char work[NOINTRO_MAX_TITLE];
	char tag[NOINTRO_MAX_FIELD];

	initNoIntroName(parsed);

	// Remove file extension
	removeExtension(filename, work);

	// Extract tags from right to left
	// First, status flags in square brackets
	while (extractTag(work, '[', ']', tag, sizeof(tag))) {
		classifyTag(tag, parsed);
	}

	// Then, metadata in parentheses
	while (extractTag(work, '(', ')', tag, sizeof(tag))) {
		classifyTag(tag, parsed);
	}

	// What's left is the title
	strcpy(parsed->title, work);

	// Create display name by moving article to front
	strcpy(parsed->display_name, work);
	fixArticle(parsed->display_name);
}
