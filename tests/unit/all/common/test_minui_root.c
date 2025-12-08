/**
 * test_minui_root.c - Unit tests for root directory generation
 *
 * Tests alias parsing, hidden file detection, deduplication,
 * and entry sorting utilities.
 *
 * 25 tests organized by functionality.
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minui_root.h"

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
// Alias List Tests
///////////////////////////////

void test_AliasList_new_creates_list(void) {
	MinUIAliasList* list = MinUIAliasList_new(10);
	TEST_ASSERT_NOT_NULL(list);
	TEST_ASSERT_EQUAL(0, list->count);
	MinUIAliasList_free(list);
}

void test_AliasList_add_stores_alias(void) {
	MinUIAliasList* list = MinUIAliasList_new(10);
	TEST_ASSERT_TRUE(MinUIAliasList_add(list, "GB", "Game Boy"));
	TEST_ASSERT_EQUAL(1, list->count);
	MinUIAliasList_free(list);
}

void test_AliasList_get_finds_alias(void) {
	MinUIAliasList* list = MinUIAliasList_new(10);
	MinUIAliasList_add(list, "GB", "Game Boy");
	MinUIAliasList_add(list, "GBA", "Game Boy Advance");

	TEST_ASSERT_EQUAL_STRING("Game Boy", MinUIAliasList_get(list, "GB"));
	TEST_ASSERT_EQUAL_STRING("Game Boy Advance", MinUIAliasList_get(list, "GBA"));
	MinUIAliasList_free(list);
}

void test_AliasList_get_returns_null_for_missing(void) {
	MinUIAliasList* list = MinUIAliasList_new(10);
	MinUIAliasList_add(list, "GB", "Game Boy");

	TEST_ASSERT_NULL(MinUIAliasList_get(list, "NES"));
	MinUIAliasList_free(list);
}

void test_AliasList_grows_when_full(void) {
	MinUIAliasList* list = MinUIAliasList_new(2);
	TEST_ASSERT_TRUE(MinUIAliasList_add(list, "A", "Alpha"));
	TEST_ASSERT_TRUE(MinUIAliasList_add(list, "B", "Beta"));
	TEST_ASSERT_TRUE(MinUIAliasList_add(list, "C", "Gamma")); // Should grow
	TEST_ASSERT_EQUAL(3, list->count);
	MinUIAliasList_free(list);
}

///////////////////////////////
// Map Line Parsing Tests
///////////////////////////////

void test_parseMapLine_parses_valid_line(void) {
	char key[MINUI_ROOT_MAX_NAME], value[MINUI_ROOT_MAX_NAME];
	TEST_ASSERT_TRUE(MinUIRoot_parseMapLine("GB\tGame Boy", key, value));
	TEST_ASSERT_EQUAL_STRING("GB", key);
	TEST_ASSERT_EQUAL_STRING("Game Boy", value);
}

void test_parseMapLine_handles_trailing_newline(void) {
	char key[MINUI_ROOT_MAX_NAME], value[MINUI_ROOT_MAX_NAME];
	TEST_ASSERT_TRUE(MinUIRoot_parseMapLine("GB\tGame Boy\n", key, value));
	TEST_ASSERT_EQUAL_STRING("Game Boy", value);
}

void test_parseMapLine_rejects_empty_line(void) {
	char key[MINUI_ROOT_MAX_NAME], value[MINUI_ROOT_MAX_NAME];
	TEST_ASSERT_FALSE(MinUIRoot_parseMapLine("", key, value));
	TEST_ASSERT_FALSE(MinUIRoot_parseMapLine("\n", key, value));
}

void test_parseMapLine_rejects_no_tab(void) {
	char key[MINUI_ROOT_MAX_NAME], value[MINUI_ROOT_MAX_NAME];
	TEST_ASSERT_FALSE(MinUIRoot_parseMapLine("GB Game Boy", key, value));
}

///////////////////////////////
// Hidden File Detection Tests
///////////////////////////////

void test_isHidden_detects_dot_files(void) {
	TEST_ASSERT_TRUE(MinUIRoot_isHidden(".hidden"));
	TEST_ASSERT_TRUE(MinUIRoot_isHidden(".DS_Store"));
}

void test_isHidden_detects_dot_and_dotdot(void) {
	TEST_ASSERT_TRUE(MinUIRoot_isHidden("."));
	TEST_ASSERT_TRUE(MinUIRoot_isHidden(".."));
}

void test_isHidden_allows_regular_files(void) {
	TEST_ASSERT_FALSE(MinUIRoot_isHidden("Pokemon.gb"));
	TEST_ASSERT_FALSE(MinUIRoot_isHidden("Game Boy"));
}

///////////////////////////////
// Name Matching Tests
///////////////////////////////

void test_namesMatch_matches_exact(void) {
	TEST_ASSERT_TRUE(MinUIRoot_namesMatch("Game Boy", "Game Boy"));
}

void test_namesMatch_rejects_different(void) {
	TEST_ASSERT_FALSE(MinUIRoot_namesMatch("Game Boy", "Game Boy Advance"));
}

///////////////////////////////
// Display Name Tests
///////////////////////////////

void test_getDisplayName_strips_numeric_prefix(void) {
	char name[MINUI_ROOT_MAX_NAME];
	MinUIRoot_getDisplayName("001) Game Boy", name);
	TEST_ASSERT_EQUAL_STRING("Game Boy", name);
}

void test_getDisplayName_keeps_name_without_prefix(void) {
	char name[MINUI_ROOT_MAX_NAME];
	MinUIRoot_getDisplayName("Game Boy", name);
	TEST_ASSERT_EQUAL_STRING("Game Boy", name);
}

void test_getDisplayName_keeps_non_numeric_paren(void) {
	char name[MINUI_ROOT_MAX_NAME];
	MinUIRoot_getDisplayName("Game Boy (USA)", name);
	TEST_ASSERT_EQUAL_STRING("Game Boy (USA)", name);
}

///////////////////////////////
// Filename Extraction Tests
///////////////////////////////

void test_extractFilename_gets_filename(void) {
	char filename[MINUI_ROOT_MAX_NAME];
	MinUIRoot_extractFilename("/mnt/SDCARD/Roms/GB", filename);
	TEST_ASSERT_EQUAL_STRING("GB", filename);
}

void test_extractFilename_handles_no_slash(void) {
	char filename[MINUI_ROOT_MAX_NAME];
	MinUIRoot_extractFilename("Pokemon.gb", filename);
	TEST_ASSERT_EQUAL_STRING("Pokemon.gb", filename);
}

///////////////////////////////
// Deduplication Tests
///////////////////////////////

void test_deduplicateEntries_marks_duplicates(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/GB (USA)", .name = "Game Boy", .type = 0, .visible = 1},
	    {.path = "/Roms/GB (Japan)", .name = "Game Boy", .type = 0, .visible = 1},
	    {.path = "/Roms/GBA", .name = "Game Boy Advance", .type = 0, .visible = 1},
	};

	int visible = MinUIRoot_deduplicateEntries(entries, 3);

	TEST_ASSERT_EQUAL(2, visible);
	TEST_ASSERT_EQUAL(1, entries[0].visible);
	TEST_ASSERT_EQUAL(0, entries[1].visible); // Duplicate
	TEST_ASSERT_EQUAL(1, entries[2].visible);
}

void test_deduplicateEntries_handles_no_duplicates(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/GB", .name = "Game Boy", .type = 0, .visible = 1},
	    {.path = "/Roms/GBA", .name = "Game Boy Advance", .type = 0, .visible = 1},
	    {.path = "/Roms/NES", .name = "Nintendo", .type = 0, .visible = 1},
	};

	int visible = MinUIRoot_deduplicateEntries(entries, 3);

	TEST_ASSERT_EQUAL(3, visible);
}

///////////////////////////////
// Alias Application Tests
///////////////////////////////

void test_applyAliases_renames_entries(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/GB", .name = "GB", .type = 0, .visible = 1},
	    {.path = "/Roms/GBA", .name = "GBA", .type = 0, .visible = 1},
	};

	MinUIAliasList* aliases = MinUIAliasList_new(10);
	MinUIAliasList_add(aliases, "GB", "Game Boy");
	MinUIAliasList_add(aliases, "GBA", "Game Boy Advance");

	int renamed = MinUIRoot_applyAliases(entries, 2, aliases);

	TEST_ASSERT_EQUAL(2, renamed);
	TEST_ASSERT_EQUAL_STRING("Game Boy", entries[0].name);
	TEST_ASSERT_EQUAL_STRING("Game Boy Advance", entries[1].name);

	MinUIAliasList_free(aliases);
}

void test_applyAliases_partial_rename(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/GB", .name = "GB", .type = 0, .visible = 1},
	    {.path = "/Roms/NES", .name = "NES", .type = 0, .visible = 1},
	};

	MinUIAliasList* aliases = MinUIAliasList_new(10);
	MinUIAliasList_add(aliases, "GB", "Game Boy");

	int renamed = MinUIRoot_applyAliases(entries, 2, aliases);

	TEST_ASSERT_EQUAL(1, renamed);
	TEST_ASSERT_EQUAL_STRING("Game Boy", entries[0].name);
	TEST_ASSERT_EQUAL_STRING("NES", entries[1].name); // Unchanged
	MinUIAliasList_free(aliases);
}

///////////////////////////////
// System Directory Validation Tests
///////////////////////////////

void test_isValidSystemDir_accepts_valid(void) {
	TEST_ASSERT_TRUE(MinUIRoot_isValidSystemDir("GB"));
	TEST_ASSERT_TRUE(MinUIRoot_isValidSystemDir("Game Boy (USA)"));
	TEST_ASSERT_TRUE(MinUIRoot_isValidSystemDir("001) Nintendo"));
}

void test_isValidSystemDir_rejects_hidden(void) {
	TEST_ASSERT_FALSE(MinUIRoot_isValidSystemDir(".hidden"));
	TEST_ASSERT_FALSE(MinUIRoot_isValidSystemDir(""));
}

///////////////////////////////
// Entry Sorting Tests
///////////////////////////////

void test_sortEntries_sorts_alphabetically(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/NES", .name = "Nintendo", .type = 0, .visible = 1},
	    {.path = "/Roms/GB", .name = "Game Boy", .type = 0, .visible = 1},
	    {.path = "/Roms/SNES", .name = "Super Nintendo", .type = 0, .visible = 1},
	};

	MinUIRoot_sortEntries(entries, 3);

	TEST_ASSERT_EQUAL_STRING("Game Boy", entries[0].name);
	TEST_ASSERT_EQUAL_STRING("Nintendo", entries[1].name);
	TEST_ASSERT_EQUAL_STRING("Super Nintendo", entries[2].name);
}

void test_sortEntries_case_insensitive(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/Roms/Z", .name = "zebra", .type = 0, .visible = 1},
	    {.path = "/Roms/A", .name = "Alpha", .type = 0, .visible = 1},
	    {.path = "/Roms/B", .name = "beta", .type = 0, .visible = 1},
	};

	MinUIRoot_sortEntries(entries, 3);

	TEST_ASSERT_EQUAL_STRING("Alpha", entries[0].name);
	TEST_ASSERT_EQUAL_STRING("beta", entries[1].name);
	TEST_ASSERT_EQUAL_STRING("zebra", entries[2].name);
}

///////////////////////////////
// Counting Tests
///////////////////////////////

void test_countVisible_counts_correctly(void) {
	MinUIRootEntry entries[] = {
	    {.path = "/a", .name = "A", .type = 0, .visible = 1},
	    {.path = "/b", .name = "B", .type = 0, .visible = 0},
	    {.path = "/c", .name = "C", .type = 0, .visible = 1},
	    {.path = "/d", .name = "D", .type = 0, .visible = 0},
	};

	TEST_ASSERT_EQUAL(2, MinUIRoot_countVisible(entries, 4));
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Alias list
	RUN_TEST(test_AliasList_new_creates_list);
	RUN_TEST(test_AliasList_add_stores_alias);
	RUN_TEST(test_AliasList_get_finds_alias);
	RUN_TEST(test_AliasList_get_returns_null_for_missing);
	RUN_TEST(test_AliasList_grows_when_full);

	// Map line parsing
	RUN_TEST(test_parseMapLine_parses_valid_line);
	RUN_TEST(test_parseMapLine_handles_trailing_newline);
	RUN_TEST(test_parseMapLine_rejects_empty_line);
	RUN_TEST(test_parseMapLine_rejects_no_tab);

	// Hidden file detection
	RUN_TEST(test_isHidden_detects_dot_files);
	RUN_TEST(test_isHidden_detects_dot_and_dotdot);
	RUN_TEST(test_isHidden_allows_regular_files);

	// Name matching
	RUN_TEST(test_namesMatch_matches_exact);
	RUN_TEST(test_namesMatch_rejects_different);

	// Display name
	RUN_TEST(test_getDisplayName_strips_numeric_prefix);
	RUN_TEST(test_getDisplayName_keeps_name_without_prefix);
	RUN_TEST(test_getDisplayName_keeps_non_numeric_paren);

	// Filename extraction
	RUN_TEST(test_extractFilename_gets_filename);
	RUN_TEST(test_extractFilename_handles_no_slash);

	// Deduplication
	RUN_TEST(test_deduplicateEntries_marks_duplicates);
	RUN_TEST(test_deduplicateEntries_handles_no_duplicates);

	// Alias application
	RUN_TEST(test_applyAliases_renames_entries);
	RUN_TEST(test_applyAliases_partial_rename);

	// System directory validation
	RUN_TEST(test_isValidSystemDir_accepts_valid);
	RUN_TEST(test_isValidSystemDir_rejects_hidden);

	// Sorting
	RUN_TEST(test_sortEntries_sorts_alphabetically);
	RUN_TEST(test_sortEntries_case_insensitive);

	// Counting
	RUN_TEST(test_countVisible_counts_correctly);

	return UNITY_END();
}
