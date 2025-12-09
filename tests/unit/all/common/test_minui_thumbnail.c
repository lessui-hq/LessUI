/**
 * test_minui_thumbnail.c - Unit tests for thumbnail cache and fade animation
 *
 * Tests the pure cache management logic and fade animation math.
 *
 * Test coverage:
 * - ThumbCache_init - Initialize empty cache
 * - ThumbCache_find - Search by entry index
 * - ThumbCache_add/evict - FIFO cache operations
 * - ThumbCache_clear - Reset cache state
 * - ThumbPreload_getHintIndex - Scroll direction preload
 * - ThumbFade_* - Fade animation state and calculation
 */

#include "../../support/unity/unity.h"
#include "minui_thumbnail.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// ThumbCache_init tests
///////////////////////////////

void test_ThumbCache_init_sets_size_zero(void) {
	ThumbCache cache;
	cache.size = 99; // Garbage value
	ThumbCache_init(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);
}

void test_ThumbCache_init_clears_slots(void) {
	ThumbCache cache;
	// Put garbage in slots
	cache.slots[0].entry_index = 42;
	cache.slots[0].data = (void*)0xDEADBEEF;
	ThumbCache_init(&cache);
	TEST_ASSERT_EQUAL(0, cache.slots[0].entry_index);
	TEST_ASSERT_NULL(cache.slots[0].data);
}

void test_ThumbCache_init_handles_null(void) {
	// Should not crash
	ThumbCache_init(NULL);
}

///////////////////////////////
// ThumbCache_find tests
///////////////////////////////

void test_ThumbCache_find_returns_negative_when_empty(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	TEST_ASSERT_EQUAL(-1, ThumbCache_find(&cache, 5));
}

void test_ThumbCache_find_returns_slot_index(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/thumb.png", (void*)0x1000);
	ThumbCache_add(&cache, 20, "/path/thumb2.png", (void*)0x2000);

	TEST_ASSERT_EQUAL(0, ThumbCache_find(&cache, 10));
	TEST_ASSERT_EQUAL(1, ThumbCache_find(&cache, 20));
}

void test_ThumbCache_find_returns_negative_for_missing(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL(-1, ThumbCache_find(&cache, 99));
}

void test_ThumbCache_find_handles_null(void) {
	TEST_ASSERT_EQUAL(-1, ThumbCache_find(NULL, 5));
}

///////////////////////////////
// ThumbCache_isFull tests
///////////////////////////////

void test_ThumbCache_isFull_returns_false_when_empty(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	TEST_ASSERT_FALSE(ThumbCache_isFull(&cache));
}

void test_ThumbCache_isFull_returns_false_when_partial(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/thumb.png", (void*)0x1000);
	TEST_ASSERT_FALSE(ThumbCache_isFull(&cache));
}

void test_ThumbCache_isFull_returns_true_when_full(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	for (int i = 0; i < THUMB_CACHE_SIZE; i++) {
		ThumbCache_add(&cache, i, "/path/thumb.png", (void*)(long)(i + 1));
	}
	TEST_ASSERT_TRUE(ThumbCache_isFull(&cache));
}

void test_ThumbCache_isFull_handles_null(void) {
	TEST_ASSERT_FALSE(ThumbCache_isFull(NULL));
}

///////////////////////////////
// ThumbCache_getEvictSlot tests
///////////////////////////////

void test_ThumbCache_getEvictSlot_returns_negative_when_not_full(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/thumb.png", (void*)0x1000);
	TEST_ASSERT_EQUAL(-1, ThumbCache_getEvictSlot(&cache));
}

void test_ThumbCache_getEvictSlot_returns_zero_when_full(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	for (int i = 0; i < THUMB_CACHE_SIZE; i++) {
		ThumbCache_add(&cache, i, "/path/thumb.png", (void*)(long)(i + 1));
	}
	TEST_ASSERT_EQUAL(0, ThumbCache_getEvictSlot(&cache));
}

void test_ThumbCache_getEvictSlot_handles_null(void) {
	TEST_ASSERT_EQUAL(-1, ThumbCache_getEvictSlot(NULL));
}

///////////////////////////////
// ThumbCache_add tests
///////////////////////////////

void test_ThumbCache_add_stores_entry_index(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 42, "/path/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL(42, cache.slots[0].entry_index);
}

void test_ThumbCache_add_stores_path(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 42, "/path/to/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL_STRING("/path/to/thumb.png", cache.slots[0].path);
}

void test_ThumbCache_add_stores_data(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	void* data = (void*)0xCAFE;
	ThumbCache_add(&cache, 42, "/path/thumb.png", data);

	TEST_ASSERT_EQUAL_PTR(data, cache.slots[0].data);
}

void test_ThumbCache_add_increments_size(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);

	ThumbCache_add(&cache, 1, "/path/a.png", (void*)0x1);
	TEST_ASSERT_EQUAL(1, cache.size);

	ThumbCache_add(&cache, 2, "/path/b.png", (void*)0x2);
	TEST_ASSERT_EQUAL(2, cache.size);
}

void test_ThumbCache_add_fails_when_full(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	for (int i = 0; i < THUMB_CACHE_SIZE; i++) {
		TEST_ASSERT_TRUE(ThumbCache_add(&cache, i, "/path/thumb.png", (void*)(long)(i + 1)));
	}
	// Should fail - cache is full
	TEST_ASSERT_FALSE(ThumbCache_add(&cache, 99, "/path/new.png", (void*)0x999));
	TEST_ASSERT_EQUAL(THUMB_CACHE_SIZE, cache.size);
}

void test_ThumbCache_add_handles_null_cache(void) {
	TEST_ASSERT_FALSE(ThumbCache_add(NULL, 1, "/path/a.png", (void*)0x1));
}

void test_ThumbCache_add_handles_null_path(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	TEST_ASSERT_FALSE(ThumbCache_add(&cache, 1, NULL, (void*)0x1));
}

///////////////////////////////
// ThumbCache_evict tests
///////////////////////////////

void test_ThumbCache_evict_removes_first_slot(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/a.png", (void*)0x10);
	ThumbCache_add(&cache, 20, "/path/b.png", (void*)0x20);
	ThumbCache_add(&cache, 30, "/path/c.png", (void*)0x30);

	ThumbCache_evict(&cache);

	TEST_ASSERT_EQUAL(2, cache.size);
	TEST_ASSERT_EQUAL(20, cache.slots[0].entry_index);
	TEST_ASSERT_EQUAL(30, cache.slots[1].entry_index);
}

void test_ThumbCache_evict_shifts_items_left(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/a.png", (void*)0x10);
	ThumbCache_add(&cache, 20, "/path/b.png", (void*)0x20);

	ThumbCache_evict(&cache);

	// Entry 20 should now be at slot 0
	TEST_ASSERT_EQUAL(20, cache.slots[0].entry_index);
	TEST_ASSERT_EQUAL_PTR((void*)0x20, cache.slots[0].data);
}

void test_ThumbCache_evict_clears_last_slot(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/a.png", (void*)0x10);
	ThumbCache_add(&cache, 20, "/path/b.png", (void*)0x20);

	ThumbCache_evict(&cache);

	// Slot 1 should be cleared
	TEST_ASSERT_EQUAL(0, cache.slots[1].entry_index);
	TEST_ASSERT_NULL(cache.slots[1].data);
}

void test_ThumbCache_evict_returns_false_when_empty(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	TEST_ASSERT_FALSE(ThumbCache_evict(&cache));
}

void test_ThumbCache_evict_handles_null(void) {
	TEST_ASSERT_FALSE(ThumbCache_evict(NULL));
}

///////////////////////////////
// ThumbCache_getData tests
///////////////////////////////

void test_ThumbCache_getData_returns_data(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	void* data = (void*)0xBEEF;
	ThumbCache_add(&cache, 42, "/path/thumb.png", data);

	TEST_ASSERT_EQUAL_PTR(data, ThumbCache_getData(&cache, 0));
}

void test_ThumbCache_getData_returns_null_for_invalid_slot(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	TEST_ASSERT_NULL(ThumbCache_getData(&cache, -1));
	TEST_ASSERT_NULL(ThumbCache_getData(&cache, 1));
	TEST_ASSERT_NULL(ThumbCache_getData(&cache, 99));
}

void test_ThumbCache_getData_handles_null(void) {
	TEST_ASSERT_NULL(ThumbCache_getData(NULL, 0));
}

///////////////////////////////
// ThumbCache_getSlot tests
///////////////////////////////

void test_ThumbCache_getSlot_returns_slot(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	ThumbCacheSlot* slot = ThumbCache_getSlot(&cache, 0);
	TEST_ASSERT_NOT_NULL(slot);
	TEST_ASSERT_EQUAL(42, slot->entry_index);
}

void test_ThumbCache_getSlot_returns_null_for_invalid(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	TEST_ASSERT_NULL(ThumbCache_getSlot(&cache, -1));
	TEST_ASSERT_NULL(ThumbCache_getSlot(&cache, 1));
}

void test_ThumbCache_getSlot_handles_null(void) {
	TEST_ASSERT_NULL(ThumbCache_getSlot(NULL, 0));
}

///////////////////////////////
// ThumbCache_clear tests
///////////////////////////////

void test_ThumbCache_clear_sets_size_zero(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/a.png", (void*)0x10);
	ThumbCache_add(&cache, 20, "/path/b.png", (void*)0x20);

	ThumbCache_clear(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);
}

void test_ThumbCache_clear_clears_slots(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);
	ThumbCache_add(&cache, 10, "/path/a.png", (void*)0x10);

	ThumbCache_clear(&cache);
	TEST_ASSERT_NULL(cache.slots[0].data);
	TEST_ASSERT_EQUAL(0, cache.slots[0].entry_index);
}

void test_ThumbCache_clear_handles_null(void) {
	// Should not crash
	ThumbCache_clear(NULL);
}

///////////////////////////////
// ThumbPreload_getHintIndex tests
///////////////////////////////

void test_ThumbPreload_getHintIndex_scrolling_down(void) {
	// Current > last means scrolling down, preload next
	TEST_ASSERT_EQUAL(11, ThumbPreload_getHintIndex(10, 9, 100));
}

void test_ThumbPreload_getHintIndex_scrolling_up(void) {
	// Current < last means scrolling up, preload previous
	TEST_ASSERT_EQUAL(9, ThumbPreload_getHintIndex(10, 11, 100));
}

void test_ThumbPreload_getHintIndex_at_end(void) {
	// At last item, scrolling down would exceed bounds
	TEST_ASSERT_EQUAL(-1, ThumbPreload_getHintIndex(99, 98, 100));
}

void test_ThumbPreload_getHintIndex_at_start(void) {
	// At first item, scrolling up would go negative
	TEST_ASSERT_EQUAL(-1, ThumbPreload_getHintIndex(0, 1, 100));
}

void test_ThumbPreload_getHintIndex_same_position(void) {
	// Same position (no scroll) - no preload hint needed
	TEST_ASSERT_EQUAL(-1, ThumbPreload_getHintIndex(10, 10, 100));
}

void test_ThumbPreload_getHintIndex_empty_directory(void) {
	TEST_ASSERT_EQUAL(-1, ThumbPreload_getHintIndex(0, 0, 0));
}

void test_ThumbPreload_getHintIndex_single_item(void) {
	// Single item, preload would be out of bounds
	TEST_ASSERT_EQUAL(-1, ThumbPreload_getHintIndex(0, 0, 1));
}

///////////////////////////////
// ThumbFade_init tests
///////////////////////////////

void test_ThumbFade_init_sets_default_duration(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 0);
	TEST_ASSERT_EQUAL(THUMB_FADE_DURATION_MS, state.duration_ms);
}

void test_ThumbFade_init_sets_custom_duration(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 500);
	TEST_ASSERT_EQUAL(500, state.duration_ms);
}

void test_ThumbFade_init_sets_max_alpha(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MAX, state.alpha);
}

void test_ThumbFade_init_not_fading(void) {
	ThumbFadeState state;
	state.start_ms = 12345; // Garbage
	ThumbFade_init(&state, 200);
	TEST_ASSERT_EQUAL(0, state.start_ms);
}

void test_ThumbFade_init_handles_null(void) {
	// Should not crash
	ThumbFade_init(NULL, 200);
}

///////////////////////////////
// ThumbFade_start tests
///////////////////////////////

void test_ThumbFade_start_sets_start_time(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	TEST_ASSERT_EQUAL(1000, state.start_ms);
}

void test_ThumbFade_start_sets_min_alpha(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MIN, state.alpha);
}

void test_ThumbFade_start_handles_null(void) {
	// Should not crash
	ThumbFade_start(NULL, 1000);
}

///////////////////////////////
// ThumbFade_reset tests
///////////////////////////////

void test_ThumbFade_reset_clears_start_time(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	ThumbFade_reset(&state);
	TEST_ASSERT_EQUAL(0, state.start_ms);
}

void test_ThumbFade_reset_sets_max_alpha(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	ThumbFade_reset(&state);
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MAX, state.alpha);
}

void test_ThumbFade_reset_handles_null(void) {
	// Should not crash
	ThumbFade_reset(NULL);
}

///////////////////////////////
// ThumbFade_update tests
///////////////////////////////

void test_ThumbFade_update_returns_false_when_not_fading(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	TEST_ASSERT_FALSE(ThumbFade_update(&state, 1000));
}

void test_ThumbFade_update_returns_true_while_fading(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	TEST_ASSERT_TRUE(ThumbFade_update(&state, 1100));
}

void test_ThumbFade_update_increases_alpha(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);

	ThumbFade_update(&state, 1100); // 50% through
	TEST_ASSERT_GREATER_THAN(THUMB_ALPHA_MIN, state.alpha);
	TEST_ASSERT_LESS_THAN(THUMB_ALPHA_MAX, state.alpha);
}

void test_ThumbFade_update_completes_at_duration(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);

	ThumbFade_update(&state, 1200); // Exactly at duration
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MAX, state.alpha);
	TEST_ASSERT_EQUAL(0, state.start_ms); // No longer fading
}

void test_ThumbFade_update_handles_overshoot(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);

	ThumbFade_update(&state, 2000); // Way past duration
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MAX, state.alpha);
}

void test_ThumbFade_update_handles_null(void) {
	TEST_ASSERT_FALSE(ThumbFade_update(NULL, 1000));
}

///////////////////////////////
// ThumbFade_isActive tests
///////////////////////////////

void test_ThumbFade_isActive_false_when_not_started(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	TEST_ASSERT_FALSE(ThumbFade_isActive(&state));
}

void test_ThumbFade_isActive_true_when_fading(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	TEST_ASSERT_TRUE(ThumbFade_isActive(&state));
}

void test_ThumbFade_isActive_false_after_complete(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 200);
	ThumbFade_start(&state, 1000);
	ThumbFade_update(&state, 1200); // Complete
	TEST_ASSERT_FALSE(ThumbFade_isActive(&state));
}

void test_ThumbFade_isActive_handles_null(void) {
	TEST_ASSERT_FALSE(ThumbFade_isActive(NULL));
}

///////////////////////////////
// ThumbFade_calculateAlpha tests
///////////////////////////////

void test_ThumbFade_calculateAlpha_zero_at_start(void) {
	TEST_ASSERT_EQUAL(0, ThumbFade_calculateAlpha(0, 200, 255));
}

void test_ThumbFade_calculateAlpha_max_at_end(void) {
	TEST_ASSERT_EQUAL(255, ThumbFade_calculateAlpha(200, 200, 255));
}

void test_ThumbFade_calculateAlpha_max_past_end(void) {
	TEST_ASSERT_EQUAL(255, ThumbFade_calculateAlpha(300, 200, 255));
}

void test_ThumbFade_calculateAlpha_smoothstep_midpoint(void) {
	// At t=0.5, smoothstep gives 0.5 * 0.5 * (3 - 2 * 0.5) = 0.25 * 2 = 0.5
	int alpha = ThumbFade_calculateAlpha(100, 200, 255);
	// Should be close to 127 (half of 255)
	TEST_ASSERT_INT_WITHIN(5, 127, alpha);
}

void test_ThumbFade_calculateAlpha_smoothstep_easing(void) {
	// Verify smoothstep easing properties (starts slow, accelerates, ends slow)
	int alpha_10 = ThumbFade_calculateAlpha(20, 200, 255);   // 10%
	int alpha_25 = ThumbFade_calculateAlpha(50, 200, 255);   // 25%
	int alpha_50 = ThumbFade_calculateAlpha(100, 200, 255);  // 50%
	int alpha_75 = ThumbFade_calculateAlpha(150, 200, 255);  // 75%
	int alpha_90 = ThumbFade_calculateAlpha(180, 200, 255);  // 90%

	// Test monotonicity: alpha should increase with time
	TEST_ASSERT_LESS_THAN(alpha_25, alpha_10);
	TEST_ASSERT_LESS_THAN(alpha_50, alpha_25);
	TEST_ASSERT_LESS_THAN(alpha_75, alpha_50);
	TEST_ASSERT_LESS_THAN(alpha_90, alpha_75);

	// Test smoothstep property: should be close to 50% at midpoint
	// (exact value depends on formula, but should be within reasonable range)
	TEST_ASSERT_GREATER_THAN(100, alpha_50);
	TEST_ASSERT_LESS_THAN(155, alpha_50);

	// Test easing: early and late changes should be smaller than middle changes
	int delta_early = alpha_25 - alpha_10;   // 10% -> 25%
	int delta_mid = alpha_50 - alpha_25;     // 25% -> 50%
	int delta_late = alpha_90 - alpha_75;    // 75% -> 90%

	// Middle should have larger changes (acceleration phase)
	TEST_ASSERT_GREATER_THAN(delta_early, delta_mid);
	TEST_ASSERT_GREATER_THAN(delta_late, delta_mid);
}

void test_ThumbFade_calculateAlpha_zero_duration(void) {
	// Division by zero protection
	TEST_ASSERT_EQUAL(255, ThumbFade_calculateAlpha(100, 0, 255));
}

void test_ThumbFade_calculateAlpha_custom_max(void) {
	TEST_ASSERT_EQUAL(100, ThumbFade_calculateAlpha(200, 200, 100));
}

///////////////////////////////
// Integration scenarios
///////////////////////////////

void test_cache_fifo_eviction_order(void) {
	ThumbCache cache;
	ThumbCache_init(&cache);

	// Fill cache
	ThumbCache_add(&cache, 1, "/a.png", (void*)0x1);
	ThumbCache_add(&cache, 2, "/b.png", (void*)0x2);
	ThumbCache_add(&cache, 3, "/c.png", (void*)0x3);

	TEST_ASSERT_TRUE(ThumbCache_isFull(&cache));

	// Evict and add new item
	ThumbCache_evict(&cache);
	ThumbCache_add(&cache, 4, "/d.png", (void*)0x4);

	// Should have 2, 3, 4 now
	TEST_ASSERT_EQUAL(-1, ThumbCache_find(&cache, 1)); // Evicted
	TEST_ASSERT_EQUAL(0, ThumbCache_find(&cache, 2));
	TEST_ASSERT_EQUAL(1, ThumbCache_find(&cache, 3));
	TEST_ASSERT_EQUAL(2, ThumbCache_find(&cache, 4));
}

void test_fade_full_cycle(void) {
	ThumbFadeState state;
	ThumbFade_init(&state, 100);

	// Start fade
	ThumbFade_start(&state, 1000);
	TEST_ASSERT_TRUE(ThumbFade_isActive(&state));
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MIN, state.alpha);

	// Update partway
	ThumbFade_update(&state, 1050);
	TEST_ASSERT_TRUE(ThumbFade_isActive(&state));
	TEST_ASSERT_GREATER_THAN(0, state.alpha);
	TEST_ASSERT_LESS_THAN(255, state.alpha);

	// Complete
	ThumbFade_update(&state, 1100);
	TEST_ASSERT_FALSE(ThumbFade_isActive(&state));
	TEST_ASSERT_EQUAL(THUMB_ALPHA_MAX, state.alpha);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// ThumbCache_init tests
	RUN_TEST(test_ThumbCache_init_sets_size_zero);
	RUN_TEST(test_ThumbCache_init_clears_slots);
	RUN_TEST(test_ThumbCache_init_handles_null);

	// ThumbCache_find tests
	RUN_TEST(test_ThumbCache_find_returns_negative_when_empty);
	RUN_TEST(test_ThumbCache_find_returns_slot_index);
	RUN_TEST(test_ThumbCache_find_returns_negative_for_missing);
	RUN_TEST(test_ThumbCache_find_handles_null);

	// ThumbCache_isFull tests
	RUN_TEST(test_ThumbCache_isFull_returns_false_when_empty);
	RUN_TEST(test_ThumbCache_isFull_returns_false_when_partial);
	RUN_TEST(test_ThumbCache_isFull_returns_true_when_full);
	RUN_TEST(test_ThumbCache_isFull_handles_null);

	// ThumbCache_getEvictSlot tests
	RUN_TEST(test_ThumbCache_getEvictSlot_returns_negative_when_not_full);
	RUN_TEST(test_ThumbCache_getEvictSlot_returns_zero_when_full);
	RUN_TEST(test_ThumbCache_getEvictSlot_handles_null);

	// ThumbCache_add tests
	RUN_TEST(test_ThumbCache_add_stores_entry_index);
	RUN_TEST(test_ThumbCache_add_stores_path);
	RUN_TEST(test_ThumbCache_add_stores_data);
	RUN_TEST(test_ThumbCache_add_increments_size);
	RUN_TEST(test_ThumbCache_add_fails_when_full);
	RUN_TEST(test_ThumbCache_add_handles_null_cache);
	RUN_TEST(test_ThumbCache_add_handles_null_path);

	// ThumbCache_evict tests
	RUN_TEST(test_ThumbCache_evict_removes_first_slot);
	RUN_TEST(test_ThumbCache_evict_shifts_items_left);
	RUN_TEST(test_ThumbCache_evict_clears_last_slot);
	RUN_TEST(test_ThumbCache_evict_returns_false_when_empty);
	RUN_TEST(test_ThumbCache_evict_handles_null);

	// ThumbCache_getData tests
	RUN_TEST(test_ThumbCache_getData_returns_data);
	RUN_TEST(test_ThumbCache_getData_returns_null_for_invalid_slot);
	RUN_TEST(test_ThumbCache_getData_handles_null);

	// ThumbCache_getSlot tests
	RUN_TEST(test_ThumbCache_getSlot_returns_slot);
	RUN_TEST(test_ThumbCache_getSlot_returns_null_for_invalid);
	RUN_TEST(test_ThumbCache_getSlot_handles_null);

	// ThumbCache_clear tests
	RUN_TEST(test_ThumbCache_clear_sets_size_zero);
	RUN_TEST(test_ThumbCache_clear_clears_slots);
	RUN_TEST(test_ThumbCache_clear_handles_null);

	// ThumbPreload_getHintIndex tests
	RUN_TEST(test_ThumbPreload_getHintIndex_scrolling_down);
	RUN_TEST(test_ThumbPreload_getHintIndex_scrolling_up);
	RUN_TEST(test_ThumbPreload_getHintIndex_at_end);
	RUN_TEST(test_ThumbPreload_getHintIndex_at_start);
	RUN_TEST(test_ThumbPreload_getHintIndex_same_position);
	RUN_TEST(test_ThumbPreload_getHintIndex_empty_directory);
	RUN_TEST(test_ThumbPreload_getHintIndex_single_item);

	// ThumbFade_init tests
	RUN_TEST(test_ThumbFade_init_sets_default_duration);
	RUN_TEST(test_ThumbFade_init_sets_custom_duration);
	RUN_TEST(test_ThumbFade_init_sets_max_alpha);
	RUN_TEST(test_ThumbFade_init_not_fading);
	RUN_TEST(test_ThumbFade_init_handles_null);

	// ThumbFade_start tests
	RUN_TEST(test_ThumbFade_start_sets_start_time);
	RUN_TEST(test_ThumbFade_start_sets_min_alpha);
	RUN_TEST(test_ThumbFade_start_handles_null);

	// ThumbFade_reset tests
	RUN_TEST(test_ThumbFade_reset_clears_start_time);
	RUN_TEST(test_ThumbFade_reset_sets_max_alpha);
	RUN_TEST(test_ThumbFade_reset_handles_null);

	// ThumbFade_update tests
	RUN_TEST(test_ThumbFade_update_returns_false_when_not_fading);
	RUN_TEST(test_ThumbFade_update_returns_true_while_fading);
	RUN_TEST(test_ThumbFade_update_increases_alpha);
	RUN_TEST(test_ThumbFade_update_completes_at_duration);
	RUN_TEST(test_ThumbFade_update_handles_overshoot);
	RUN_TEST(test_ThumbFade_update_handles_null);

	// ThumbFade_isActive tests
	RUN_TEST(test_ThumbFade_isActive_false_when_not_started);
	RUN_TEST(test_ThumbFade_isActive_true_when_fading);
	RUN_TEST(test_ThumbFade_isActive_false_after_complete);
	RUN_TEST(test_ThumbFade_isActive_handles_null);

	// ThumbFade_calculateAlpha tests
	RUN_TEST(test_ThumbFade_calculateAlpha_zero_at_start);
	RUN_TEST(test_ThumbFade_calculateAlpha_max_at_end);
	RUN_TEST(test_ThumbFade_calculateAlpha_max_past_end);
	RUN_TEST(test_ThumbFade_calculateAlpha_smoothstep_midpoint);
	RUN_TEST(test_ThumbFade_calculateAlpha_smoothstep_easing);
	RUN_TEST(test_ThumbFade_calculateAlpha_zero_duration);
	RUN_TEST(test_ThumbFade_calculateAlpha_custom_max);

	// Integration scenarios
	RUN_TEST(test_cache_fifo_eviction_order);
	RUN_TEST(test_fade_full_cycle);

	return UNITY_END();
}
