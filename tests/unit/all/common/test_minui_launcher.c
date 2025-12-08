/**
 * test_minui_launcher.c - Unit tests for ROM/PAK launcher utilities
 *
 * Tests command construction and string escaping functions.
 * These are pure functions with minimal dependencies.
 *
 * Test coverage:
 * - String replacement
 * - Quote escaping for shell safety
 * - PAK command construction
 * - ROM command construction
 * - Path prefix checking
 */

#include "../../../support/unity/unity.h"
#include "minui_launcher.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Temp file for queueCommand tests
static char test_path[256];

void setUp(void) {
	strcpy(test_path, "/tmp/test_launcher_XXXXXX");
	int fd = mkstemp(test_path);
	if (fd >= 0)
		close(fd);
}

void tearDown(void) {
	unlink(test_path);
}

///////////////////////////////
// replaceString Tests
///////////////////////////////

void test_replaceString_replaces_single_occurrence(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, "World", "Universe");

	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hello Universe", str);
}

void test_replaceString_replaces_multiple_occurrences(void) {
	char str[256] = "one two one three one";

	int count = MinUI_replaceString(str, "one", "1");

	TEST_ASSERT_EQUAL(3, count);
	TEST_ASSERT_EQUAL_STRING("1 two 1 three 1", str);
}

void test_replaceString_handles_no_match(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, "foo", "bar");

	TEST_ASSERT_EQUAL(0, count);
	TEST_ASSERT_EQUAL_STRING("Hello World", str);
}

void test_replaceString_replaces_with_longer_string(void) {
	char str[256] = "a b c";

	int count = MinUI_replaceString(str, " ", "---");

	TEST_ASSERT_EQUAL(2, count);
	TEST_ASSERT_EQUAL_STRING("a---b---c", str);
}

void test_replaceString_replaces_with_shorter_string(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, "World", "X");

	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hello X", str);
}

void test_replaceString_replaces_with_empty_string(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, " World", "");

	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hello", str);
}

void test_replaceString_replaces_at_start(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, "Hello", "Hi");

	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hi World", str);
}

void test_replaceString_replaces_at_end(void) {
	char str[256] = "Hello World";

	int count = MinUI_replaceString(str, "World", "There");

	TEST_ASSERT_EQUAL(1, count);
	TEST_ASSERT_EQUAL_STRING("Hello There", str);
}

///////////////////////////////
// escapeSingleQuotes Tests
///////////////////////////////

void test_escapeSingleQuotes_escapes_single_quote(void) {
	char str[256] = "it's a test";

	char* result = MinUI_escapeSingleQuotes(str);

	TEST_ASSERT_EQUAL_PTR(str, result);
	TEST_ASSERT_EQUAL_STRING("it'\\''s a test", str);
}

void test_escapeSingleQuotes_escapes_multiple_quotes(void) {
	char str[256] = "'hello' 'world'";

	MinUI_escapeSingleQuotes(str);

	TEST_ASSERT_EQUAL_STRING("'\\''hello'\\'' '\\''world'\\''", str);
}

void test_escapeSingleQuotes_handles_no_quotes(void) {
	char str[256] = "hello world";

	MinUI_escapeSingleQuotes(str);

	TEST_ASSERT_EQUAL_STRING("hello world", str);
}

void test_escapeSingleQuotes_handles_empty_string(void) {
	char str[256] = "";

	MinUI_escapeSingleQuotes(str);

	TEST_ASSERT_EQUAL_STRING("", str);
}

void test_escapeSingleQuotes_handles_only_quotes(void) {
	char str[256] = "'''";

	MinUI_escapeSingleQuotes(str);

	// Each ' becomes '\''
	TEST_ASSERT_EQUAL_STRING("'\\'''\\'''\\''", str);
}

void test_escapeSingleQuotes_real_path_example(void) {
	// Real-world path with apostrophe
	char str[256] = "/mnt/SDCARD/Roms/GB/Link's Awakening.gb";

	MinUI_escapeSingleQuotes(str);

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/Link'\\''s Awakening.gb", str);
}

///////////////////////////////
// buildPakCommand Tests
///////////////////////////////

void test_buildPakCommand_creates_correct_format(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char pak_path[256] = "/mnt/SDCARD/.system/miyoomini/paks/Input.pak";

	int len = MinUI_buildPakCommand(cmd, sizeof(cmd), pak_path);

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL_STRING("'/mnt/SDCARD/.system/miyoomini/paks/Input.pak/launch.sh'", cmd);
}

void test_buildPakCommand_escapes_quotes_in_path(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char pak_path[256] = "/path/with'quote/Test.pak";

	int len = MinUI_buildPakCommand(cmd, sizeof(cmd), pak_path);

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL_STRING("'/path/with'\\''quote/Test.pak/launch.sh'", cmd);
}

void test_buildPakCommand_returns_zero_on_null_cmd(void) {
	char pak_path[256] = "/some/path.pak";

	int len = MinUI_buildPakCommand(NULL, 100, pak_path);

	TEST_ASSERT_EQUAL(0, len);
}

void test_buildPakCommand_returns_zero_on_null_path(void) {
	char cmd[MINUI_MAX_CMD_SIZE];

	int len = MinUI_buildPakCommand(cmd, sizeof(cmd), NULL);

	TEST_ASSERT_EQUAL(0, len);
}

void test_buildPakCommand_returns_zero_on_small_buffer(void) {
	char cmd[10]; // Too small
	char pak_path[256] = "/mnt/SDCARD/path.pak";

	int len = MinUI_buildPakCommand(cmd, sizeof(cmd), pak_path);

	TEST_ASSERT_EQUAL(0, len);
}

///////////////////////////////
// buildRomCommand Tests
///////////////////////////////

void test_buildRomCommand_creates_correct_format(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char emu_path[256] = "/mnt/SDCARD/.system/miyoomini/paks/GB.pak/launch.sh";
	char rom_path[256] = "/mnt/SDCARD/Roms/GB/Tetris.gb";

	int len = MinUI_buildRomCommand(cmd, sizeof(cmd), emu_path, rom_path);

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL_STRING(
	    "'/mnt/SDCARD/.system/miyoomini/paks/GB.pak/launch.sh' '/mnt/SDCARD/Roms/GB/Tetris.gb'", cmd);
}

void test_buildRomCommand_escapes_quotes_in_both_paths(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char emu_path[256] = "/path/with'quote/launch.sh";
	char rom_path[256] = "/roms/Game's Name.rom";

	int len = MinUI_buildRomCommand(cmd, sizeof(cmd), emu_path, rom_path);

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL_STRING("'/path/with'\\''quote/launch.sh' '/roms/Game'\\''s Name.rom'", cmd);
}

void test_buildRomCommand_returns_zero_on_null_inputs(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char path[256] = "/some/path";

	TEST_ASSERT_EQUAL(0, MinUI_buildRomCommand(NULL, 100, path, path));
	TEST_ASSERT_EQUAL(0, MinUI_buildRomCommand(cmd, sizeof(cmd), NULL, path));
	TEST_ASSERT_EQUAL(0, MinUI_buildRomCommand(cmd, sizeof(cmd), path, NULL));
}

void test_buildRomCommand_handles_long_paths(void) {
	char cmd[MINUI_MAX_CMD_SIZE];
	char emu_path[256] = "/mnt/SDCARD/.system/platform/paks/Emulator.pak/launch.sh";
	char rom_path[256] =
	    "/mnt/SDCARD/Roms/System/The Legend of Something - A Very Long Game Name (USA) (Rev 1).rom";

	int len = MinUI_buildRomCommand(cmd, sizeof(cmd), emu_path, rom_path);

	TEST_ASSERT_GREATER_THAN(0, len);
	// Just verify it starts and ends correctly
	TEST_ASSERT_TRUE(cmd[0] == '\'');
	TEST_ASSERT_TRUE(cmd[len - 1] == '\'');
}

///////////////////////////////
// queueCommand Tests
///////////////////////////////

void test_queueCommand_writes_to_file(void) {
	const char* cmd = "'/path/to/launch.sh' '/path/to/game.rom'";

	int result = MinUI_queueCommand(test_path, cmd);

	TEST_ASSERT_EQUAL(0, result);

	// Verify file contents
	FILE* f = fopen(test_path, "r");
	TEST_ASSERT_NOT_NULL(f);

	char buffer[256];
	char* read = fgets(buffer, sizeof(buffer), f);
	fclose(f);

	TEST_ASSERT_NOT_NULL(read);
	TEST_ASSERT_EQUAL_STRING(cmd, buffer);
}

void test_queueCommand_returns_error_on_null_inputs(void) {
	TEST_ASSERT_EQUAL(-1, MinUI_queueCommand(NULL, "cmd"));
	TEST_ASSERT_EQUAL(-1, MinUI_queueCommand(test_path, NULL));
}

void test_queueCommand_overwrites_existing_file(void) {
	// Write initial content
	FILE* f = fopen(test_path, "w");
	fputs("old content", f);
	fclose(f);

	// Queue new command
	int result = MinUI_queueCommand(test_path, "new command");
	TEST_ASSERT_EQUAL(0, result);

	// Verify new content
	f = fopen(test_path, "r");
	char buffer[256];
	fgets(buffer, sizeof(buffer), f);
	fclose(f);

	TEST_ASSERT_EQUAL_STRING("new command", buffer);
}

///////////////////////////////
// isRomsPath Tests
///////////////////////////////

void test_isRomsPath_returns_true_for_exact_match(void) {
	int result = MinUI_isRomsPath("/mnt/SDCARD/Roms", "/mnt/SDCARD/Roms");
	TEST_ASSERT_EQUAL(1, result);
}

void test_isRomsPath_returns_true_for_subpath(void) {
	int result = MinUI_isRomsPath("/mnt/SDCARD/Roms/GB/game.gb", "/mnt/SDCARD/Roms");
	TEST_ASSERT_EQUAL(1, result);
}

void test_isRomsPath_returns_false_for_different_path(void) {
	int result = MinUI_isRomsPath("/mnt/SDCARD/Apps/something", "/mnt/SDCARD/Roms");
	TEST_ASSERT_EQUAL(0, result);
}

void test_isRomsPath_returns_false_for_similar_prefix(void) {
	// /mnt/SDCARD/RomsExtra should NOT match /mnt/SDCARD/Roms
	int result = MinUI_isRomsPath("/mnt/SDCARD/RomsExtra/game.gb", "/mnt/SDCARD/Roms");
	TEST_ASSERT_EQUAL(0, result);
}

void test_isRomsPath_handles_null_inputs(void) {
	TEST_ASSERT_EQUAL(0, MinUI_isRomsPath(NULL, "/mnt/SDCARD/Roms"));
	TEST_ASSERT_EQUAL(0, MinUI_isRomsPath("/mnt/SDCARD/Roms/game", NULL));
	TEST_ASSERT_EQUAL(0, MinUI_isRomsPath(NULL, NULL));
}

void test_isRomsPath_handles_trailing_slash(void) {
	// Path with trailing slash after roms_path
	int result = MinUI_isRomsPath("/mnt/SDCARD/Roms/GB", "/mnt/SDCARD/Roms");
	TEST_ASSERT_EQUAL(1, result);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// replaceString tests
	RUN_TEST(test_replaceString_replaces_single_occurrence);
	RUN_TEST(test_replaceString_replaces_multiple_occurrences);
	RUN_TEST(test_replaceString_handles_no_match);
	RUN_TEST(test_replaceString_replaces_with_longer_string);
	RUN_TEST(test_replaceString_replaces_with_shorter_string);
	RUN_TEST(test_replaceString_replaces_with_empty_string);
	RUN_TEST(test_replaceString_replaces_at_start);
	RUN_TEST(test_replaceString_replaces_at_end);

	// escapeSingleQuotes tests
	RUN_TEST(test_escapeSingleQuotes_escapes_single_quote);
	RUN_TEST(test_escapeSingleQuotes_escapes_multiple_quotes);
	RUN_TEST(test_escapeSingleQuotes_handles_no_quotes);
	RUN_TEST(test_escapeSingleQuotes_handles_empty_string);
	RUN_TEST(test_escapeSingleQuotes_handles_only_quotes);
	RUN_TEST(test_escapeSingleQuotes_real_path_example);

	// buildPakCommand tests
	RUN_TEST(test_buildPakCommand_creates_correct_format);
	RUN_TEST(test_buildPakCommand_escapes_quotes_in_path);
	RUN_TEST(test_buildPakCommand_returns_zero_on_null_cmd);
	RUN_TEST(test_buildPakCommand_returns_zero_on_null_path);
	RUN_TEST(test_buildPakCommand_returns_zero_on_small_buffer);

	// buildRomCommand tests
	RUN_TEST(test_buildRomCommand_creates_correct_format);
	RUN_TEST(test_buildRomCommand_escapes_quotes_in_both_paths);
	RUN_TEST(test_buildRomCommand_returns_zero_on_null_inputs);
	RUN_TEST(test_buildRomCommand_handles_long_paths);

	// queueCommand tests
	RUN_TEST(test_queueCommand_writes_to_file);
	RUN_TEST(test_queueCommand_returns_error_on_null_inputs);
	RUN_TEST(test_queueCommand_overwrites_existing_file);

	// isRomsPath tests
	RUN_TEST(test_isRomsPath_returns_true_for_exact_match);
	RUN_TEST(test_isRomsPath_returns_true_for_subpath);
	RUN_TEST(test_isRomsPath_returns_false_for_different_path);
	RUN_TEST(test_isRomsPath_returns_false_for_similar_prefix);
	RUN_TEST(test_isRomsPath_handles_null_inputs);
	RUN_TEST(test_isRomsPath_handles_trailing_slash);

	return UNITY_END();
}
