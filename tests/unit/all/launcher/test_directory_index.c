/**
 * test_directory_index.c - Unit tests for directory indexing
 *
 * Tests the directory indexing algorithm components:
 * - Alphabetical character indexing
 * - Unique name generation for duplicates
 * - Map.txt alias application
 * - Hidden entry filtering
 * - Duplicate name detection and marking
 * - Alpha index building for L1/R1 navigation
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "unity.h"
#include "directory_index.h"
#include "launcher_entry.h"
#include "launcher_map.h"
#include "../../../../workspace/all/common/defines.h"
#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>

// Test paths must use ROMS_PATH prefix for getEmuName() to work
#define TEST_ROM_PATH(system, file) ROMS_PATH "/" system "/" file

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// DirectoryIndex_getAlphaChar tests
///////////////////////////////

void test_getAlphaChar_returns_1_for_A(void) {
	TEST_ASSERT_EQUAL(1, DirectoryIndex_getAlphaChar("Apple"));
	TEST_ASSERT_EQUAL(1, DirectoryIndex_getAlphaChar("apple"));
	TEST_ASSERT_EQUAL(1, DirectoryIndex_getAlphaChar("APPLE"));
}

void test_getAlphaChar_returns_26_for_Z(void) {
	TEST_ASSERT_EQUAL(26, DirectoryIndex_getAlphaChar("Zelda"));
	TEST_ASSERT_EQUAL(26, DirectoryIndex_getAlphaChar("zelda"));
}

void test_getAlphaChar_returns_0_for_number(void) {
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar("123 Game"));
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar("007 Agent"));
}

void test_getAlphaChar_returns_0_for_symbol(void) {
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar("!Special"));
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar("@Game"));
}

void test_getAlphaChar_returns_0_for_empty(void) {
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar(""));
}

void test_getAlphaChar_returns_0_for_null(void) {
	TEST_ASSERT_EQUAL(0, DirectoryIndex_getAlphaChar(NULL));
}

void test_getAlphaChar_all_letters(void) {
	// Test all 26 letters
	TEST_ASSERT_EQUAL(1, DirectoryIndex_getAlphaChar("A"));
	TEST_ASSERT_EQUAL(2, DirectoryIndex_getAlphaChar("B"));
	TEST_ASSERT_EQUAL(3, DirectoryIndex_getAlphaChar("C"));
	TEST_ASSERT_EQUAL(13, DirectoryIndex_getAlphaChar("M"));
	TEST_ASSERT_EQUAL(14, DirectoryIndex_getAlphaChar("N"));
	TEST_ASSERT_EQUAL(26, DirectoryIndex_getAlphaChar("Z"));
}

///////////////////////////////
// DirectoryIndex_getUniqueName tests
///////////////////////////////

void test_getUniqueName_appends_emu_tag(void) {
	char result[256];
	DirectoryIndex_getUniqueName("Tetris", TEST_ROM_PATH("GB", "Tetris.gb"), result);
	TEST_ASSERT_EQUAL_STRING("Tetris (GB)", result);
}

void test_getUniqueName_different_systems(void) {
	char result1[256];
	char result2[256];
	DirectoryIndex_getUniqueName("Tetris", TEST_ROM_PATH("GB", "Tetris.gb"), result1);
	DirectoryIndex_getUniqueName("Tetris", TEST_ROM_PATH("NES", "Tetris.nes"), result2);
	TEST_ASSERT_EQUAL_STRING("Tetris (GB)", result1);
	TEST_ASSERT_EQUAL_STRING("Tetris (NES)", result2);
}

void test_getUniqueName_gba_system(void) {
	char result[256];
	DirectoryIndex_getUniqueName("Pokemon", TEST_ROM_PATH("GBA", "Pokemon.gba"), result);
	TEST_ASSERT_EQUAL_STRING("Pokemon (GBA)", result);
}

///////////////////////////////
// DirectoryIndex_applyAliases tests
///////////////////////////////

void test_applyAliases_updates_name(void) {
	Entry** entries = NULL;
	Entry* e = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	arrpush(entries, e);

	MapEntry* map = NULL;
	sh_new_strdup(map);
	shput(map, "game.gb", strdup("Custom Name"));

	int result = DirectoryIndex_applyAliases(entries, map);

	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("Custom Name", e->name);

	Map_free(map);
	EntryArray_free(entries);
}

void test_applyAliases_no_match_returns_0(void) {
	Entry** entries = NULL;
	Entry* e = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	arrpush(entries, e);

	MapEntry* map = NULL;
	sh_new_strdup(map);
	shput(map, "other.gb", strdup("Other Name"));

	int result = DirectoryIndex_applyAliases(entries, map);

	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING("game", e->name);

	Map_free(map);
	EntryArray_free(entries);
}

void test_applyAliases_multiple_entries(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/a.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/b.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/c.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	MapEntry* map = NULL;
	sh_new_strdup(map);
	shput(map, "a.gb", strdup("Alpha"));
	shput(map, "c.gb", strdup("Charlie"));

	DirectoryIndex_applyAliases(entries, map);

	TEST_ASSERT_EQUAL_STRING("Alpha", e1->name);
	TEST_ASSERT_EQUAL_STRING("b", e2->name);
	TEST_ASSERT_EQUAL_STRING("Charlie", e3->name);

	Map_free(map);
	EntryArray_free(entries);
}

void test_applyAliases_null_map_returns_0(void) {
	Entry** entries = NULL;
	Entry* e = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	arrpush(entries, e);

	int result = DirectoryIndex_applyAliases(entries, NULL);

	TEST_ASSERT_EQUAL(0, result);

	EntryArray_free(entries);
}

void test_applyAliases_null_entries_returns_0(void) {
	MapEntry* map = NULL;
	sh_new_strdup(map);
	shput(map, "game.gb", strdup("Name"));

	int result = DirectoryIndex_applyAliases(NULL, map);

	TEST_ASSERT_EQUAL(0, result);

	Map_free(map);
}

///////////////////////////////
// DirectoryIndex_filterHidden tests
///////////////////////////////

void test_filterHidden_removes_dot_prefix(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/visible.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/hidden.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	// Manually set hidden name
	Entry_setName(e2, ".hidden");

	Entry** result = DirectoryIndex_filterHidden(entries);

	TEST_ASSERT_EQUAL(1, arrlen(result));
	TEST_ASSERT_EQUAL_STRING("visible", result[0]->name);

	arrfree(entries);
	EntryArray_free(result);
}

void test_filterHidden_removes_disabled_suffix(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/active.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/inactive.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	// Manually set disabled name
	Entry_setName(e2, "inactive.disabled");

	Entry** result = DirectoryIndex_filterHidden(entries);

	TEST_ASSERT_EQUAL(1, arrlen(result));
	TEST_ASSERT_EQUAL_STRING("active", result[0]->name);

	arrfree(entries);
	EntryArray_free(result);
}

void test_filterHidden_keeps_all_visible(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/a.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/b.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/c.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	Entry** result = DirectoryIndex_filterHidden(entries);

	TEST_ASSERT_EQUAL(3, arrlen(result));

	arrfree(entries);
	EntryArray_free(result);
}

void test_filterHidden_removes_all_hidden(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/a.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/b.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	Entry_setName(e1, ".hidden1");
	Entry_setName(e2, ".hidden2");

	Entry** result = DirectoryIndex_filterHidden(entries);

	TEST_ASSERT_EQUAL(0, arrlen(result));

	arrfree(entries);
	arrfree(result);
}

void test_filterHidden_null_returns_null(void) {
	Entry** result = DirectoryIndex_filterHidden(NULL);
	TEST_ASSERT_NULL(result);
}

///////////////////////////////
// DirectoryIndex_markDuplicates tests
///////////////////////////////

void test_markDuplicates_no_duplicates(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/Mario.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Zelda.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	DirectoryIndex_markDuplicates(entries);

	TEST_ASSERT_NULL(e1->unique);
	TEST_ASSERT_NULL(e2->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_different_filenames(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/tetris_v1.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/tetris_v2.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	// Set same display name
	Entry_setName(e1, "Tetris");
	Entry_setName(e2, "Tetris");

	DirectoryIndex_markDuplicates(entries);

	// Should use filenames to disambiguate
	TEST_ASSERT_EQUAL_STRING("tetris_v1.gb", e1->unique);
	TEST_ASSERT_EQUAL_STRING("tetris_v2.gb", e2->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_same_filename_different_systems(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/Tetris.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/NES/Tetris.nes", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	// Set same display name (as would happen after extension stripping)
	Entry_setName(e1, "Tetris");
	Entry_setName(e2, "Tetris");

	DirectoryIndex_markDuplicates(entries);

	// Filenames differ, so use filenames
	TEST_ASSERT_EQUAL_STRING("Tetris.gb", e1->unique);
	TEST_ASSERT_EQUAL_STRING("Tetris.nes", e2->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_same_filename_uses_emu(void) {
	Entry** entries = NULL;
	// Same filename in different system folders (cross-platform ROM)
	Entry* e1 = Entry_new(TEST_ROM_PATH("GB", "Tetris.zip"), ENTRY_ROM);
	Entry* e2 = Entry_new(TEST_ROM_PATH("NES", "Tetris.zip"), ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	Entry_setName(e1, "Tetris");
	Entry_setName(e2, "Tetris");

	DirectoryIndex_markDuplicates(entries);

	// Same filename, so use emulator name
	TEST_ASSERT_EQUAL_STRING("Tetris (GB)", e1->unique);
	TEST_ASSERT_EQUAL_STRING("Tetris (NES)", e2->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_three_way(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GBC/game.gbc", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GBA/game.gba", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	Entry_setName(e1, "Game");
	Entry_setName(e2, "Game");
	Entry_setName(e3, "Game");

	DirectoryIndex_markDuplicates(entries);

	// First pair: e1 and e2 are duplicates
	TEST_ASSERT_NOT_NULL(e1->unique);
	TEST_ASSERT_NOT_NULL(e2->unique);
	// Second pair: e2 and e3 are duplicates (e2 gets overwritten)
	TEST_ASSERT_NOT_NULL(e3->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_null_entries(void) {
	// Should not crash
	DirectoryIndex_markDuplicates(NULL);
}

void test_markDuplicates_single_entry(void) {
	Entry** entries = NULL;
	Entry* e = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	arrpush(entries, e);

	DirectoryIndex_markDuplicates(entries);

	TEST_ASSERT_NULL(e->unique);

	EntryArray_free(entries);
}

void test_markDuplicates_empty_array(void) {
	Entry** entries = NULL;

	// Should not crash
	DirectoryIndex_markDuplicates(entries);

	arrfree(entries);
}

///////////////////////////////
// DirectoryIndex_buildAlphaIndex tests
///////////////////////////////

void test_buildAlphaIndex_single_letter(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/Aardvark.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Apple.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Aztec.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	IntArray* alphas = IntArray_new();
	DirectoryIndex_buildAlphaIndex(entries, alphas);

	// All entries start with A, so one alpha group
	TEST_ASSERT_EQUAL(1, alphas->count);
	TEST_ASSERT_EQUAL(0, alphas->items[0]); // First entry at index 0

	// All entries should have same alpha
	TEST_ASSERT_EQUAL(0, e1->alpha);
	TEST_ASSERT_EQUAL(0, e2->alpha);
	TEST_ASSERT_EQUAL(0, e3->alpha);

	IntArray_free(alphas);
	EntryArray_free(entries);
}

void test_buildAlphaIndex_multiple_letters(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/Apple.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Banana.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Cherry.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	IntArray* alphas = IntArray_new();
	DirectoryIndex_buildAlphaIndex(entries, alphas);

	// Three letters: A, B, C
	TEST_ASSERT_EQUAL(3, alphas->count);
	TEST_ASSERT_EQUAL(0, alphas->items[0]); // A at index 0
	TEST_ASSERT_EQUAL(1, alphas->items[1]); // B at index 1
	TEST_ASSERT_EQUAL(2, alphas->items[2]); // C at index 2

	// Each entry has different alpha
	TEST_ASSERT_EQUAL(0, e1->alpha);
	TEST_ASSERT_EQUAL(1, e2->alpha);
	TEST_ASSERT_EQUAL(2, e3->alpha);

	IntArray_free(alphas);
	EntryArray_free(entries);
}

void test_buildAlphaIndex_with_numbers(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/123 Game.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Apple.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	IntArray* alphas = IntArray_new();
	DirectoryIndex_buildAlphaIndex(entries, alphas);

	// Two groups: # (0) and A (1)
	TEST_ASSERT_EQUAL(2, alphas->count);
	TEST_ASSERT_EQUAL(0, alphas->items[0]); // # at index 0
	TEST_ASSERT_EQUAL(1, alphas->items[1]); // A at index 1

	TEST_ASSERT_EQUAL(0, e1->alpha);
	TEST_ASSERT_EQUAL(1, e2->alpha);

	IntArray_free(alphas);
	EntryArray_free(entries);
}

void test_buildAlphaIndex_mixed_letters(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/Apple.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Apricot.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Banana.gb", ENTRY_ROM);
	Entry* e4 = Entry_new("/Roms/GB/Blueberry.gb", ENTRY_ROM);
	Entry* e5 = Entry_new("/Roms/GB/Cantaloupe.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);
	arrpush(entries, e4);
	arrpush(entries, e5);

	IntArray* alphas = IntArray_new();
	DirectoryIndex_buildAlphaIndex(entries, alphas);

	// Three letters: A, B, C
	TEST_ASSERT_EQUAL(3, alphas->count);
	TEST_ASSERT_EQUAL(0, alphas->items[0]); // A at index 0
	TEST_ASSERT_EQUAL(2, alphas->items[1]); // B at index 2
	TEST_ASSERT_EQUAL(4, alphas->items[2]); // C at index 4

	// A entries
	TEST_ASSERT_EQUAL(0, e1->alpha);
	TEST_ASSERT_EQUAL(0, e2->alpha);
	// B entries
	TEST_ASSERT_EQUAL(1, e3->alpha);
	TEST_ASSERT_EQUAL(1, e4->alpha);
	// C entries
	TEST_ASSERT_EQUAL(2, e5->alpha);

	IntArray_free(alphas);
	EntryArray_free(entries);
}

void test_buildAlphaIndex_null_entries(void) {
	IntArray* alphas = IntArray_new();

	// Should not crash
	DirectoryIndex_buildAlphaIndex(NULL, alphas);

	TEST_ASSERT_EQUAL(0, alphas->count);

	IntArray_free(alphas);
}

void test_buildAlphaIndex_null_alphas(void) {
	Entry** entries = NULL;
	Entry* e = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	arrpush(entries, e);

	// Should not crash
	DirectoryIndex_buildAlphaIndex(entries, NULL);

	EntryArray_free(entries);
}

void test_buildAlphaIndex_empty_entries(void) {
	Entry** entries = NULL;
	IntArray* alphas = IntArray_new();

	DirectoryIndex_buildAlphaIndex(entries, alphas);

	TEST_ASSERT_EQUAL(0, alphas->count);

	IntArray_free(alphas);
	arrfree(entries);
}

///////////////////////////////
// DirectoryIndex_index integration tests
///////////////////////////////

void test_index_full_workflow(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/zelda.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/mario.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/hidden.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);
	arrpush(entries, e3);

	// Create map with alias and hidden entry
	MapEntry* map = NULL;
	sh_new_strdup(map);
	shput(map, "zelda.gb", strdup("The Legend of Zelda"));
	shput(map, "hidden.gb", strdup(".hidden"));

	IntArray* alphas = IntArray_new();

	Entry** result = DirectoryIndex_index(entries, alphas, map, 0);

	// Should have 2 entries (hidden removed)
	TEST_ASSERT_EQUAL(2, arrlen(result));

	// Should be sorted: "Legend of Zelda" (sort_key) < "mario"
	// "The Legend of Zelda" sorts under L, before M
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", result[0]->name);
	TEST_ASSERT_EQUAL_STRING("mario", result[1]->name);

	// Alpha index should have 2 groups (L and M)
	TEST_ASSERT_EQUAL(2, alphas->count);

	Map_free(map);
	IntArray_free(alphas);
	EntryArray_free(result);
}

void test_index_no_map(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/apple.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/banana.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	IntArray* alphas = IntArray_new();

	Entry** result = DirectoryIndex_index(entries, alphas, NULL, 0);

	TEST_ASSERT_EQUAL(2, arrlen(result));
	TEST_ASSERT_EQUAL(2, alphas->count);

	IntArray_free(alphas);
	EntryArray_free(result);
}

void test_index_skip_alpha(void) {
	Entry** entries = NULL;
	Entry* e1 = Entry_new("/Roms/GB/apple.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/banana.gb", ENTRY_ROM);
	arrpush(entries, e1);
	arrpush(entries, e2);

	IntArray* alphas = IntArray_new();

	Entry** result = DirectoryIndex_index(entries, alphas, NULL, 1);

	TEST_ASSERT_EQUAL(2, arrlen(result));
	// Alpha index should not be built
	TEST_ASSERT_EQUAL(0, alphas->count);

	IntArray_free(alphas);
	EntryArray_free(result);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// DirectoryIndex_getAlphaChar tests
	RUN_TEST(test_getAlphaChar_returns_1_for_A);
	RUN_TEST(test_getAlphaChar_returns_26_for_Z);
	RUN_TEST(test_getAlphaChar_returns_0_for_number);
	RUN_TEST(test_getAlphaChar_returns_0_for_symbol);
	RUN_TEST(test_getAlphaChar_returns_0_for_empty);
	RUN_TEST(test_getAlphaChar_returns_0_for_null);
	RUN_TEST(test_getAlphaChar_all_letters);

	// DirectoryIndex_getUniqueName tests
	RUN_TEST(test_getUniqueName_appends_emu_tag);
	RUN_TEST(test_getUniqueName_different_systems);
	RUN_TEST(test_getUniqueName_gba_system);

	// DirectoryIndex_applyAliases tests
	RUN_TEST(test_applyAliases_updates_name);
	RUN_TEST(test_applyAliases_no_match_returns_0);
	RUN_TEST(test_applyAliases_multiple_entries);
	RUN_TEST(test_applyAliases_null_map_returns_0);
	RUN_TEST(test_applyAliases_null_entries_returns_0);

	// DirectoryIndex_filterHidden tests
	RUN_TEST(test_filterHidden_removes_dot_prefix);
	RUN_TEST(test_filterHidden_removes_disabled_suffix);
	RUN_TEST(test_filterHidden_keeps_all_visible);
	RUN_TEST(test_filterHidden_removes_all_hidden);
	RUN_TEST(test_filterHidden_null_returns_null);

	// DirectoryIndex_markDuplicates tests
	RUN_TEST(test_markDuplicates_no_duplicates);
	RUN_TEST(test_markDuplicates_different_filenames);
	RUN_TEST(test_markDuplicates_same_filename_different_systems);
	RUN_TEST(test_markDuplicates_same_filename_uses_emu);
	RUN_TEST(test_markDuplicates_three_way);
	RUN_TEST(test_markDuplicates_null_entries);
	RUN_TEST(test_markDuplicates_single_entry);
	RUN_TEST(test_markDuplicates_empty_array);

	// DirectoryIndex_buildAlphaIndex tests
	RUN_TEST(test_buildAlphaIndex_single_letter);
	RUN_TEST(test_buildAlphaIndex_multiple_letters);
	RUN_TEST(test_buildAlphaIndex_with_numbers);
	RUN_TEST(test_buildAlphaIndex_mixed_letters);
	RUN_TEST(test_buildAlphaIndex_null_entries);
	RUN_TEST(test_buildAlphaIndex_null_alphas);
	RUN_TEST(test_buildAlphaIndex_empty_entries);

	// Integration tests
	RUN_TEST(test_index_full_workflow);
	RUN_TEST(test_index_no_map);
	RUN_TEST(test_index_skip_alpha);

	return UNITY_END();
}
