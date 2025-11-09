/**
 * gfx_text.h - Text rendering utilities for MinUI
 *
 * Provides text manipulation functions for the graphics system:
 * - Text truncation with ellipsis
 * - Multi-line text wrapping
 *
 * These are pure logic functions extracted from api.c for testability.
 * They depend only on TTF_SizeUTF8 for measuring text width.
 */

#ifndef __GFX_TEXT_H__
#define __GFX_TEXT_H__

// TTF_Font type must be defined before including this header:
// - Production: api.h includes SDL_ttf.h then this header
// - Tests: gfx_text.c includes sdl_stubs.h when SDL_STUBS_H is defined

/**
 * Truncates text to fit within a maximum width.
 *
 * If the text (plus padding) exceeds max_width, characters are removed from
 * the end and replaced with "..." until it fits. The output string is modified
 * in place.
 *
 * Example:
 *   "The Legend of Zelda" -> "The Legend..."
 *
 * @param font TTF font used to measure text
 * @param in_name Input text to truncate
 * @param out_name Output buffer for truncated text (min 256 bytes)
 * @param max_width Maximum width in pixels
 * @param padding Additional padding to account for in pixels
 * @return Final width of truncated text including padding
 *
 * @note out_name buffer must have space for at least "..." (4 bytes including null)
 */
int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width,
                     int padding);

/**
 * Wraps text to fit within a maximum width by inserting newlines.
 *
 * Breaks text at space characters to create wrapped lines. The last
 * line is truncated with "..." if it still exceeds max_width.
 * Modifies the input string in place by replacing spaces with newlines.
 *
 * Example:
 *   "The quick brown fox" -> "The quick\nbrown fox"
 *
 * @param font TTF font to measure text with
 * @param str String to wrap (modified in place)
 * @param max_width Maximum width per line in pixels
 * @param max_lines Maximum number of lines (0 for unlimited)
 * @return Width of the widest line in pixels
 *
 * @warning Input string is modified - spaces become newlines at wrap points
 * @note Requires MAX_PATH to be defined (typically 512)
 */
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines);

/**
 * Calculates the bounding box size of multi-line text.
 *
 * Measures the width and height needed to render text that may contain
 * newlines. Width is set to the widest line, height is line count * leading.
 *
 * Example:
 *   "Line 1\nLonger Line 2\nLine 3" with leading=16
 *   -> w = width of "Longer Line 2", h = 48 (3 lines * 16px)
 *
 * @param font TTF font to measure text with
 * @param str Text to measure (may contain \n for newlines)
 * @param leading Line spacing in pixels (line height)
 * @param w Output: Width of widest line in pixels
 * @param h Output: Total height (line count * leading)
 *
 * @note Maximum 16 lines supported (MAX_TEXT_LINES)
 */
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h);

#endif // __GFX_TEXT_H__
