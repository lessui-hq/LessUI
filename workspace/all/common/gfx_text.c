/**
 * gfx_text.c - Text rendering utilities for Launcher
 *
 * Provides text manipulation functions for the graphics system.
 * Extracted from api.c for better testability and reusability.
 */

// Include appropriate headers based on build mode
#ifdef UNIT_TEST_BUILD
// Test mode: include SDL stubs to get TTF_Font typedef
// (sdl_stubs.h available via -I tests/support in test builds)
#include "sdl_stubs.h"
#else
// Production mode: include api.h to get SDL types
#include "api.h"
#endif

#include "gfx_text.h"
#include "log.h"
#include "utils.h"
#include <string.h>

// TTF_SizeUTF8 is provided by SDL_ttf in production
// In tests, it's mocked with fff (declared in sdl_fakes.h)
#ifndef UNIT_TEST_BUILD
extern int TTF_SizeUTF8(TTF_Font* font, const char* text, int* w, int* h);
#endif

// Constants - typically defined in platform headers
#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#ifndef MAX_TEXT_LINES
#define MAX_TEXT_LINES 16
#endif

/**
 * Truncates text to fit within a maximum width.
 *
 * If the text (plus padding) exceeds max_width, characters are removed from
 * the end and replaced with "..." until it fits.
 *
 * @param font TTF font used to measure text
 * @param in_name Input text to truncate
 * @param out_name Output buffer for truncated text (min 256 bytes)
 * @param max_width Maximum width in pixels
 * @param padding Additional padding to account for in pixels
 * @return Final width of truncated text including padding
 */
int GFX_truncateText(TTF_Font* ttf_font, const char* in_name, char* out_name, int max_width,
                     int padding) {
	int text_width;
	int in_len = strlen(in_name);
	safe_strcpy(out_name, in_name, 256);

	TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
	text_width += padding;

	// Already fits - no truncation needed
	if (text_width <= max_width) {
		return text_width;
	}

	// Too short to truncate meaningfully
	if (in_len <= 4) {
		safe_strcpy(out_name, "...", 256);
		TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
		return text_width + padding;
	}

	// Binary search for the right truncation length
	// We need to find the longest prefix that fits when "..." is appended
	int lo = 1; // Minimum: at least 1 char + "..."
	int hi = in_len - 3; // Maximum: all but last 3 chars (room for "...")
	int best_len = 1; // Best fitting length found

	while (lo <= hi) {
		int mid = (lo + hi) / 2;

		// Build truncated string: first 'mid' chars + "..."
		memcpy(out_name, in_name, mid);
		memcpy(out_name + mid, "...", 4); // 3 chars + null terminator

		TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
		text_width += padding;

		if (text_width <= max_width) {
			// Fits - try longer
			best_len = mid;
			lo = mid + 1;
		} else {
			// Too wide - try shorter
			hi = mid - 1;
		}
	}

	// Build final result with best length
	memcpy(out_name, in_name, best_len);
	memcpy(out_name + best_len, "...", 4); // 3 chars + null terminator

	TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
	text_width += padding;

	return text_width;
}

/**
 * Tries to fit part of a word on the current line using truncation.
 *
 * @param ttf_font Font for measuring
 * @param word_start Start of word to truncate
 * @param word_end End of word (space or null)
 * @param remaining_space Space available on current line
 * @return Width of truncated word, or -1 if truncation not beneficial
 */
static int try_partial_word_fit(TTF_Font* ttf_font, char* word_start, char* word_end,
                                int remaining_space) {
	// Only try partial fit if we have meaningful space (>= 4 chars for "x...")
	if (remaining_space < 40) // ~4 chars at typical font sizes
		return -1;

	char truncated[MAX_PATH];
	char saved = *word_end;
	*word_end = '\0';
	GFX_truncateText(ttf_font, word_start, truncated, remaining_space, 0);
	*word_end = saved;

	// If truncation actually shortened the word (not just the same word)
	if (strlen(truncated) >= (size_t)(word_end - word_start))
		return -1;

	// Copy truncated word back, preserving space for remaining chars
	size_t trunc_len = strlen(truncated);
	memmove(word_start + trunc_len, word_end, strlen(word_end) + 1);
	memcpy(word_start, truncated, trunc_len);

	// Measure the truncated portion
	int trunc_width;
	TTF_SizeUTF8(ttf_font, truncated, &trunc_width, NULL);

	return trunc_width;
}

/**
 * Wraps to a new line before a word by converting the preceding space to newline.
 *
 * @param str Base of string (for bounds checking)
 * @param word_start Start of word to wrap before
 */
static void wrap_before_word(char* str, char* word_start) {
	if (word_start > str && *(word_start - 1) == ' ') {
		*(word_start - 1) = '\n';
	}
}

/**
 * Wraps text to fit within a maximum width by inserting newlines.
 *
 * Breaks text at space characters to create wrapped lines. Preserves
 * existing newlines (intentional line breaks). Each line segment is
 * wrapped independently. Lines that can't wrap (no spaces) are truncated
 * with "...".
 *
 * @param font TTF font to measure text with
 * @param str String to wrap (modified in place)
 * @param max_width Maximum width per line in pixels
 * @param max_lines Maximum number of lines (0 for unlimited)
 * @return Width of the widest line in pixels
 *
 * @note Input string is modified - spaces become newlines at wrap points
 */
int GFX_wrapText(TTF_Font* ttf_font, char* str, int max_width, int max_lines) {
	if (!str || !str[0] || max_width <= 0)
		return 0;

	// Get space width once for accumulating line widths
	int space_width;
	TTF_SizeUTF8(ttf_font, " ", &space_width, NULL);

	int max_line_width = 0;
	int lines = 1;
	int line_width = 0;
	char* line_start = str;
	char* word_start = str;
	char* p = str;

	while (*p) {
		// Hit existing newline - finalize this line
		if (*p == '\n') {
			if (line_width > max_line_width)
				max_line_width = line_width;
			line_width = 0;
			line_start = p + 1;
			word_start = p + 1;
			lines++;
			p++;
			continue;
		}

		// Hit space - end of word
		if (*p == ' ') {
			// Measure the word we just finished (word_start to p)
			if (p > word_start) {
				char saved = *p;
				*p = '\0';
				int word_width;
				TTF_SizeUTF8(ttf_font, word_start, &word_width, NULL);
				*p = saved;

				// Calculate width if we add this word
				int new_width =
				    (line_width == 0) ? word_width : line_width + space_width + word_width;

				if (new_width > max_width && line_width > 0) {
					// Word doesn't fit - try to fit part of it to maximize line usage
					int remaining_space = max_width - line_width - space_width;
					int trunc_width =
					    try_partial_word_fit(ttf_font, word_start, p, remaining_space);

					if (trunc_width >= 0) {
						// Partial fit succeeded
						p = word_start + strlen(word_start);
						line_width = line_width + space_width + trunc_width;
						if (line_width > max_line_width)
							max_line_width = line_width;

						// Start new line after truncation
						if (max_lines && lines >= max_lines)
							break;
						*p = '\n';
						line_start = p + 1;
						word_start = p + 1;
						line_width = 0;
						lines++;
						p++;
						continue;
					}

					// Can't fit partial word - wrap before it normally
					if (max_lines && lines >= max_lines)
						break;
					wrap_before_word(str, word_start);
					if (line_width > max_line_width)
						max_line_width = line_width;
					line_start = word_start;
					line_width = word_width;
					lines++;
				} else {
					line_width = new_width;
				}
			}
			word_start = p + 1;
			p++;
			continue;
		}

		p++;
	}

	// Handle final word (no trailing space)
	if (p > word_start) {
		int word_width;
		TTF_SizeUTF8(ttf_font, word_start, &word_width, NULL);

		int new_width = (line_width == 0) ? word_width : line_width + space_width + word_width;

		if (new_width > max_width && line_width > 0) {
			// Final word doesn't fit - wrap before it
			if (!(max_lines && lines >= max_lines)) {
				wrap_before_word(str, word_start);
				if (line_width > max_line_width)
					max_line_width = line_width;
				line_start = word_start;
				line_width = word_width;
				lines++;
			}
		} else {
			line_width = new_width;
		}
	}

	if (line_width > max_line_width)
		max_line_width = line_width;

	// Truncate final line if it's too long (single word longer than max_width)
	if (*line_start) {
		int w;
		TTF_SizeUTF8(ttf_font, line_start, &w, NULL);
		if (w > max_width) {
			char buffer[MAX_PATH];
			GFX_truncateText(ttf_font, line_start, buffer, max_width, 0);
			size_t remaining = strlen(str) - (line_start - str) + 1;
			safe_strcpy(line_start, buffer, remaining);
			TTF_SizeUTF8(ttf_font, line_start, &w, NULL);
		}
		if (w > max_line_width)
			max_line_width = w;
	}

	return max_line_width;
}

/**
 * Calculates the bounding box size of multi-line text.
 *
 * Measures the width and height needed to render text that may contain
 * newlines. Width is set to the widest line, height is line count * leading.
 *
 * @param font TTF font to measure text with
 * @param str Text to measure (may contain \n for newlines)
 * @param leading Line spacing in pixels (line height)
 * @param w Output: Width of widest line in pixels
 * @param h Output: Total height (line count * leading)
 */
void GFX_sizeText(TTF_Font* ttf_font, char* str, int leading, int* w, int* h) {
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	*h = count * leading;

	int mw = 0;
	char line[256];
	for (int i = 0; i < count; i++) {
		int len;
		if (i + 1 < count) {
			len = lines[i + 1] - lines[i] - 1;
			if (len)
				strncpy(line, lines[i], len);
			line[len] = '\0';
		} else {
			len = strlen(lines[i]);
			safe_strcpy(line, lines[i], sizeof(line));
		}

		if (len) {
			int lw;
			TTF_SizeUTF8(ttf_font, line, &lw, NULL);
			if (lw > mw)
				mw = lw;
		}
	}
	*w = mw;
}
