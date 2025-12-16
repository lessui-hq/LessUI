/**
 * test_collections.c - Unit tests for Array data structure
 *
 * Tests the generic data structures extracted from launcher.c.
 * These are pure logic with no dependencies, making them ideal for unit testing.
 *
 * Test coverage:
 * - Array_new/free - Array lifecycle
 * - Array_push/pop - Stack operations
 * - Array_unshift - Insert at beginning
 * - Array_reverse - Reverse order
 * - StringArray_indexOf - String search
 * - StringArray_free - String cleanup
 *
 * For hash map tests, see stringmap.h and its tests.
 */

#define _POSIX_C_SOURCE 200809L  // Required for strdup()

#include "unity.h"
#include "../../../../workspace/all/common/collections.h"
#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// Array basic lifecycle tests
///////////////////////////////

void test_Array_new_creates_empty_array(void) {
	Array* arr = Array_new();

	TEST_ASSERT_NOT_NULL(arr);
	TEST_ASSERT_EQUAL_INT(0, arr->count);
	TEST_ASSERT_GREATER_OR_EQUAL(arr->count, arr->capacity);
	TEST_ASSERT_NOT_NULL(arr->items);

	Array_free(arr);
}

void test_Array_free_cleans_up(void) {
	Array* arr = Array_new();
	Array_free(arr);
	// If this doesn't crash, it passed
	TEST_PASS();
}

///////////////////////////////
// Array_push tests
///////////////////////////////

void test_Array_push_single_item(void) {
	Array* arr = Array_new();
	int value = 42;

	Array_push(arr, &value);

	TEST_ASSERT_EQUAL_INT(1, arr->count);
	TEST_ASSERT_EQUAL_PTR(&value, arr->items[0]);

	Array_free(arr);
}

void test_Array_push_multiple_items(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3, 4, 5};

	for (int i = 0; i < 5; i++) {
		Array_push(arr, &values[i]);
	}

	TEST_ASSERT_EQUAL_INT(5, arr->count);
	for (int i = 0; i < 5; i++) {
		TEST_ASSERT_EQUAL_PTR(&values[i], arr->items[i]);
	}

	Array_free(arr);
}

void test_Array_push_grows_capacity(void) {
	Array* arr = Array_new();
	int values[20];

	// Push beyond initial capacity (8)
	for (int i = 0; i < 20; i++) {
		values[i] = i;
		Array_push(arr, &values[i]);
	}

	TEST_ASSERT_EQUAL_INT(20, arr->count);
	TEST_ASSERT_GREATER_OR_EQUAL(20, arr->capacity);

	// Verify all items are still accessible
	for (int i = 0; i < 20; i++) {
		TEST_ASSERT_EQUAL_PTR(&values[i], arr->items[i]);
	}

	Array_free(arr);
}

///////////////////////////////
// Array_pop tests
///////////////////////////////

void test_Array_pop_returns_last_item(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3};

	for (int i = 0; i < 3; i++) {
		Array_push(arr, &values[i]);
	}

	void* popped = Array_pop(arr);

	TEST_ASSERT_EQUAL_PTR(&values[2], popped);
	TEST_ASSERT_EQUAL_INT(2, arr->count);

	Array_free(arr);
}

void test_Array_pop_empty_array_returns_null(void) {
	Array* arr = Array_new();

	void* popped = Array_pop(arr);

	TEST_ASSERT_NULL(popped);
	TEST_ASSERT_EQUAL_INT(0, arr->count);

	Array_free(arr);
}

void test_Array_pop_all_items(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3};

	for (int i = 0; i < 3; i++) {
		Array_push(arr, &values[i]);
	}

	// Pop all items in reverse order
	TEST_ASSERT_EQUAL_PTR(&values[2], Array_pop(arr));
	TEST_ASSERT_EQUAL_PTR(&values[1], Array_pop(arr));
	TEST_ASSERT_EQUAL_PTR(&values[0], Array_pop(arr));
	TEST_ASSERT_NULL(Array_pop(arr));

	Array_free(arr);
}

///////////////////////////////
// Array_unshift tests
///////////////////////////////

void test_Array_unshift_empty_array(void) {
	Array* arr = Array_new();
	int value = 42;

	Array_unshift(arr, &value);

	TEST_ASSERT_EQUAL_INT(1, arr->count);
	TEST_ASSERT_EQUAL_PTR(&value, arr->items[0]);

	Array_free(arr);
}

void test_Array_unshift_shifts_existing_items(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3};

	// Push initial items
	Array_push(arr, &values[1]);
	Array_push(arr, &values[2]);

	// Unshift new first item
	Array_unshift(arr, &values[0]);

	TEST_ASSERT_EQUAL_INT(3, arr->count);
	TEST_ASSERT_EQUAL_PTR(&values[0], arr->items[0]);
	TEST_ASSERT_EQUAL_PTR(&values[1], arr->items[1]);
	TEST_ASSERT_EQUAL_PTR(&values[2], arr->items[2]);

	Array_free(arr);
}

void test_Array_unshift_multiple_times(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3, 4};

	// Unshift in reverse order to get correct final order
	for (int i = 3; i >= 0; i--) {
		Array_unshift(arr, &values[i]);
	}

	TEST_ASSERT_EQUAL_INT(4, arr->count);
	for (int i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL_PTR(&values[i], arr->items[i]);
	}

	Array_free(arr);
}

///////////////////////////////
// Array_reverse tests
///////////////////////////////

void test_Array_reverse_empty_array(void) {
	Array* arr = Array_new();

	Array_reverse(arr);

	TEST_ASSERT_EQUAL_INT(0, arr->count);

	Array_free(arr);
}

void test_Array_reverse_single_item(void) {
	Array* arr = Array_new();
	int value = 42;

	Array_push(arr, &value);
	Array_reverse(arr);

	TEST_ASSERT_EQUAL_INT(1, arr->count);
	TEST_ASSERT_EQUAL_PTR(&value, arr->items[0]);

	Array_free(arr);
}

void test_Array_reverse_even_count(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3, 4};

	for (int i = 0; i < 4; i++) {
		Array_push(arr, &values[i]);
	}

	Array_reverse(arr);

	TEST_ASSERT_EQUAL_INT(4, arr->count);
	TEST_ASSERT_EQUAL_PTR(&values[3], arr->items[0]);
	TEST_ASSERT_EQUAL_PTR(&values[2], arr->items[1]);
	TEST_ASSERT_EQUAL_PTR(&values[1], arr->items[2]);
	TEST_ASSERT_EQUAL_PTR(&values[0], arr->items[3]);

	Array_free(arr);
}

void test_Array_reverse_odd_count(void) {
	Array* arr = Array_new();
	int values[] = {1, 2, 3};

	for (int i = 0; i < 3; i++) {
		Array_push(arr, &values[i]);
	}

	Array_reverse(arr);

	TEST_ASSERT_EQUAL_INT(3, arr->count);
	TEST_ASSERT_EQUAL_PTR(&values[2], arr->items[0]);
	TEST_ASSERT_EQUAL_PTR(&values[1], arr->items[1]);
	TEST_ASSERT_EQUAL_PTR(&values[0], arr->items[2]);

	Array_free(arr);
}

///////////////////////////////
// StringArray tests
///////////////////////////////

void test_StringArray_indexOf_finds_string(void) {
	Array* arr = Array_new();
	char* strings[] = {"apple", "banana", "cherry"};

	for (int i = 0; i < 3; i++) {
		Array_push(arr, strings[i]);
	}

	TEST_ASSERT_EQUAL_INT(0, StringArray_indexOf(arr, "apple"));
	TEST_ASSERT_EQUAL_INT(1, StringArray_indexOf(arr, "banana"));
	TEST_ASSERT_EQUAL_INT(2, StringArray_indexOf(arr, "cherry"));

	Array_free(arr);
}

void test_StringArray_indexOf_not_found(void) {
	Array* arr = Array_new();
	char* strings[] = {"apple", "banana"};

	for (int i = 0; i < 2; i++) {
		Array_push(arr, strings[i]);
	}

	TEST_ASSERT_EQUAL_INT(-1, StringArray_indexOf(arr, "orange"));

	Array_free(arr);
}

void test_StringArray_indexOf_empty_array(void) {
	Array* arr = Array_new();

	TEST_ASSERT_EQUAL_INT(-1, StringArray_indexOf(arr, "test"));

	Array_free(arr);
}

void test_StringArray_free_frees_strings(void) {
	Array* arr = Array_new();

	// Add dynamically allocated strings
	Array_push(arr, strdup("hello"));
	Array_push(arr, strdup("world"));

	// StringArray_free should free both the array and the strings
	StringArray_free(arr);

	// If this doesn't leak memory, it passed
	TEST_PASS();
}

///////////////////////////////
// Integration tests
///////////////////////////////

void test_Array_integration_recent_games_list(void) {
	Array* recents = Array_new();

	// Simulate adding recently played games (newest first)
	Array_unshift(recents, strdup("/Roms/GB/mario.gb"));
	Array_unshift(recents, strdup("/Roms/NES/zelda.nes"));
	Array_unshift(recents, strdup("/Roms/SNES/metroid.smc"));

	// Most recent should be first
	TEST_ASSERT_EQUAL_INT(3, recents->count);
	TEST_ASSERT_EQUAL_STRING("/Roms/SNES/metroid.smc", (char*)recents->items[0]);
	TEST_ASSERT_EQUAL_STRING("/Roms/NES/zelda.nes", (char*)recents->items[1]);
	TEST_ASSERT_EQUAL_STRING("/Roms/GB/mario.gb", (char*)recents->items[2]);

	StringArray_free(recents);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Array basic lifecycle
	RUN_TEST(test_Array_new_creates_empty_array);
	RUN_TEST(test_Array_free_cleans_up);

	// Array_push
	RUN_TEST(test_Array_push_single_item);
	RUN_TEST(test_Array_push_multiple_items);
	RUN_TEST(test_Array_push_grows_capacity);

	// Array_pop
	RUN_TEST(test_Array_pop_returns_last_item);
	RUN_TEST(test_Array_pop_empty_array_returns_null);
	RUN_TEST(test_Array_pop_all_items);

	// Array_unshift
	RUN_TEST(test_Array_unshift_empty_array);
	RUN_TEST(test_Array_unshift_shifts_existing_items);
	RUN_TEST(test_Array_unshift_multiple_times);

	// Array_reverse
	RUN_TEST(test_Array_reverse_empty_array);
	RUN_TEST(test_Array_reverse_single_item);
	RUN_TEST(test_Array_reverse_even_count);
	RUN_TEST(test_Array_reverse_odd_count);

	// StringArray
	RUN_TEST(test_StringArray_indexOf_finds_string);
	RUN_TEST(test_StringArray_indexOf_not_found);
	RUN_TEST(test_StringArray_indexOf_empty_array);
	RUN_TEST(test_StringArray_free_frees_strings);

	// Integration tests
	RUN_TEST(test_Array_integration_recent_games_list);

	return UNITY_END();
}
