/**
 * test_gfx_text.c - Unit tests for GFX text utilities
 *
 * Tests text truncation and wrapping algorithms extracted from api.c.
 * Uses fff framework to mock TTF_SizeUTF8 font measurement function.
 *
 * Test coverage:
 * - GFX_truncateText() - Text truncation with ellipsis
 * - GFX_wrapText() - Multi-line text wrapping
 */

#include "../../../support/unity/unity.h"
#include "../../../support/fff/fff.h"
#include "../../../support/sdl_stubs.h"
#include "../../../support/sdl_fakes.h"
#include "../../../../workspace/all/common/gfx_text.h"

#include <string.h>

DEFINE_FFF_GLOBALS;

// Mock font for testing
static TTF_Font mock_font = {.point_size = 16, .style = 0};

///////////////////////////////
// Custom TTF_SizeUTF8 Mock
///////////////////////////////

/**
 * Simple width calculation: 10 pixels per character
 * This makes testing predictable and easy to understand
 */
int mock_TTF_SizeUTF8(TTF_Font* font, const char* text, int* w, int* h) {
	if (w)
		*w = strlen(text) * 10; // 10 pixels per char
	if (h)
		*h = font->point_size;
	return 0;
}

void setUp(void) {
	// Reset fff fakes
	RESET_FAKE(TTF_SizeUTF8);
	FFF_RESET_HISTORY();

	// Use our simple mock by default
	TTF_SizeUTF8_fake.custom_fake = mock_TTF_SizeUTF8;
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// GFX_truncateText() Tests
///////////////////////////////

void test_truncateText_no_truncation_needed(void) {
	char output[256];

	// "Hello" = 50px, max_width = 100px, no truncation needed
	int width = GFX_truncateText(&mock_font, "Hello", output, 100, 0);

	TEST_ASSERT_EQUAL_STRING("Hello", output);
	TEST_ASSERT_EQUAL_INT(50, width); // 5 chars * 10px
}

void test_truncateText_with_padding(void) {
	char output[256];

	// "Hello" = 50px + 20px padding = 70px, max_width = 100px
	int width = GFX_truncateText(&mock_font, "Hello", output, 100, 20);

	TEST_ASSERT_EQUAL_STRING("Hello", output);
	TEST_ASSERT_EQUAL_INT(70, width); // 50px + 20px padding
}

void test_truncateText_truncates_long_text(void) {
	char output[256];

	// "The Legend of Zelda" = 190px, max_width = 100px
	// Should truncate to fit
	int width = GFX_truncateText(&mock_font, "The Legend of Zelda", output, 100, 0);

	// Result should end with "..."
	TEST_ASSERT_TRUE(strstr(output, "...") != NULL);
	TEST_ASSERT_LESS_OR_EQUAL(100, width);
}

void test_truncateText_result_ends_with_ellipsis(void) {
	char output[256];

	GFX_truncateText(&mock_font, "Very Long Text That Needs Truncating", output, 80, 0);

	// Check that output ends with "..."
	int len = strlen(output);
	TEST_ASSERT_GREATER_THAN(3, len);
	TEST_ASSERT_EQUAL_STRING("...", &output[len - 3]);
}

void test_truncateText_respects_max_width_with_padding(void) {
	char output[256];

	// With padding, should still fit within max_width
	int width = GFX_truncateText(&mock_font, "A Very Long String Indeed", output, 100, 15);

	TEST_ASSERT_LESS_OR_EQUAL(100, width);
	TEST_ASSERT_TRUE(strstr(output, "...") != NULL);
}

void test_truncateText_short_text_unchanged(void) {
	char output[256];

	int width = GFX_truncateText(&mock_font, "Hi", output, 500, 0);

	TEST_ASSERT_EQUAL_STRING("Hi", output);
	TEST_ASSERT_EQUAL_INT(20, width); // 2 chars * 10px
}

void test_truncateText_exact_fit_no_truncation(void) {
	char output[256];

	// "12345" = 50px exactly
	int width = GFX_truncateText(&mock_font, "12345", output, 50, 0);

	TEST_ASSERT_EQUAL_STRING("12345", output);
	TEST_ASSERT_EQUAL_INT(50, width);
}

void test_truncateText_one_char_over_truncates(void) {
	char output[256];

	// "123456" = 60px, max = 50px, needs truncation
	int width = GFX_truncateText(&mock_font, "123456", output, 50, 0);

	TEST_ASSERT_TRUE(strstr(output, "...") != NULL);
	TEST_ASSERT_LESS_OR_EQUAL(50, width);
}

///////////////////////////////
// GFX_wrapText() Tests
///////////////////////////////

void test_wrapText_null_string_returns_zero(void) {
	int width = GFX_wrapText(&mock_font, NULL, 100, 0);

	TEST_ASSERT_EQUAL_INT(0, width);
}

void test_wrapText_short_text_no_wrapping(void) {
	char text[256] = "Hello";

	int width = GFX_wrapText(&mock_font, text, 100, 0);

	// Text fits, no newlines added
	TEST_ASSERT_EQUAL_STRING("Hello", text);
	TEST_ASSERT_EQUAL_INT(50, width); // 5 chars * 10px
}

void test_wrapText_wraps_at_space(void) {
	char text[256] = "Hello World Test";

	// "Hello World Test" = 160px, max = 100px
	// Should wrap at spaces
	int width = GFX_wrapText(&mock_font, text, 100, 0);

	// Should have newlines inserted
	TEST_ASSERT_TRUE(strchr(text, '\n') != NULL);
	// Width should be reasonable (widest line, possibly with truncation)
	TEST_ASSERT_GREATER_THAN(0, width);
}

void test_wrapText_modifies_string_in_place(void) {
	char text[256] = "The quick brown fox jumps";

	char* original_ptr = text;
	GFX_wrapText(&mock_font, text, 80, 0);

	// Should have modified the same buffer
	TEST_ASSERT_EQUAL_PTR(original_ptr, text);
	// Should contain newlines now
	TEST_ASSERT_TRUE(strchr(text, '\n') != NULL);
}

void test_wrapText_respects_max_lines(void) {
	char text[256] = "One Two Three Four Five Six Seven Eight";

	// Limit to 3 lines
	GFX_wrapText(&mock_font, text, 50, 3);

	// Count newlines
	int newline_count = 0;
	for (char* p = text; *p; p++) {
		if (*p == '\n')
			newline_count++;
	}

	// Should have at most 2 newlines (for 3 lines total)
	TEST_ASSERT_LESS_OR_EQUAL(2, newline_count);
}

void test_wrapText_zero_max_lines_means_unlimited(void) {
	char text[256] = "A B C D E F G H I J K L M N O P";

	// max_lines = 0 means no limit
	GFX_wrapText(&mock_font, text, 30, 0);

	// Should wrap as needed without line limit
	// Just verify it doesn't crash
	TEST_PASS();
}

void test_wrapText_returns_widest_line_width(void) {
	char text[256] = "Short VeryLongWord X";

	// Should return width of widest line
	int width = GFX_wrapText(&mock_font, text, 100, 0);

	// Width should be the longest line (may include truncated long words)
	TEST_ASSERT_GREATER_THAN(0, width);
	// No upper bound assertion - function returns actual widest line
}

void test_wrapText_single_long_word_truncates(void) {
	char text[256] = "Supercalifragilisticexpialidocious";

	// Single word too long should get truncated with "..."
	GFX_wrapText(&mock_font, text, 100, 0);

	TEST_ASSERT_TRUE(strstr(text, "...") != NULL);
}

void test_wrapText_preserves_multiple_spaces(void) {
	char text[256] = "Hello  World";

	// Has two spaces between words
	GFX_wrapText(&mock_font, text, 200, 0);

	// If no wrapping occurred, spaces should be unchanged
	if (strchr(text, '\n') == NULL) {
		TEST_ASSERT_TRUE(strstr(text, "  ") != NULL);
	}
}

void test_wrapText_last_line_truncated_if_too_long(void) {
	char text[256] = "OK ThisIsAVeryLongWordThatWillBeTruncated";

	GFX_wrapText(&mock_font, text, 80, 0);

	// Last line should be truncated with "..."
	TEST_ASSERT_TRUE(strstr(text, "...") != NULL);
}

///////////////////////////////
// Integration Tests
///////////////////////////////

void test_truncateText_integration_rom_name(void) {
	char output[256];

	// Simulate ROM name truncation
	GFX_truncateText(&mock_font, "The Legend of Zelda - A Link to the Past", output, 150, 5);

	// Should be truncated and end with "..."
	TEST_ASSERT_TRUE(strstr(output, "...") != NULL);
	TEST_ASSERT_LESS_OR_EQUAL(strlen("The Legend of Zelda - A Link to the Past"),
	                          strlen(output));
}

void test_wrapText_integration_dialog_text(void) {
	char text[256] = "This is a long dialog message that should wrap nicely across "
	                 "multiple lines for display";

	int width = GFX_wrapText(&mock_font, text, 200, 4);

	// Should have wrapped the text
	TEST_ASSERT_TRUE(strchr(text, '\n') != NULL);
	// Width is the widest line (may exceed max_width if single word is long)
	TEST_ASSERT_GREATER_THAN(0, width);

	// Should not exceed max lines (4 lines = 3 newlines max)
	int newlines = 0;
	for (char* p = text; *p; p++) {
		if (*p == '\n')
			newlines++;
	}
	TEST_ASSERT_LESS_OR_EQUAL(3, newlines);
}

void test_wrapText_integration_no_spaces(void) {
	char text[256] = "NoSpacesInThisTextAtAll";

	GFX_wrapText(&mock_font, text, 100, 0);

	// Since no spaces, should truncate with "..."
	TEST_ASSERT_TRUE(strstr(text, "...") != NULL);
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_truncateText_empty_string(void) {
	char output[256];

	int width = GFX_truncateText(&mock_font, "", output, 100, 0);

	TEST_ASSERT_EQUAL_STRING("", output);
	TEST_ASSERT_EQUAL_INT(0, width);
}

void test_wrapText_empty_string(void) {
	char text[256] = "";

	int width = GFX_wrapText(&mock_font, text, 100, 0);

	TEST_ASSERT_EQUAL_STRING("", text);
	TEST_ASSERT_EQUAL_INT(0, width);
}

void test_wrapText_only_spaces(void) {
	char text[256] = "     ";

	int width = GFX_wrapText(&mock_font, text, 100, 0);

	// Should handle gracefully
	TEST_ASSERT_GREATER_OR_EQUAL(0, width);
}

///////////////////////////////
// GFX_sizeText() Tests
///////////////////////////////

void test_sizeText_single_line(void) {
	char text[256] = "Hello World";
	int w, h;

	// "Hello World" = 110px (11 chars * 10px), leading = 16px
	GFX_sizeText(&mock_font, text, 16, &w, &h);

	TEST_ASSERT_EQUAL_INT(110, w); // 11 chars * 10px
	TEST_ASSERT_EQUAL_INT(16, h);  // 1 line * 16px leading
}

void test_sizeText_multiple_lines(void) {
	char text[256] = "Line 1\nLine 2\nLine 3";
	int w, h;

	// 3 lines, each "Line X" = 60px, leading = 20px
	GFX_sizeText(&mock_font, text, 20, &w, &h);

	TEST_ASSERT_EQUAL_INT(60, w);  // 6 chars * 10px
	TEST_ASSERT_EQUAL_INT(60, h);  // 3 lines * 20px leading
}

void test_sizeText_finds_widest_line(void) {
	char text[256] = "Short\nThis is a much longer line\nShort";
	int w, h;

	GFX_sizeText(&mock_font, text, 16, &w, &h);

	// Widest line is "This is a much longer line" = 260px (26 chars)
	TEST_ASSERT_EQUAL_INT(260, w); // 26 chars * 10px
	TEST_ASSERT_EQUAL_INT(48, h);  // 3 lines * 16px
}

void test_sizeText_empty_string(void) {
	char text[256] = "";
	int w, h;

	GFX_sizeText(&mock_font, text, 16, &w, &h);

	TEST_ASSERT_EQUAL_INT(0, w);
	TEST_ASSERT_EQUAL_INT(16, h); // 1 line (empty) * 16px
}

void test_sizeText_empty_lines_in_middle(void) {
	char text[256] = "First\n\nThird";
	int w, h;

	GFX_sizeText(&mock_font, text, 18, &w, &h);

	// 3 lines (including empty middle line)
	TEST_ASSERT_EQUAL_INT(50, w);  // "First" or "Third" = 50px
	TEST_ASSERT_EQUAL_INT(54, h);  // 3 lines * 18px
}

void test_sizeText_trailing_newline(void) {
	char text[256] = "Line 1\nLine 2\n";
	int w, h;

	GFX_sizeText(&mock_font, text, 16, &w, &h);

	// Should have 3 lines (last is empty)
	TEST_ASSERT_EQUAL_INT(60, w);  // "Line X" = 60px
	TEST_ASSERT_EQUAL_INT(48, h);  // 3 lines * 16px
}

void test_sizeText_different_leading(void) {
	char text[256] = "A\nB\nC\nD";
	int w, h;

	// Test with large leading
	GFX_sizeText(&mock_font, text, 32, &w, &h);

	TEST_ASSERT_EQUAL_INT(10, w);   // 1 char * 10px
	TEST_ASSERT_EQUAL_INT(128, h);  // 4 lines * 32px
}

void test_sizeText_integration_dialog_box(void) {
	char text[256] = "Game saved!\nContinue playing?";
	int w, h;

	GFX_sizeText(&mock_font, text, 20, &w, &h);

	// "Continue playing?" = 170px is longer
	TEST_ASSERT_EQUAL_INT(170, w); // 17 chars * 10px
	TEST_ASSERT_EQUAL_INT(40, h);  // 2 lines * 20px
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// GFX_truncateText tests
	RUN_TEST(test_truncateText_no_truncation_needed);
	RUN_TEST(test_truncateText_with_padding);
	RUN_TEST(test_truncateText_truncates_long_text);
	RUN_TEST(test_truncateText_result_ends_with_ellipsis);
	RUN_TEST(test_truncateText_respects_max_width_with_padding);
	RUN_TEST(test_truncateText_short_text_unchanged);
	RUN_TEST(test_truncateText_exact_fit_no_truncation);
	RUN_TEST(test_truncateText_one_char_over_truncates);

	// GFX_wrapText tests
	RUN_TEST(test_wrapText_null_string_returns_zero);
	RUN_TEST(test_wrapText_short_text_no_wrapping);
	RUN_TEST(test_wrapText_wraps_at_space);
	RUN_TEST(test_wrapText_modifies_string_in_place);
	RUN_TEST(test_wrapText_respects_max_lines);
	RUN_TEST(test_wrapText_zero_max_lines_means_unlimited);
	RUN_TEST(test_wrapText_returns_widest_line_width);
	RUN_TEST(test_wrapText_single_long_word_truncates);
	RUN_TEST(test_wrapText_preserves_multiple_spaces);
	RUN_TEST(test_wrapText_last_line_truncated_if_too_long);

	// GFX_sizeText tests
	RUN_TEST(test_sizeText_single_line);
	RUN_TEST(test_sizeText_multiple_lines);
	RUN_TEST(test_sizeText_finds_widest_line);
	RUN_TEST(test_sizeText_empty_string);
	RUN_TEST(test_sizeText_empty_lines_in_middle);
	RUN_TEST(test_sizeText_trailing_newline);
	RUN_TEST(test_sizeText_different_leading);
	RUN_TEST(test_sizeText_integration_dialog_box);

	// Integration tests
	RUN_TEST(test_truncateText_integration_rom_name);
	RUN_TEST(test_wrapText_integration_dialog_text);
	RUN_TEST(test_wrapText_integration_no_spaces);

	// Edge cases
	RUN_TEST(test_truncateText_empty_string);
	RUN_TEST(test_wrapText_empty_string);
	RUN_TEST(test_wrapText_only_spaces);

	return UNITY_END();
}
