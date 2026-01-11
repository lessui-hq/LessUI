/**
 * test_player_utils.c - Unit tests for pure player utility functions
 *
 * Tests utility functions extracted from player.c that have no
 * external dependencies.
 *
 * Test coverage:
 * - PlayerUtils_getCoreName - Core name extraction from .so path
 * - PlayerUtils_replaceString - In-place string replacement
 * - PlayerUtils_escapeSingleQuotes - Shell quote escaping
 *
 * For option-related tests, see test_player_options.c
 * For CPU frequency tests, see test_cpu.c (in common/)
 */

#include "unity.h"
#include "player_utils.h"
#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// PlayerUtils_getCoreName tests
///////////////////////////////

void test_getCoreName_simple_core(void) {
	char out[256];
	PlayerUtils_getCoreName("fceumm_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("fceumm", out);
}

void test_getCoreName_with_underscore_in_name(void) {
	char out[256];
	PlayerUtils_getCoreName("pcsx_rearmed_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("pcsx_rearmed", out);
}

void test_getCoreName_gambatte(void) {
	char out[256];
	PlayerUtils_getCoreName("gambatte_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("gambatte", out);
}

void test_getCoreName_mgba(void) {
	char out[256];
	PlayerUtils_getCoreName("mgba_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("mgba", out);
}

void test_getCoreName_snes9x(void) {
	char out[256];
	PlayerUtils_getCoreName("snes9x_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("snes9x", out);
}

void test_getCoreName_with_path(void) {
	char out[256];
	PlayerUtils_getCoreName("/path/to/cores/fceumm_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("fceumm", out);
}

void test_getCoreName_genesis_plus_gx(void) {
	char out[256];
	PlayerUtils_getCoreName("genesis_plus_gx_libretro.so", out);
	TEST_ASSERT_EQUAL_STRING("genesis_plus_gx", out);
}

void test_getCoreName_no_underscore(void) {
	char out[256];
	// Edge case: no underscore in filename
	PlayerUtils_getCoreName("corename.so", out);
	TEST_ASSERT_EQUAL_STRING("corename.so", out);
}

///////////////////////////////
// PlayerUtils_replaceString tests
///////////////////////////////

void test_replaceString_single_occurrence(void) {
	char buf[256] = "hello world";
	int count = PlayerUtils_replaceString(buf, "world", "there");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello there", buf);
}

void test_replaceString_multiple_occurrences(void) {
	char buf[256] = "one two one three one";
	int count = PlayerUtils_replaceString(buf, "one", "1");
	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_EQUAL_STRING("1 two 1 three 1", buf);
}

void test_replaceString_no_match(void) {
	char buf[256] = "hello world";
	int count = PlayerUtils_replaceString(buf, "xyz", "abc");
	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void test_replaceString_longer_replacement(void) {
	char buf[256] = "a b c";
	int count = PlayerUtils_replaceString(buf, "b", "longer");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("a longer c", buf);
}

void test_replaceString_shorter_replacement(void) {
	char buf[256] = "hello world";
	int count = PlayerUtils_replaceString(buf, "world", "x");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello x", buf);
}

void test_replaceString_empty_replacement(void) {
	char buf[256] = "hello world";
	int count = PlayerUtils_replaceString(buf, "world", "");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("hello ", buf);
}

void test_replaceString_at_start(void) {
	char buf[256] = "start middle end";
	int count = PlayerUtils_replaceString(buf, "start", "BEGIN");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("BEGIN middle end", buf);
}

void test_replaceString_at_end(void) {
	char buf[256] = "start middle end";
	int count = PlayerUtils_replaceString(buf, "end", "END");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("start middle END", buf);
}

void test_replaceString_entire_string(void) {
	char buf[256] = "replace";
	int count = PlayerUtils_replaceString(buf, "replace", "new");
	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("new", buf);
}

///////////////////////////////
// PlayerUtils_escapeSingleQuotes tests
///////////////////////////////

void test_escapeSingleQuotes_single_quote(void) {
	char buf[256] = "it's cool";
	PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("it'\\''s cool", buf);
}

void test_escapeSingleQuotes_multiple_quotes(void) {
	char buf[256] = "'hello' 'world'";
	PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("'\\''hello'\\'' '\\''world'\\''", buf);
}

void test_escapeSingleQuotes_no_quotes(void) {
	char buf[256] = "hello world";
	PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("hello world", buf);
}

void test_escapeSingleQuotes_returns_same_pointer(void) {
	char buf[256] = "test";
	char* result = PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_PTR(buf, result);
}

void test_escapeSingleQuotes_game_title(void) {
	char buf[256] = "Tony Hawk's Pro Skater";
	PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("Tony Hawk'\\''s Pro Skater", buf);
}

void test_escapeSingleQuotes_apostrophe_at_end(void) {
	char buf[256] = "Players'";
	PlayerUtils_escapeSingleQuotes(buf);
	TEST_ASSERT_EQUAL_STRING("Players'\\''", buf);
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// PlayerUtils_getCoreName
	RUN_TEST(test_getCoreName_simple_core);
	RUN_TEST(test_getCoreName_with_underscore_in_name);
	RUN_TEST(test_getCoreName_gambatte);
	RUN_TEST(test_getCoreName_mgba);
	RUN_TEST(test_getCoreName_snes9x);
	RUN_TEST(test_getCoreName_with_path);
	RUN_TEST(test_getCoreName_genesis_plus_gx);
	RUN_TEST(test_getCoreName_no_underscore);

	// PlayerUtils_replaceString
	RUN_TEST(test_replaceString_single_occurrence);
	RUN_TEST(test_replaceString_multiple_occurrences);
	RUN_TEST(test_replaceString_no_match);
	RUN_TEST(test_replaceString_longer_replacement);
	RUN_TEST(test_replaceString_shorter_replacement);
	RUN_TEST(test_replaceString_empty_replacement);
	RUN_TEST(test_replaceString_at_start);
	RUN_TEST(test_replaceString_at_end);
	RUN_TEST(test_replaceString_entire_string);

	// PlayerUtils_escapeSingleQuotes
	RUN_TEST(test_escapeSingleQuotes_single_quote);
	RUN_TEST(test_escapeSingleQuotes_multiple_quotes);
	RUN_TEST(test_escapeSingleQuotes_no_quotes);
	RUN_TEST(test_escapeSingleQuotes_returns_same_pointer);
	RUN_TEST(test_escapeSingleQuotes_game_title);
	RUN_TEST(test_escapeSingleQuotes_apostrophe_at_end);

	return UNITY_END();
}
