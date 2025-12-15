/**
 * gfx_text.c - Text rendering utilities for MinUI
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
	safe_strcpy(out_name, in_name, 256);
	TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
	text_width += padding;

	while (text_width > max_width) {
		int len = strlen(out_name);
		// Need at least 4 chars to truncate (replace last char with "...")
		// If string is too short, just use "..." directly
		if (len <= 4) {
			safe_strcpy(out_name, "...", 256);
			TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
			text_width += padding;
			break;
		}
		safe_strcpy(&out_name[len - 4], "...", 4);
		TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
		text_width += padding;
	}

	return text_width;
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

	int max_line_width = 0;
	int lines = 1;
	char* line_start = str; // Start of current line being measured
	char* last_space = NULL; // Last space we could wrap at
	char* p = str;

	while (*p) {
		// Hit existing newline - reset for next line
		if (*p == '\n') {
			// Measure this line segment
			char saved = *p;
			*p = '\0';
			int w;
			TTF_SizeUTF8(ttf_font, line_start, &w, NULL);
			*p = saved;
			if (w > max_line_width)
				max_line_width = w;
			line_start = p + 1;
			last_space = NULL;
			lines++;
			p++;
			continue;
		}

		// Track spaces as potential wrap points
		if (*p == ' ')
			last_space = p;

		// Measure current line (up to and including current char)
		char saved = *(p + 1);
		*(p + 1) = '\0';
		int line_width;
		TTF_SizeUTF8(ttf_font, line_start, &line_width, NULL);
		*(p + 1) = saved;

		// Line too long - wrap at last space if possible
		if (line_width > max_width) {
			if (last_space) {
				// Wrap at the last space
				if (max_lines && lines >= max_lines)
					break;
				*last_space = '\n';
				line_start = last_space + 1;
				last_space = NULL;
				lines++;
				// Reset p to scan the new line from the start, so we don't miss spaces
				p = line_start;
				continue;
			}
			// If no space to wrap at, we'll truncate at the end
		}

		if (line_width > max_line_width)
			max_line_width = line_width;

		p++;
	}

	// Truncate final line if it's too long
	if (*line_start) {
		int w;
		TTF_SizeUTF8(ttf_font, line_start, &w, NULL);
		if (w > max_width) {
			// Use GFX_truncateText to truncate with "..."
			char buffer[MAX_PATH];
			GFX_truncateText(ttf_font, line_start, buffer, max_width, 0);
			// Calculate remaining space in the buffer from line_start
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
