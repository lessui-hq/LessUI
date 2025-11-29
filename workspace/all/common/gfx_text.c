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
	strcpy(out_name, in_name);
	TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
	text_width += padding;

	while (text_width > max_width) {
		int len = strlen(out_name);
		// Need at least 4 chars to truncate (replace last char with "...")
		// If string is too short, just use "..." directly
		if (len <= 4) {
			strcpy(out_name, "...");
			TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
			text_width += padding;
			break;
		}
		strcpy(&out_name[len - 4], "...");
		TTF_SizeUTF8(ttf_font, out_name, &text_width, NULL);
		text_width += padding;
	}

	return text_width;
}

/**
 * Wraps text to fit within a maximum width by inserting newlines.
 *
 * Breaks text at space characters to create wrapped lines. The last
 * line is truncated with "..." if it still exceeds max_width.
 * Modifies the input string in place by replacing spaces with newlines.
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
	if (!str)
		return 0;

	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[MAX_PATH];

	TTF_SizeUTF8(ttf_font, line, &line_width, NULL);
	if (line_width <= max_width) {
		line_width = GFX_truncateText(ttf_font, line, buffer, max_width, 0);
		strcpy(line, buffer);
		return line_width;
	}

	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	while (!max_lines || lines < max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				TTF_SizeUTF8(ttf_font, line, &line_width, NULL);
				if (line_width >= max_width) {
					if (line_width > max_line_width)
						max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			}
			break;
		}
		tmp[0] = '\0';

		TTF_SizeUTF8(ttf_font, line, &line_width, NULL);

		if (line_width >= max_width) { // wrap
			if (line_width > max_line_width)
				max_line_width = line_width;
			tmp[0] = ' ';
			tmp += 1;
			if (prev) {
				prev[0] = '\n';
				prev += 1;
				line = prev;
			} else {
				// TODO: If first word is too long (no prev space), it gets skipped.
				// Should truncate with "..." instead of dropping it entirely.
				line = tmp;
			}
			lines += 1;
		} else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
	}

	line_width = GFX_truncateText(ttf_font, line, buffer, max_width, 0);
	strcpy(line, buffer);

	if (line_width > max_line_width)
		max_line_width = line_width;
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
			strcpy(line, lines[i]);
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
