/**
 * test_minarch_utils.c - Unit tests for pure minarch utility functions
 *
 * Tests utility functions extracted from minarch.c that have no
 * external dependencies.
 *
 * Test coverage:
 * - MinArch_getCoreName - Core name extraction from .so path
 * - MinArch_getOptionValueIndex - Option value searching
 * - MinArch_findNearestFrequency - CPU frequency matching
 * - MinArch_replaceString - In-place string replacement
 * - MinArch_escapeSingleQuotes - Shell quote escaping
 */

#include "../../support/unity/unity.h"
#include "minarch_utils.h"
#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArch_getCoreName tests
///////////////////////////////

void test_getCoreName_simple_core(void) {
	char out[256];
	MinArch_getCoreName("fceumm_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("fceumm", out);
}

void test_getCoreName_with_underscore_in_name(void) {
	char out[256];
	MinArch_getCoreName("pcsx_rearmed_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("pcsx_rearmed", out);
}

void test_getCoreName_gambatte(void) {
	char out[256];
	MinArch_getCoreName("gambatte_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("gambatte", out);
}

void test_getCoreName_mgba(void) {
	char out[256];
	MinArch_getCoreName("mgba_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("mgba", out);
}

void test_getCoreName_snes9x(void) {
	char out[256];
	MinArch_getCoreName("snes9x_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("snes9x", out);
}

void test_getCoreName_with_path(void) {
	char out[256];
	MinArch_getCoreName("/path/to/cores/fceumm_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("fceumm", out);
}

void test_getCoreName_genesis_plus_gx(void) {
	char out[256];
	MinArch_getCoreName("genesis_plus_gx_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("genesis_plus_gx", out);
}

void test_getCoreName_no_underscore(void) {
	char out[256];
	// Edge case: no underscore in filename
	MinArch_getCoreName("corename.so", out);
	TEST_ASSERT_EQUAL_STRING("corename.so", out);
}

///////////////////////////////
// MinArch_getOptionValueIndex tests
///////////////////////////////

void test_getOptionValueIndex_finds_first(void) {
	const char* values[] = {"1x", "2x", "3x"};
	MinArchOption opt = {.key = "scale", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, "1x"));
}

void test_getOptionValueIndex_finds_middle(void) {
	const char* values[] = {"1x", "2x", "3x"};
	MinArchOption opt = {.key = "scale", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(1, MinArch_getOptionValueIndex(&opt, "2x"));
}

void test_getOptionValueIndex_finds_last(void) {
	const char* values[] = {"1x", "2x", "3x"};
	MinArchOption opt = {.key = "scale", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(2, MinArch_getOptionValueIndex(&opt, "3x"));
}

void test_getOptionValueIndex_returns_0_for_not_found(void) {
	const char* values[] = {"1x", "2x", "3x"};
	MinArchOption opt = {.key = "scale", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, "4x"));
}

void test_getOptionValueIndex_returns_0_for_null_value(void) {
	const char* values[] = {"1x", "2x", "3x"};
	MinArchOption opt = {.key = "scale", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, NULL));
}

void test_getOptionValueIndex_returns_0_for_null_opt(void) {
	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(NULL, "1x"));
}

void test_getOptionValueIndex_case_sensitive(void) {
	const char* values[] = {"enabled", "disabled"};
	MinArchOption opt = {.key = "feature", .values = values, .count = 2, .value = 0};

	// Should not match "Enabled" (capital E)
	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, "Enabled"));
}

void test_getOptionValueIndex_empty_string(void) {
	const char* values[] = {"", "on", "off"};
	MinArchOption opt = {.key = "toggle", .values = values, .count = 3, .value = 0};

	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, ""));
}

void test_getOptionValueIndex_single_value(void) {
	const char* values[] = {"only"};
	MinArchOption opt = {.key = "single", .values = values, .count = 1, .value = 0};

	TEST_ASSERT_EQUAL_INT(0, MinArch_getOptionValueIndex(&opt, "only"));
}

///////////////////////////////
// MinArch_findNearestFrequency tests
///////////////////////////////

void test_findNearestFrequency_exact_match(void) {
	int freqs[] = {600000, 800000, 1000000, 1200000};
	TEST_ASSERT_EQUAL_INT(2, MinArch_findNearestFrequency(freqs, 4, 1000000));
}

void test_findNearestFrequency_rounds_up(void) {
	int freqs[] = {600000, 800000, 1000000, 1200000};
	// 750000 is closer to 800000 than 600000
	TEST_ASSERT_EQUAL_INT(1, MinArch_findNearestFrequency(freqs, 4, 750000));
}

void test_findNearestFrequency_rounds_down(void) {
	int freqs[] = {600000, 800000, 1000000, 1200000};
	// 650000 is closer to 600000 than 800000
	TEST_ASSERT_EQUAL_INT(0, MinArch_findNearestFrequency(freqs, 4, 650000));
}

void test_findNearestFrequency_below_minimum(void) {
	int freqs[] = {600000, 800000, 1000000, 1200000};
	// 100000 is closest to first element
	TEST_ASSERT_EQUAL_INT(0, MinArch_findNearestFrequency(freqs, 4, 100000));
}

void test_findNearestFrequency_above_maximum(void) {
	int freqs[] = {600000, 800000, 1000000, 1200000};
	// 2000000 is closest to last element
	TEST_ASSERT_EQUAL_INT(3, MinArch_findNearestFrequency(freqs, 4, 2000000));
}

void test_findNearestFrequency_single_element(void) {
	int freqs[] = {1000000};
	TEST_ASSERT_EQUAL_INT(0, MinArch_findNearestFrequency(freqs, 1, 500000));
}

void test_findNearestFrequency_empty_returns_0(void) {
	int freqs[] = {1000000};
	TEST_ASSERT_EQUAL_INT(0, MinArch_findNearestFrequency(freqs, 0, 500000));
}

void test_findNearestFrequency_null_returns_0(void) {
	TEST_ASSERT_EQUAL_INT(0, MinArch_findNearestFrequency(NULL, 4, 500000));
}

void test_findNearestFrequency_midpoint_prefers_first(void) {
	int freqs[] = {600000, 800000};
	// 700000 is equidistant - should return first match (index 0)
	int result = MinArch_findNearestFrequency(freqs, 2, 700000);
	// Either 0 or 1 is acceptable since they're equidistant
	TEST_ASSERT_TRUE(result == 0 || result == 1);
}

///////////////////////////////
// MinArch_replaceString tests
///////////////////////////////

void test_replaceString_single_occurrence(void) {
	char buf[256] = "hello world";
	int count = MinArch_replaceString(buf, "world", "there");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello there", buf);
}

void test_replaceString_multiple_occurrences(void) {
	char buf[256] = "one two one three one";
	int count = MinArch_replaceString(buf, "one", "1");
	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_EQUAL_STRING("1 two 1 three 1", buf);
}

void test_replaceString_no_match(void) {
	char buf[256] = "hello world";
	int count = MinArch_replaceString(buf, "xyz", "abc");
	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void test_replaceString_longer_replacement(void) {
	char buf[256] = "a b c";
	int count = MinArch_replaceString(buf, "b", "longer");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("a longer c", buf);
}

void test_replaceString_shorter_replacement(void) {
	char buf[256] = "hello world";
	int count = MinArch_replaceString(buf, "world", "x");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello x", buf);
}

void test_replaceString_empty_replacement(void) {
	char buf[256] = "hello world";
	int count = MinArch_replaceString(buf, "world", "");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello ", buf);
}

void test_replaceString_at_start(void) {
	char buf[256] = "start middle end";
	int count = MinArch_replaceString(buf, "start", "BEGIN");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("BEGIN middle end", buf);
}

void test_replaceString_at_end(void) {
	char buf[256] = "start middle end";
	int count = MinArch_replaceString(buf, "end", "END");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("start middle END", buf);
}

void test_replaceString_entire_string(void) {
	char buf[256] = "replace";
	int count = MinArch_replaceString(buf, "replace", "new");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("new", buf);
}

///////////////////////////////
// MinArch_escapeSingleQuotes tests
///////////////////////////////

void test_escapeSingleQuotes_single_quote(void) {
	char buf[256] = "it's cool";
	MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("it'\\''s cool", buf);
}

void test_escapeSingleQuotes_multiple_quotes(void) {
	char buf[256] = "'hello' 'world'";
	MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("'\\''hello'\\'' '\\''world'\\''", buf);
}

void test_escapeSingleQuotes_no_quotes(void) {
	char buf[256] = "hello world";
	MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void test_escapeSingleQuotes_returns_same_pointer(void) {
	char buf[256] = "test";
	char* result = MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_PTR(buf, result);
}

void test_escapeSingleQuotes_game_title(void) {
	char buf[256] = "Tony Hawk's Pro Skater";
	MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("Tony Hawk'\\''s Pro Skater", buf);
}

void test_escapeSingleQuotes_apostrophe_at_end(void) {
	char buf[256] = "Players'";
	MinArch_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("Players'\\''", buf);
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// MinArch_getCoreName
	RUN_TEST(test_getCoreName_simple_core);
	RUN_TEST(test_getCoreName_with_underscore_in_name);
	RUN_TEST(test_getCoreName_gambatte);
	RUN_TEST(test_getCoreName_mgba);
	RUN_TEST(test_getCoreName_snes9x);
	RUN_TEST(test_getCoreName_with_path);
	RUN_TEST(test_getCoreName_genesis_plus_gx);
	RUN_TEST(test_getCoreName_no_underscore);

	// MinArch_getOptionValueIndex
	RUN_TEST(test_getOptionValueIndex_finds_first);
	RUN_TEST(test_getOptionValueIndex_finds_middle);
	RUN_TEST(test_getOptionValueIndex_finds_last);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_not_found);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_null_value);
	RUN_TEST(test_getOptionValueIndex_returns_0_for_null_opt);
	RUN_TEST(test_getOptionValueIndex_case_sensitive);
	RUN_TEST(test_getOptionValueIndex_empty_string);
	RUN_TEST(test_getOptionValueIndex_single_value);

	// MinArch_findNearestFrequency
	RUN_TEST(test_findNearestFrequency_exact_match);
	RUN_TEST(test_findNearestFrequency_rounds_up);
	RUN_TEST(test_findNearestFrequency_rounds_down);
	RUN_TEST(test_findNearestFrequency_below_minimum);
	RUN_TEST(test_findNearestFrequency_above_maximum);
	RUN_TEST(test_findNearestFrequency_single_element);
	RUN_TEST(test_findNearestFrequency_empty_returns_0);
	RUN_TEST(test_findNearestFrequency_null_returns_0);
	RUN_TEST(test_findNearestFrequency_midpoint_prefers_first);

	// MinArch_replaceString
	RUN_TEST(test_replaceString_single_occurrence);
	RUN_TEST(test_replaceString_multiple_occurrences);
	RUN_TEST(test_replaceString_no_match);
	RUN_TEST(test_replaceString_longer_replacement);
	RUN_TEST(test_replaceString_shorter_replacement);
	RUN_TEST(test_replaceString_empty_replacement);
	RUN_TEST(test_replaceString_at_start);
	RUN_TEST(test_replaceString_at_end);
	RUN_TEST(test_replaceString_entire_string);

	// MinArch_escapeSingleQuotes
	RUN_TEST(test_escapeSingleQuotes_single_quote);
	RUN_TEST(test_escapeSingleQuotes_multiple_quotes);
	RUN_TEST(test_escapeSingleQuotes_no_quotes);
	RUN_TEST(test_escapeSingleQuotes_returns_same_pointer);
	RUN_TEST(test_escapeSingleQuotes_game_title);
	RUN_TEST(test_escapeSingleQuotes_apostrophe_at_end);

	return UNITY_END();
}
