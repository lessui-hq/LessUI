/**
 * test_minui_entry.c - Unit tests for MinUI Entry type
 *
 * Tests Entry creation, modification, and array operations.
 *
 * Test coverage:
 * - Entry_new - Create entries from paths
 * - Entry_free - Memory cleanup
 * - Entry_setName - Name and sort key updates
 * - EntryArray_indexOf - Search by path
 * - EntryArray_sort - Natural sort order
 * - IntArray operations - Fixed-size integer array
 */

#define _POSIX_C_SOURCE 200809L // Required for strdup()

#include "../../support/unity/unity.h"
#include "../../../../workspace/all/common/minui_entry.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// Entry_new tests
///////////////////////////////

void test_Entry_new_creates_entry(void) {
	Entry* entry = Entry_new("/Roms/GB/Tetris.gb", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/Tetris.gb", entry->path);
	TEST_ASSERT_EQUAL_STRING("Tetris", entry->name);
	TEST_ASSERT_EQUAL_STRING("Tetris", entry->sort_key);
	TEST_ASSERT_NULL(entry->unique);
	TEST_ASSERT_EQUAL(ENTRY_ROM, entry->type);
	TEST_ASSERT_EQUAL(0, entry->alpha);
	Entry_free(entry);
}

void test_Entry_new_strips_extension(void) {
	Entry* entry = Entry_new("/Roms/GBA/Pokemon.gba", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("Pokemon", entry->name);
	Entry_free(entry);
}

void test_Entry_new_strips_region_codes(void) {
	Entry* entry = Entry_new("/Roms/NES/Super Mario Bros (USA).nes", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", entry->name);
	Entry_free(entry);
}

void test_Entry_new_handles_directory(void) {
	Entry* entry = Entry_new("/Roms/GB", ENTRY_DIR);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("GB", entry->name);
	TEST_ASSERT_EQUAL(ENTRY_DIR, entry->type);
	Entry_free(entry);
}

void test_Entry_new_handles_pak(void) {
	Entry* entry = Entry_new("/Roms/Tools/Clock.pak", ENTRY_PAK);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("Clock", entry->name);
	TEST_ASSERT_EQUAL(ENTRY_PAK, entry->type);
	Entry_free(entry);
}

void test_Entry_new_strips_leading_article_for_sort(void) {
	Entry* entry = Entry_new("/Roms/NES/The Legend of Zelda.nes", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", entry->name);
	TEST_ASSERT_EQUAL_STRING("Legend of Zelda", entry->sort_key);
	Entry_free(entry);
}

void test_Entry_new_handles_A_article(void) {
	Entry* entry = Entry_new("/Roms/SNES/A Link to the Past.sfc", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);
	TEST_ASSERT_EQUAL_STRING("A Link to the Past", entry->name);
	TEST_ASSERT_EQUAL_STRING("Link to the Past", entry->sort_key);
	Entry_free(entry);
}

///////////////////////////////
// Entry_setName tests
///////////////////////////////

void test_Entry_setName_updates_name(void) {
	Entry* entry = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);

	int result = Entry_setName(entry, "Custom Name");
	TEST_ASSERT_EQUAL(1, result);
	TEST_ASSERT_EQUAL_STRING("Custom Name", entry->name);
	TEST_ASSERT_EQUAL_STRING("Custom Name", entry->sort_key);

	Entry_free(entry);
}

void test_Entry_setName_updates_sort_key_with_article(void) {
	Entry* entry = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);

	Entry_setName(entry, "The Best Game");
	TEST_ASSERT_EQUAL_STRING("The Best Game", entry->name);
	TEST_ASSERT_EQUAL_STRING("Best Game", entry->sort_key);

	Entry_free(entry);
}

void test_Entry_setName_preserves_path(void) {
	Entry* entry = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);

	Entry_setName(entry, "New Name");
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/game.gb", entry->path);

	Entry_free(entry);
}

///////////////////////////////
// Entry_free tests
///////////////////////////////

void test_Entry_free_handles_null(void) {
	// Should not crash
	Entry_free(NULL);
}

void test_Entry_free_handles_entry_with_unique(void) {
	Entry* entry = Entry_new("/Roms/GB/game.gb", ENTRY_ROM);
	TEST_ASSERT_NOT_NULL(entry);

	entry->unique = strdup("game.gb");
	Entry_free(entry);
	// Should not leak memory
}

///////////////////////////////
// EntryArray_indexOf tests
///////////////////////////////

void test_EntryArray_indexOf_finds_first(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/A.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/B.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/C.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	int idx = EntryArray_indexOf(arr, "/Roms/GB/A.gb");
	TEST_ASSERT_EQUAL(0, idx);

	EntryArray_free(arr);
}

void test_EntryArray_indexOf_finds_middle(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/A.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/B.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/C.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	int idx = EntryArray_indexOf(arr, "/Roms/GB/B.gb");
	TEST_ASSERT_EQUAL(1, idx);

	EntryArray_free(arr);
}

void test_EntryArray_indexOf_finds_last(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/A.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/B.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/C.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	int idx = EntryArray_indexOf(arr, "/Roms/GB/C.gb");
	TEST_ASSERT_EQUAL(2, idx);

	EntryArray_free(arr);
}

void test_EntryArray_indexOf_returns_negative_for_missing(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/A.gb", ENTRY_ROM);
	Array_push(arr, e1);

	int idx = EntryArray_indexOf(arr, "/Roms/GB/NotHere.gb");
	TEST_ASSERT_EQUAL(-1, idx);

	EntryArray_free(arr);
}

void test_EntryArray_indexOf_handles_empty_array(void) {
	Array* arr = Array_new();

	int idx = EntryArray_indexOf(arr, "/Roms/GB/A.gb");
	TEST_ASSERT_EQUAL(-1, idx);

	Array_free(arr);
}

///////////////////////////////
// EntryArray_sort tests
///////////////////////////////

void test_EntryArray_sort_alphabetical(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/Zelda.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Mario.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Aladdin.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	EntryArray_sort(arr);

	TEST_ASSERT_EQUAL_STRING("Aladdin", ((Entry*)arr->items[0])->name);
	TEST_ASSERT_EQUAL_STRING("Mario", ((Entry*)arr->items[1])->name);
	TEST_ASSERT_EQUAL_STRING("Zelda", ((Entry*)arr->items[2])->name);

	EntryArray_free(arr);
}

void test_EntryArray_sort_natural_numbers(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/Game 10.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/Game 2.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Game 1.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	EntryArray_sort(arr);

	// Natural sort: 1 < 2 < 10
	TEST_ASSERT_EQUAL_STRING("Game 1", ((Entry*)arr->items[0])->name);
	TEST_ASSERT_EQUAL_STRING("Game 2", ((Entry*)arr->items[1])->name);
	TEST_ASSERT_EQUAL_STRING("Game 10", ((Entry*)arr->items[2])->name);

	EntryArray_free(arr);
}

void test_EntryArray_sort_ignores_leading_article(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/NES/The Legend of Zelda.nes", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/NES/Mario.nes", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/NES/Kirby.nes", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	EntryArray_sort(arr);

	// "The Legend" sorts under L, not T
	TEST_ASSERT_EQUAL_STRING("Kirby", ((Entry*)arr->items[0])->name);
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", ((Entry*)arr->items[1])->name);
	TEST_ASSERT_EQUAL_STRING("Mario", ((Entry*)arr->items[2])->name);

	EntryArray_free(arr);
}

void test_EntryArray_sort_case_insensitive(void) {
	Array* arr = Array_new();
	Entry* e1 = Entry_new("/Roms/GB/ZELDA.gb", ENTRY_ROM);
	Entry* e2 = Entry_new("/Roms/GB/mario.gb", ENTRY_ROM);
	Entry* e3 = Entry_new("/Roms/GB/Kirby.gb", ENTRY_ROM);
	Array_push(arr, e1);
	Array_push(arr, e2);
	Array_push(arr, e3);

	EntryArray_sort(arr);

	TEST_ASSERT_EQUAL_STRING("Kirby", ((Entry*)arr->items[0])->name);
	TEST_ASSERT_EQUAL_STRING("mario", ((Entry*)arr->items[1])->name);
	TEST_ASSERT_EQUAL_STRING("ZELDA", ((Entry*)arr->items[2])->name);

	EntryArray_free(arr);
}

///////////////////////////////
// IntArray tests
///////////////////////////////

void test_IntArray_new_creates_empty(void) {
	IntArray* arr = IntArray_new();
	TEST_ASSERT_NOT_NULL(arr);
	TEST_ASSERT_EQUAL(0, arr->count);
	IntArray_free(arr);
}

void test_IntArray_push_adds_items(void) {
	IntArray* arr = IntArray_new();
	IntArray_push(arr, 5);
	IntArray_push(arr, 10);
	IntArray_push(arr, 15);

	TEST_ASSERT_EQUAL(3, arr->count);
	TEST_ASSERT_EQUAL(5, arr->items[0]);
	TEST_ASSERT_EQUAL(10, arr->items[1]);
	TEST_ASSERT_EQUAL(15, arr->items[2]);

	IntArray_free(arr);
}

void test_IntArray_push_respects_max(void) {
	IntArray* arr = IntArray_new();

	// Push more than INT_ARRAY_MAX (27) items
	for (int i = 0; i < 30; i++) {
		IntArray_push(arr, i);
	}

	// Should cap at 27
	TEST_ASSERT_EQUAL(INT_ARRAY_MAX, arr->count);

	IntArray_free(arr);
}

void test_IntArray_items_initialized_to_zero(void) {
	IntArray* arr = IntArray_new();

	// All items should be zero initially
	for (int i = 0; i < INT_ARRAY_MAX; i++) {
		TEST_ASSERT_EQUAL(0, arr->items[i]);
	}

	IntArray_free(arr);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Entry_new tests
	RUN_TEST(test_Entry_new_creates_entry);
	RUN_TEST(test_Entry_new_strips_extension);
	RUN_TEST(test_Entry_new_strips_region_codes);
	RUN_TEST(test_Entry_new_handles_directory);
	RUN_TEST(test_Entry_new_handles_pak);
	RUN_TEST(test_Entry_new_strips_leading_article_for_sort);
	RUN_TEST(test_Entry_new_handles_A_article);

	// Entry_setName tests
	RUN_TEST(test_Entry_setName_updates_name);
	RUN_TEST(test_Entry_setName_updates_sort_key_with_article);
	RUN_TEST(test_Entry_setName_preserves_path);

	// Entry_free tests
	RUN_TEST(test_Entry_free_handles_null);
	RUN_TEST(test_Entry_free_handles_entry_with_unique);

	// EntryArray_indexOf tests
	RUN_TEST(test_EntryArray_indexOf_finds_first);
	RUN_TEST(test_EntryArray_indexOf_finds_middle);
	RUN_TEST(test_EntryArray_indexOf_finds_last);
	RUN_TEST(test_EntryArray_indexOf_returns_negative_for_missing);
	RUN_TEST(test_EntryArray_indexOf_handles_empty_array);

	// EntryArray_sort tests
	RUN_TEST(test_EntryArray_sort_alphabetical);
	RUN_TEST(test_EntryArray_sort_natural_numbers);
	RUN_TEST(test_EntryArray_sort_ignores_leading_article);
	RUN_TEST(test_EntryArray_sort_case_insensitive);

	// IntArray tests
	RUN_TEST(test_IntArray_new_creates_empty);
	RUN_TEST(test_IntArray_push_adds_items);
	RUN_TEST(test_IntArray_push_respects_max);
	RUN_TEST(test_IntArray_items_initialized_to_zero);

	return UNITY_END();
}
