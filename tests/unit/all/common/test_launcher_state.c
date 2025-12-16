/**
 * test_launcher_state.c - Unit tests for launcher state persistence
 *
 * Tests path decomposition, collation detection, resume path generation,
 * and path validation utilities.
 *
 * 25 tests organized by functionality.
 */

#include "../../../support/unity/unity.h"
#include "launcher_state.h"

#include <string.h>

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Path Stack Tests
///////////////////////////////

void test_PathStack_new_creates_stack(void) {
	LauncherPathStack* stack = LauncherPathStack_new(10);
	TEST_ASSERT_NOT_NULL(stack);
	TEST_ASSERT_EQUAL(0, stack->count);
	TEST_ASSERT_EQUAL(10, stack->capacity);
	LauncherPathStack_free(stack);
}

void test_PathStack_push_adds_items(void) {
	LauncherPathStack* stack = LauncherPathStack_new(10);
	TEST_ASSERT_TRUE(LauncherPathStack_push(stack, "/path/one"));
	TEST_ASSERT_TRUE(LauncherPathStack_push(stack, "/path/two"));
	TEST_ASSERT_EQUAL(2, stack->count);
	LauncherPathStack_free(stack);
}

void test_PathStack_pop_returns_lifo(void) {
	LauncherPathStack* stack = LauncherPathStack_new(10);
	LauncherPathStack_push(stack, "/path/one");
	LauncherPathStack_push(stack, "/path/two");
	LauncherPathStack_push(stack, "/path/three");

	char path[LAUNCHER_STATE_MAX_PATH];
	TEST_ASSERT_TRUE(LauncherPathStack_pop(stack, path));
	TEST_ASSERT_EQUAL_STRING("/path/three", path);

	TEST_ASSERT_TRUE(LauncherPathStack_pop(stack, path));
	TEST_ASSERT_EQUAL_STRING("/path/two", path);

	LauncherPathStack_free(stack);
}

void test_PathStack_pop_empty_returns_false(void) {
	LauncherPathStack* stack = LauncherPathStack_new(10);
	char path[LAUNCHER_STATE_MAX_PATH];
	TEST_ASSERT_FALSE(LauncherPathStack_pop(stack, path));
	LauncherPathStack_free(stack);
}

void test_PathStack_grows_when_full(void) {
	LauncherPathStack* stack = LauncherPathStack_new(2);
	TEST_ASSERT_TRUE(LauncherPathStack_push(stack, "/one"));
	TEST_ASSERT_TRUE(LauncherPathStack_push(stack, "/two"));
	TEST_ASSERT_TRUE(LauncherPathStack_push(stack, "/three")); // Should grow
	TEST_ASSERT_EQUAL(3, stack->count);
	TEST_ASSERT_TRUE(stack->capacity >= 3);
	LauncherPathStack_free(stack);
}

///////////////////////////////
// Path Decomposition Tests
///////////////////////////////

void test_decomposePath_creates_stack(void) {
	LauncherPathStack* stack =
	    LauncherState_decomposePath("/mnt/SDCARD/Roms/GB/game.gb", "/mnt/SDCARD");

	TEST_ASSERT_NOT_NULL(stack);
	TEST_ASSERT_EQUAL(3, stack->count);

	char path[LAUNCHER_STATE_MAX_PATH];

	// Pop in LIFO order (first pushed = last popped)
	TEST_ASSERT_TRUE(LauncherPathStack_pop(stack, path));
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms", path);

	TEST_ASSERT_TRUE(LauncherPathStack_pop(stack, path));
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB", path);

	TEST_ASSERT_TRUE(LauncherPathStack_pop(stack, path));
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/game.gb", path);

	LauncherPathStack_free(stack);
}

void test_decomposePath_stops_at_root(void) {
	LauncherPathStack* stack = LauncherState_decomposePath("/mnt/SDCARD/Roms", "/mnt/SDCARD");

	TEST_ASSERT_NOT_NULL(stack);
	TEST_ASSERT_EQUAL(1, stack->count); // Only /mnt/SDCARD/Roms

	LauncherPathStack_free(stack);
}

void test_decomposePath_null_returns_null(void) {
	TEST_ASSERT_NULL(LauncherState_decomposePath(NULL, "/mnt"));
	TEST_ASSERT_NULL(LauncherState_decomposePath("/path", NULL));
}

///////////////////////////////
// Filename Extraction Tests
///////////////////////////////

void test_extractFilename_gets_filename(void) {
	char filename[LAUNCHER_STATE_MAX_PATH];
	LauncherState_extractFilename("/mnt/SDCARD/Roms/GB/Pokemon.gb", filename);
	TEST_ASSERT_EQUAL_STRING("Pokemon.gb", filename);
}

void test_extractFilename_handles_no_slash(void) {
	char filename[LAUNCHER_STATE_MAX_PATH];
	LauncherState_extractFilename("justfile.txt", filename);
	TEST_ASSERT_EQUAL_STRING("justfile.txt", filename);
}

void test_extractFilename_null_safe(void) {
	char filename[LAUNCHER_STATE_MAX_PATH] = "original";
	LauncherState_extractFilename(NULL, filename);
	TEST_ASSERT_EQUAL_STRING("", filename);
}

///////////////////////////////
// Collation Detection Tests
///////////////////////////////

void test_isCollatedPath_detects_collated(void) {
	TEST_ASSERT_TRUE(LauncherState_isCollatedPath("/Roms/Game Boy (USA)"));
	TEST_ASSERT_TRUE(LauncherState_isCollatedPath("/Roms/Super Nintendo (Japan)"));
}

void test_isCollatedPath_rejects_non_collated(void) {
	TEST_ASSERT_FALSE(LauncherState_isCollatedPath("/Roms/Game Boy"));
	TEST_ASSERT_FALSE(LauncherState_isCollatedPath("/Roms/GB"));
	TEST_ASSERT_FALSE(LauncherState_isCollatedPath("/Roms/Game (incomplete"));
}

void test_isCollatedPath_null_safe(void) {
	TEST_ASSERT_FALSE(LauncherState_isCollatedPath(NULL));
}

void test_getCollationPrefix_extracts_prefix(void) {
	char prefix[LAUNCHER_STATE_MAX_PATH];
	TEST_ASSERT_TRUE(LauncherState_getCollationPrefix("/Roms/Game Boy (USA)", prefix));
	TEST_ASSERT_EQUAL_STRING("/Roms/Game Boy (", prefix);
}

void test_getCollationPrefix_returns_false_for_non_collated(void) {
	char prefix[LAUNCHER_STATE_MAX_PATH];
	TEST_ASSERT_FALSE(LauncherState_getCollationPrefix("/Roms/Game Boy", prefix));
}

///////////////////////////////
// Resume Path Generation Tests
///////////////////////////////

void test_getResumeSlotPath_generates_path(void) {
	char path[LAUNCHER_STATE_MAX_PATH];
	LauncherState_getResumeSlotPath("/Roms/GB/Pokemon.gb", "/.userdata", "gambatte", path);
	TEST_ASSERT_EQUAL_STRING("/.userdata/.launcher/gambatte/Pokemon.gb.txt", path);
}

void test_getResumeSlotPath_null_safe(void) {
	char path[LAUNCHER_STATE_MAX_PATH] = "original";
	LauncherState_getResumeSlotPath(NULL, "/.userdata", "gambatte", path);
	TEST_ASSERT_EQUAL_STRING("", path);
}

void test_buildResumeCommand_creates_command(void) {
	char cmd[LAUNCHER_STATE_MAX_PATH * 2];
	LauncherState_buildResumeCommand("/path/to/emu", "/path/to/rom.gb", cmd);
	TEST_ASSERT_EQUAL_STRING("'/path/to/emu' '/path/to/rom.gb'", cmd);
}

void test_buildResumeCommand_escapes_quotes(void) {
	char cmd[LAUNCHER_STATE_MAX_PATH * 2];
	LauncherState_buildResumeCommand("/path/to/it's/emu", "/path/to/rom.gb", cmd);
	// Single quote becomes '\''
	TEST_ASSERT_TRUE(strstr(cmd, "'\\''") != NULL);
}

///////////////////////////////
// Path Validation Tests
///////////////////////////////

void test_isRecentsPath_matches_exact(void) {
	TEST_ASSERT_TRUE(LauncherState_isRecentsPath("FAUX:RECENT", "FAUX:RECENT"));
	TEST_ASSERT_FALSE(LauncherState_isRecentsPath("/some/path", "FAUX:RECENT"));
}

void test_validatePath_checks_prefix(void) {
	TEST_ASSERT_TRUE(LauncherState_validatePath("/mnt/SDCARD/Roms/game.gb", "/mnt/SDCARD"));
	TEST_ASSERT_FALSE(LauncherState_validatePath("/other/path", "/mnt/SDCARD"));
}

void test_validatePath_requires_content(void) {
	// Just the SD path with nothing after it is not valid
	TEST_ASSERT_FALSE(LauncherState_validatePath("/mnt/SDCARD", "/mnt/SDCARD"));
}

void test_makeAbsolutePath_prepends_sd(void) {
	char path[LAUNCHER_STATE_MAX_PATH];
	LauncherState_makeAbsolutePath("/Roms/GB/game.gb", "/mnt/SDCARD", path);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/game.gb", path);
}

///////////////////////////////
// Quote Escaping Tests
///////////////////////////////

void test_escapeQuotes_no_quotes(void) {
	char escaped[256];
	LauncherState_escapeQuotes("/path/to/file", escaped, sizeof(escaped));
	TEST_ASSERT_EQUAL_STRING("/path/to/file", escaped);
}

void test_escapeQuotes_single_quote(void) {
	char escaped[256];
	LauncherState_escapeQuotes("it's", escaped, sizeof(escaped));
	TEST_ASSERT_EQUAL_STRING("it'\\''s", escaped);
}

void test_escapeQuotes_multiple_quotes(void) {
	char escaped[256];
	LauncherState_escapeQuotes("a'b'c", escaped, sizeof(escaped));
	TEST_ASSERT_EQUAL_STRING("a'\\''b'\\''c", escaped);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Path stack
	RUN_TEST(test_PathStack_new_creates_stack);
	RUN_TEST(test_PathStack_push_adds_items);
	RUN_TEST(test_PathStack_pop_returns_lifo);
	RUN_TEST(test_PathStack_pop_empty_returns_false);
	RUN_TEST(test_PathStack_grows_when_full);

	// Path decomposition
	RUN_TEST(test_decomposePath_creates_stack);
	RUN_TEST(test_decomposePath_stops_at_root);
	RUN_TEST(test_decomposePath_null_returns_null);

	// Filename extraction
	RUN_TEST(test_extractFilename_gets_filename);
	RUN_TEST(test_extractFilename_handles_no_slash);
	RUN_TEST(test_extractFilename_null_safe);

	// Collation detection
	RUN_TEST(test_isCollatedPath_detects_collated);
	RUN_TEST(test_isCollatedPath_rejects_non_collated);
	RUN_TEST(test_isCollatedPath_null_safe);
	RUN_TEST(test_getCollationPrefix_extracts_prefix);
	RUN_TEST(test_getCollationPrefix_returns_false_for_non_collated);

	// Resume path generation
	RUN_TEST(test_getResumeSlotPath_generates_path);
	RUN_TEST(test_getResumeSlotPath_null_safe);
	RUN_TEST(test_buildResumeCommand_creates_command);
	RUN_TEST(test_buildResumeCommand_escapes_quotes);

	// Path validation
	RUN_TEST(test_isRecentsPath_matches_exact);
	RUN_TEST(test_validatePath_checks_prefix);
	RUN_TEST(test_validatePath_requires_content);
	RUN_TEST(test_makeAbsolutePath_prepends_sd);

	// Quote escaping
	RUN_TEST(test_escapeQuotes_no_quotes);
	RUN_TEST(test_escapeQuotes_single_quote);
	RUN_TEST(test_escapeQuotes_multiple_quotes);

	return UNITY_END();
}
