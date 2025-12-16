/**
 * test_launcher_thumbnail.c - Unit tests for thumbnail cache and fade animation
 *
 * Tests the pure cache management logic and fade animation math.
 *
 * Test coverage:
 * - LauncherThumbnail_cacheInit - Initialize empty cache
 * - LauncherThumbnail_cacheFind - Search by entry index
 * - LauncherThumbnail_cacheAdd/evict - FIFO cache operations
 * - LauncherThumbnail_cacheClear - Reset cache state
 * - LauncherThumbnail_preloadGetHintIndex - Scroll direction preload
 * - LauncherThumbnail_fade* - Fade animation state and calculation
 */

#include "../../support/unity/unity.h"
#include "launcher_thumbnail.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// LauncherThumbnail_cacheInit tests
///////////////////////////////

void test_LauncherThumbnail_cacheInit_sets_size_zero(void) {
	LauncherThumbnailCache cache;
	cache.size = 99; // Garbage value
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);
}

void test_LauncherThumbnail_cacheInit_clears_slots(void) {
	LauncherThumbnailCache cache;
	// Put garbage in slots
	cache.slots[0].entry_index = 42;
	cache.slots[0].data = (void*)0xDEADBEEF;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(0, cache.slots[0].entry_index);
	TEST_ASSERT_NULL(cache.slots[0].data);
}

void test_LauncherThumbnail_cacheInit_handles_null(void) {
	// Should not crash
	LauncherThumbnail_cacheInit(NULL);
}

///////////////////////////////
// LauncherThumbnail_cacheFind tests
///////////////////////////////

void test_LauncherThumbnail_cacheFind_returns_negative_when_empty(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheFind(&cache, 5));
}

void test_LauncherThumbnail_cacheFind_returns_slot_index(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/thumb.png", (void*)0x1000);
	LauncherThumbnail_cacheAdd(&cache, 20, "/path/thumb2.png", (void*)0x2000);

	TEST_ASSERT_EQUAL(0, LauncherThumbnail_cacheFind(&cache, 10));
	TEST_ASSERT_EQUAL(1, LauncherThumbnail_cacheFind(&cache, 20));
}

void test_LauncherThumbnail_cacheFind_returns_negative_for_missing(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheFind(&cache, 99));
}

void test_LauncherThumbnail_cacheFind_handles_null(void) {
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheFind(NULL, 5));
}

///////////////////////////////
// LauncherThumbnail_cacheIsFull tests
///////////////////////////////

void test_LauncherThumbnail_cacheIsFull_returns_false_when_empty(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsFull(&cache));
}

void test_LauncherThumbnail_cacheIsFull_returns_false_when_partial(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/thumb.png", (void*)0x1000);
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsFull(&cache));
}

void test_LauncherThumbnail_cacheIsFull_returns_true_when_full(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	for (int i = 0; i < LAUNCHER_THUMBNAIL_CACHE_SIZE; i++) {
		LauncherThumbnail_cacheAdd(&cache, i, "/path/thumb.png", (void*)(long)(i + 1));
	}
	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsFull(&cache));
}

void test_LauncherThumbnail_cacheIsFull_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsFull(NULL));
}

///////////////////////////////
// LauncherThumbnail_cacheGetEvictSlot tests
///////////////////////////////

void test_LauncherThumbnail_cacheGetEvictSlot_returns_negative_when_not_full(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/thumb.png", (void*)0x1000);
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheGetEvictSlot(&cache));
}

void test_LauncherThumbnail_cacheGetEvictSlot_returns_zero_when_full(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	for (int i = 0; i < LAUNCHER_THUMBNAIL_CACHE_SIZE; i++) {
		LauncherThumbnail_cacheAdd(&cache, i, "/path/thumb.png", (void*)(long)(i + 1));
	}
	TEST_ASSERT_EQUAL(0, LauncherThumbnail_cacheGetEvictSlot(&cache));
}

void test_LauncherThumbnail_cacheGetEvictSlot_handles_null(void) {
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheGetEvictSlot(NULL));
}

///////////////////////////////
// LauncherThumbnail_cacheAdd tests
///////////////////////////////

void test_LauncherThumbnail_cacheAdd_stores_entry_index(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL(42, cache.slots[0].entry_index);
}

void test_LauncherThumbnail_cacheAdd_stores_path(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/to/thumb.png", (void*)0x1000);

	TEST_ASSERT_EQUAL_STRING("/path/to/thumb.png", cache.slots[0].path);
}

void test_LauncherThumbnail_cacheAdd_stores_data(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	void* data = (void*)0xCAFE;
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", data);

	TEST_ASSERT_EQUAL_PTR(data, cache.slots[0].data);
}

void test_LauncherThumbnail_cacheAdd_increments_size(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);

	LauncherThumbnail_cacheAdd(&cache, 1, "/path/a.png", (void*)0x1);
	TEST_ASSERT_EQUAL(1, cache.size);

	LauncherThumbnail_cacheAdd(&cache, 2, "/path/b.png", (void*)0x2);
	TEST_ASSERT_EQUAL(2, cache.size);
}

void test_LauncherThumbnail_cacheAdd_fails_when_full(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	for (int i = 0; i < LAUNCHER_THUMBNAIL_CACHE_SIZE; i++) {
		TEST_ASSERT_TRUE(LauncherThumbnail_cacheAdd(&cache, i, "/path/thumb.png", (void*)(long)(i + 1)));
	}
	// Should fail - cache is full
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheAdd(&cache, 99, "/path/new.png", (void*)0x999));
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_CACHE_SIZE, cache.size);
}

void test_LauncherThumbnail_cacheAdd_handles_null_cache(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheAdd(NULL, 1, "/path/a.png", (void*)0x1));
}

void test_LauncherThumbnail_cacheAdd_handles_null_path(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheAdd(&cache, 1, NULL, (void*)0x1));
}

///////////////////////////////
// LauncherThumbnail_cacheEvict tests
///////////////////////////////

void test_LauncherThumbnail_cacheEvict_removes_first_slot(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/path/b.png", (void*)0x20);
	LauncherThumbnail_cacheAdd(&cache, 30, "/path/c.png", (void*)0x30);

	LauncherThumbnail_cacheEvict(&cache);

	TEST_ASSERT_EQUAL(2, cache.size);
	TEST_ASSERT_EQUAL(20, cache.slots[0].entry_index);
	TEST_ASSERT_EQUAL(30, cache.slots[1].entry_index);
}

void test_LauncherThumbnail_cacheEvict_shifts_items_left(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/path/b.png", (void*)0x20);

	LauncherThumbnail_cacheEvict(&cache);

	// Entry 20 should now be at slot 0
	TEST_ASSERT_EQUAL(20, cache.slots[0].entry_index);
	TEST_ASSERT_EQUAL_PTR((void*)0x20, cache.slots[0].data);
}

void test_LauncherThumbnail_cacheEvict_clears_last_slot(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/path/b.png", (void*)0x20);

	LauncherThumbnail_cacheEvict(&cache);

	// Slot 1 should be cleared
	TEST_ASSERT_EQUAL(0, cache.slots[1].entry_index);
	TEST_ASSERT_NULL(cache.slots[1].data);
}

void test_LauncherThumbnail_cacheEvict_returns_false_when_empty(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheEvict(&cache));
}

void test_LauncherThumbnail_cacheEvict_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheEvict(NULL));
}

///////////////////////////////
// LauncherThumbnail_cacheGetData tests
///////////////////////////////

void test_LauncherThumbnail_cacheGetData_returns_data(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	void* data = (void*)0xBEEF;
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", data);

	TEST_ASSERT_EQUAL_PTR(data, LauncherThumbnail_cacheGetData(&cache, 0));
}

void test_LauncherThumbnail_cacheGetData_returns_null_for_invalid_slot(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetData(&cache, -1));
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetData(&cache, 1));
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetData(&cache, 99));
}

void test_LauncherThumbnail_cacheGetData_handles_null(void) {
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetData(NULL, 0));
}

///////////////////////////////
// LauncherThumbnail_cacheGetSlot tests
///////////////////////////////

void test_LauncherThumbnail_cacheGetSlot_returns_slot(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	LauncherThumbnailCacheSlot* slot = LauncherThumbnail_cacheGetSlot(&cache, 0);
	TEST_ASSERT_NOT_NULL(slot);
	TEST_ASSERT_EQUAL(42, slot->entry_index);
}

void test_LauncherThumbnail_cacheGetSlot_returns_null_for_invalid(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0xBEEF);

	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetSlot(&cache, -1));
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetSlot(&cache, 1));
}

void test_LauncherThumbnail_cacheGetSlot_handles_null(void) {
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetSlot(NULL, 0));
}

///////////////////////////////
// LauncherThumbnail_cacheClear tests
///////////////////////////////

void test_LauncherThumbnail_cacheClear_sets_size_zero(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/path/b.png", (void*)0x20);

	LauncherThumbnail_cacheClear(&cache);
	TEST_ASSERT_EQUAL(0, cache.size);
}

void test_LauncherThumbnail_cacheClear_clears_slots(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/path/a.png", (void*)0x10);

	LauncherThumbnail_cacheClear(&cache);
	TEST_ASSERT_NULL(cache.slots[0].data);
	TEST_ASSERT_EQUAL(0, cache.slots[0].entry_index);
}

void test_LauncherThumbnail_cacheClear_handles_null(void) {
	// Should not crash
	LauncherThumbnail_cacheClear(NULL);
}

///////////////////////////////
// LauncherThumbnail_preloadGetHintIndex tests
///////////////////////////////

void test_LauncherThumbnail_preloadGetHintIndex_scrolling_down(void) {
	// Current > last means scrolling down, preload next
	TEST_ASSERT_EQUAL(11, LauncherThumbnail_preloadGetHintIndex(10, 9, 100));
}

void test_LauncherThumbnail_preloadGetHintIndex_scrolling_up(void) {
	// Current < last means scrolling up, preload previous
	TEST_ASSERT_EQUAL(9, LauncherThumbnail_preloadGetHintIndex(10, 11, 100));
}

void test_LauncherThumbnail_preloadGetHintIndex_at_end(void) {
	// At last item, scrolling down would exceed bounds
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_preloadGetHintIndex(99, 98, 100));
}

void test_LauncherThumbnail_preloadGetHintIndex_at_start(void) {
	// At first item, scrolling up would go negative
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_preloadGetHintIndex(0, 1, 100));
}

void test_LauncherThumbnail_preloadGetHintIndex_same_position(void) {
	// Same position (no scroll) - no preload hint needed
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_preloadGetHintIndex(10, 10, 100));
}

void test_LauncherThumbnail_preloadGetHintIndex_empty_directory(void) {
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_preloadGetHintIndex(0, 0, 0));
}

void test_LauncherThumbnail_preloadGetHintIndex_single_item(void) {
	// Single item, preload would be out of bounds
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_preloadGetHintIndex(0, 0, 1));
}

///////////////////////////////
// LauncherThumbnail_fadeInit tests
///////////////////////////////

void test_LauncherThumbnail_fadeInit_sets_default_duration(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 0);
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_FADE_DURATION_MS, state.duration_ms);
}

void test_LauncherThumbnail_fadeInit_sets_custom_duration(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 500);
	TEST_ASSERT_EQUAL(500, state.duration_ms);
}

void test_LauncherThumbnail_fadeInit_sets_max_alpha(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
}

void test_LauncherThumbnail_fadeInit_not_fading(void) {
	LauncherThumbnailFadeState state;
	state.start_ms = 12345; // Garbage
	LauncherThumbnail_fadeInit(&state, 200);
	TEST_ASSERT_EQUAL(0, state.start_ms);
}

void test_LauncherThumbnail_fadeInit_handles_null(void) {
	// Should not crash
	LauncherThumbnail_fadeInit(NULL, 200);
}

///////////////////////////////
// LauncherThumbnail_fadeStart tests
///////////////////////////////

void test_LauncherThumbnail_fadeStart_sets_start_time(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	TEST_ASSERT_EQUAL(1000, state.start_ms);
}

void test_LauncherThumbnail_fadeStart_sets_min_alpha(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MIN, state.alpha);
}

void test_LauncherThumbnail_fadeStart_handles_null(void) {
	// Should not crash
	LauncherThumbnail_fadeStart(NULL, 1000);
}

///////////////////////////////
// LauncherThumbnail_fadeReset tests
///////////////////////////////

void test_LauncherThumbnail_fadeReset_clears_start_time(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	LauncherThumbnail_fadeReset(&state);
	TEST_ASSERT_EQUAL(0, state.start_ms);
}

void test_LauncherThumbnail_fadeReset_sets_max_alpha(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	LauncherThumbnail_fadeReset(&state);
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
}

void test_LauncherThumbnail_fadeReset_handles_null(void) {
	// Should not crash
	LauncherThumbnail_fadeReset(NULL);
}

///////////////////////////////
// LauncherThumbnail_fadeUpdate tests
///////////////////////////////

void test_LauncherThumbnail_fadeUpdate_returns_false_when_not_fading(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeUpdate(&state, 1000));
}

void test_LauncherThumbnail_fadeUpdate_returns_true_while_fading(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	TEST_ASSERT_TRUE(LauncherThumbnail_fadeUpdate(&state, 1100));
}

void test_LauncherThumbnail_fadeUpdate_increases_alpha(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);

	LauncherThumbnail_fadeUpdate(&state, 1100); // 50% through
	TEST_ASSERT_GREATER_THAN(LAUNCHER_THUMBNAIL_ALPHA_MIN, state.alpha);
	TEST_ASSERT_LESS_THAN(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
}

void test_LauncherThumbnail_fadeUpdate_completes_at_duration(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);

	LauncherThumbnail_fadeUpdate(&state, 1200); // Exactly at duration
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
	TEST_ASSERT_EQUAL(0, state.start_ms); // No longer fading
}

void test_LauncherThumbnail_fadeUpdate_handles_overshoot(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);

	LauncherThumbnail_fadeUpdate(&state, 2000); // Way past duration
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
}

void test_LauncherThumbnail_fadeUpdate_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeUpdate(NULL, 1000));
}

///////////////////////////////
// LauncherThumbnail_fadeIsActive tests
///////////////////////////////

void test_LauncherThumbnail_fadeIsActive_false_when_not_started(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeIsActive(&state));
}

void test_LauncherThumbnail_fadeIsActive_true_when_fading(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	TEST_ASSERT_TRUE(LauncherThumbnail_fadeIsActive(&state));
}

void test_LauncherThumbnail_fadeIsActive_false_after_complete(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 200);
	LauncherThumbnail_fadeStart(&state, 1000);
	LauncherThumbnail_fadeUpdate(&state, 1200); // Complete
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeIsActive(&state));
}

void test_LauncherThumbnail_fadeIsActive_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeIsActive(NULL));
}

///////////////////////////////
// LauncherThumbnail_fadeCalculateAlpha tests
///////////////////////////////

void test_LauncherThumbnail_fadeCalculateAlpha_zero_at_start(void) {
	TEST_ASSERT_EQUAL(0, LauncherThumbnail_fadeCalculateAlpha(0, 200, 255));
}

void test_LauncherThumbnail_fadeCalculateAlpha_max_at_end(void) {
	TEST_ASSERT_EQUAL(255, LauncherThumbnail_fadeCalculateAlpha(200, 200, 255));
}

void test_LauncherThumbnail_fadeCalculateAlpha_max_past_end(void) {
	TEST_ASSERT_EQUAL(255, LauncherThumbnail_fadeCalculateAlpha(300, 200, 255));
}

void test_LauncherThumbnail_fadeCalculateAlpha_smoothstep_midpoint(void) {
	// At t=0.5, smoothstep gives 0.5 * 0.5 * (3 - 2 * 0.5) = 0.25 * 2 = 0.5
	int alpha = LauncherThumbnail_fadeCalculateAlpha(100, 200, 255);
	// Should be close to 127 (half of 255)
	TEST_ASSERT_INT_WITHIN(5, 127, alpha);
}

void test_LauncherThumbnail_fadeCalculateAlpha_smoothstep_easing(void) {
	// Verify smoothstep easing properties (starts slow, accelerates, ends slow)
	int alpha_10 = LauncherThumbnail_fadeCalculateAlpha(20, 200, 255);   // 10%
	int alpha_25 = LauncherThumbnail_fadeCalculateAlpha(50, 200, 255);   // 25%
	int alpha_50 = LauncherThumbnail_fadeCalculateAlpha(100, 200, 255);  // 50%
	int alpha_75 = LauncherThumbnail_fadeCalculateAlpha(150, 200, 255);  // 75%
	int alpha_90 = LauncherThumbnail_fadeCalculateAlpha(180, 200, 255);  // 90%

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

void test_LauncherThumbnail_fadeCalculateAlpha_zero_duration(void) {
	// Division by zero protection
	TEST_ASSERT_EQUAL(255, LauncherThumbnail_fadeCalculateAlpha(100, 0, 255));
}

void test_LauncherThumbnail_fadeCalculateAlpha_custom_max(void) {
	TEST_ASSERT_EQUAL(100, LauncherThumbnail_fadeCalculateAlpha(200, 200, 100));
}

///////////////////////////////
// Displayed Item Tracking tests
///////////////////////////////

void test_LauncherThumbnail_cacheSetDisplayed_sets_index(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);

	LauncherThumbnail_cacheSetDisplayed(&cache, 42);
	TEST_ASSERT_EQUAL(42, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
}

void test_LauncherThumbnail_cacheSetDisplayed_sets_valid_when_in_cache(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);

	LauncherThumbnail_cacheSetDisplayed(&cache, 42);
	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsDisplayedValid(&cache));
}

void test_LauncherThumbnail_cacheSetDisplayed_not_valid_when_not_in_cache(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);

	LauncherThumbnail_cacheSetDisplayed(&cache, 99); // Not in cache
	TEST_ASSERT_EQUAL(99, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsDisplayedValid(&cache));
}

void test_LauncherThumbnail_cacheSetDisplayed_handles_null(void) {
	// Should not crash
	LauncherThumbnail_cacheSetDisplayed(NULL, 42);
}

void test_LauncherThumbnail_cacheClearDisplayed_resets_state(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);
	LauncherThumbnail_cacheSetDisplayed(&cache, 42);

	LauncherThumbnail_cacheClearDisplayed(&cache);
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsDisplayedValid(&cache));
}

void test_LauncherThumbnail_cacheClearDisplayed_handles_null(void) {
	// Should not crash
	LauncherThumbnail_cacheClearDisplayed(NULL);
}

void test_LauncherThumbnail_cacheIsDisplayedValid_handles_null(void) {
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsDisplayedValid(NULL));
}

void test_LauncherThumbnail_cacheGetDisplayedIndex_returns_negative_when_none(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
}

void test_LauncherThumbnail_cacheGetDisplayedIndex_handles_null(void) {
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheGetDisplayedIndex(NULL));
}

void test_LauncherThumbnail_cacheGetDisplayedData_returns_data(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	void* data = (void*)0xCAFE;
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", data);
	LauncherThumbnail_cacheSetDisplayed(&cache, 42);

	TEST_ASSERT_EQUAL_PTR(data, LauncherThumbnail_cacheGetDisplayedData(&cache));
}

void test_LauncherThumbnail_cacheGetDisplayedData_returns_null_when_none(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetDisplayedData(&cache));
}

void test_LauncherThumbnail_cacheGetDisplayedData_returns_null_when_evicted(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);
	LauncherThumbnail_cacheSetDisplayed(&cache, 42);

	// Evict the displayed item
	LauncherThumbnail_cacheEvict(&cache);

	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetDisplayedData(&cache));
}

void test_LauncherThumbnail_cacheGetDisplayedData_handles_null(void) {
	TEST_ASSERT_NULL(LauncherThumbnail_cacheGetDisplayedData(NULL));
}

void test_LauncherThumbnail_cacheEvict_invalidates_displayed_item(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/b.png", (void*)0x20);
	LauncherThumbnail_cacheSetDisplayed(&cache, 10); // Display item at slot 0

	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsDisplayedValid(&cache));

	// Evict slot 0 (the displayed item)
	LauncherThumbnail_cacheEvict(&cache);

	// displayed_index stays the same, but valid becomes false
	TEST_ASSERT_EQUAL(10, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
	TEST_ASSERT_FALSE(LauncherThumbnail_cacheIsDisplayedValid(&cache));
}

void test_LauncherThumbnail_cacheEvict_keeps_displayed_valid_if_not_evicted(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 10, "/a.png", (void*)0x10);
	LauncherThumbnail_cacheAdd(&cache, 20, "/b.png", (void*)0x20);
	LauncherThumbnail_cacheSetDisplayed(&cache, 20); // Display item at slot 1

	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsDisplayedValid(&cache));

	// Evict slot 0 (NOT the displayed item)
	LauncherThumbnail_cacheEvict(&cache);

	// displayed_index and valid should stay the same
	TEST_ASSERT_EQUAL(20, LauncherThumbnail_cacheGetDisplayedIndex(&cache));
	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsDisplayedValid(&cache));
}

void test_LauncherThumbnail_cacheInit_clears_displayed_tracking(void) {
	LauncherThumbnailCache cache;
	// Put garbage in displayed fields
	cache.displayed_index = 99;
	cache.displayed_valid = 1;

	LauncherThumbnail_cacheInit(&cache);
	TEST_ASSERT_EQUAL(-1, cache.displayed_index);
	TEST_ASSERT_EQUAL(0, cache.displayed_valid);
}

void test_LauncherThumbnail_cacheClear_resets_displayed_tracking(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);
	LauncherThumbnail_cacheAdd(&cache, 42, "/path/thumb.png", (void*)0x1000);
	LauncherThumbnail_cacheSetDisplayed(&cache, 42);

	LauncherThumbnail_cacheClear(&cache);
	TEST_ASSERT_EQUAL(-1, cache.displayed_index);
	TEST_ASSERT_EQUAL(0, cache.displayed_valid);
}

///////////////////////////////
// Integration scenarios
///////////////////////////////

void test_cache_fifo_eviction_order(void) {
	LauncherThumbnailCache cache;
	LauncherThumbnail_cacheInit(&cache);

	// Fill cache
	LauncherThumbnail_cacheAdd(&cache, 1, "/a.png", (void*)0x1);
	LauncherThumbnail_cacheAdd(&cache, 2, "/b.png", (void*)0x2);
	LauncherThumbnail_cacheAdd(&cache, 3, "/c.png", (void*)0x3);

	TEST_ASSERT_TRUE(LauncherThumbnail_cacheIsFull(&cache));

	// Evict and add new item
	LauncherThumbnail_cacheEvict(&cache);
	LauncherThumbnail_cacheAdd(&cache, 4, "/d.png", (void*)0x4);

	// Should have 2, 3, 4 now
	TEST_ASSERT_EQUAL(-1, LauncherThumbnail_cacheFind(&cache, 1)); // Evicted
	TEST_ASSERT_EQUAL(0, LauncherThumbnail_cacheFind(&cache, 2));
	TEST_ASSERT_EQUAL(1, LauncherThumbnail_cacheFind(&cache, 3));
	TEST_ASSERT_EQUAL(2, LauncherThumbnail_cacheFind(&cache, 4));
}

void test_fade_full_cycle(void) {
	LauncherThumbnailFadeState state;
	LauncherThumbnail_fadeInit(&state, 100);

	// Start fade
	LauncherThumbnail_fadeStart(&state, 1000);
	TEST_ASSERT_TRUE(LauncherThumbnail_fadeIsActive(&state));
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MIN, state.alpha);

	// Update partway
	LauncherThumbnail_fadeUpdate(&state, 1050);
	TEST_ASSERT_TRUE(LauncherThumbnail_fadeIsActive(&state));
	TEST_ASSERT_GREATER_THAN(0, state.alpha);
	TEST_ASSERT_LESS_THAN(255, state.alpha);

	// Complete
	LauncherThumbnail_fadeUpdate(&state, 1100);
	TEST_ASSERT_FALSE(LauncherThumbnail_fadeIsActive(&state));
	TEST_ASSERT_EQUAL(LAUNCHER_THUMBNAIL_ALPHA_MAX, state.alpha);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// LauncherThumbnail_cacheInit tests
	RUN_TEST(test_LauncherThumbnail_cacheInit_sets_size_zero);
	RUN_TEST(test_LauncherThumbnail_cacheInit_clears_slots);
	RUN_TEST(test_LauncherThumbnail_cacheInit_handles_null);

	// LauncherThumbnail_cacheFind tests
	RUN_TEST(test_LauncherThumbnail_cacheFind_returns_negative_when_empty);
	RUN_TEST(test_LauncherThumbnail_cacheFind_returns_slot_index);
	RUN_TEST(test_LauncherThumbnail_cacheFind_returns_negative_for_missing);
	RUN_TEST(test_LauncherThumbnail_cacheFind_handles_null);

	// LauncherThumbnail_cacheIsFull tests
	RUN_TEST(test_LauncherThumbnail_cacheIsFull_returns_false_when_empty);
	RUN_TEST(test_LauncherThumbnail_cacheIsFull_returns_false_when_partial);
	RUN_TEST(test_LauncherThumbnail_cacheIsFull_returns_true_when_full);
	RUN_TEST(test_LauncherThumbnail_cacheIsFull_handles_null);

	// LauncherThumbnail_cacheGetEvictSlot tests
	RUN_TEST(test_LauncherThumbnail_cacheGetEvictSlot_returns_negative_when_not_full);
	RUN_TEST(test_LauncherThumbnail_cacheGetEvictSlot_returns_zero_when_full);
	RUN_TEST(test_LauncherThumbnail_cacheGetEvictSlot_handles_null);

	// LauncherThumbnail_cacheAdd tests
	RUN_TEST(test_LauncherThumbnail_cacheAdd_stores_entry_index);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_stores_path);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_stores_data);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_increments_size);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_fails_when_full);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_handles_null_cache);
	RUN_TEST(test_LauncherThumbnail_cacheAdd_handles_null_path);

	// LauncherThumbnail_cacheEvict tests
	RUN_TEST(test_LauncherThumbnail_cacheEvict_removes_first_slot);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_shifts_items_left);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_clears_last_slot);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_returns_false_when_empty);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_handles_null);

	// LauncherThumbnail_cacheGetData tests
	RUN_TEST(test_LauncherThumbnail_cacheGetData_returns_data);
	RUN_TEST(test_LauncherThumbnail_cacheGetData_returns_null_for_invalid_slot);
	RUN_TEST(test_LauncherThumbnail_cacheGetData_handles_null);

	// LauncherThumbnail_cacheGetSlot tests
	RUN_TEST(test_LauncherThumbnail_cacheGetSlot_returns_slot);
	RUN_TEST(test_LauncherThumbnail_cacheGetSlot_returns_null_for_invalid);
	RUN_TEST(test_LauncherThumbnail_cacheGetSlot_handles_null);

	// LauncherThumbnail_cacheClear tests
	RUN_TEST(test_LauncherThumbnail_cacheClear_sets_size_zero);
	RUN_TEST(test_LauncherThumbnail_cacheClear_clears_slots);
	RUN_TEST(test_LauncherThumbnail_cacheClear_handles_null);

	// LauncherThumbnail_preloadGetHintIndex tests
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_scrolling_down);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_scrolling_up);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_at_end);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_at_start);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_same_position);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_empty_directory);
	RUN_TEST(test_LauncherThumbnail_preloadGetHintIndex_single_item);

	// LauncherThumbnail_fadeInit tests
	RUN_TEST(test_LauncherThumbnail_fadeInit_sets_default_duration);
	RUN_TEST(test_LauncherThumbnail_fadeInit_sets_custom_duration);
	RUN_TEST(test_LauncherThumbnail_fadeInit_sets_max_alpha);
	RUN_TEST(test_LauncherThumbnail_fadeInit_not_fading);
	RUN_TEST(test_LauncherThumbnail_fadeInit_handles_null);

	// LauncherThumbnail_fadeStart tests
	RUN_TEST(test_LauncherThumbnail_fadeStart_sets_start_time);
	RUN_TEST(test_LauncherThumbnail_fadeStart_sets_min_alpha);
	RUN_TEST(test_LauncherThumbnail_fadeStart_handles_null);

	// LauncherThumbnail_fadeReset tests
	RUN_TEST(test_LauncherThumbnail_fadeReset_clears_start_time);
	RUN_TEST(test_LauncherThumbnail_fadeReset_sets_max_alpha);
	RUN_TEST(test_LauncherThumbnail_fadeReset_handles_null);

	// LauncherThumbnail_fadeUpdate tests
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_returns_false_when_not_fading);
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_returns_true_while_fading);
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_increases_alpha);
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_completes_at_duration);
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_handles_overshoot);
	RUN_TEST(test_LauncherThumbnail_fadeUpdate_handles_null);

	// LauncherThumbnail_fadeIsActive tests
	RUN_TEST(test_LauncherThumbnail_fadeIsActive_false_when_not_started);
	RUN_TEST(test_LauncherThumbnail_fadeIsActive_true_when_fading);
	RUN_TEST(test_LauncherThumbnail_fadeIsActive_false_after_complete);
	RUN_TEST(test_LauncherThumbnail_fadeIsActive_handles_null);

	// LauncherThumbnail_fadeCalculateAlpha tests
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_zero_at_start);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_max_at_end);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_max_past_end);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_smoothstep_midpoint);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_smoothstep_easing);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_zero_duration);
	RUN_TEST(test_LauncherThumbnail_fadeCalculateAlpha_custom_max);

	// Displayed Item Tracking tests
	RUN_TEST(test_LauncherThumbnail_cacheSetDisplayed_sets_index);
	RUN_TEST(test_LauncherThumbnail_cacheSetDisplayed_sets_valid_when_in_cache);
	RUN_TEST(test_LauncherThumbnail_cacheSetDisplayed_not_valid_when_not_in_cache);
	RUN_TEST(test_LauncherThumbnail_cacheSetDisplayed_handles_null);
	RUN_TEST(test_LauncherThumbnail_cacheClearDisplayed_resets_state);
	RUN_TEST(test_LauncherThumbnail_cacheClearDisplayed_handles_null);
	RUN_TEST(test_LauncherThumbnail_cacheIsDisplayedValid_handles_null);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedIndex_returns_negative_when_none);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedIndex_handles_null);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedData_returns_data);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedData_returns_null_when_none);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedData_returns_null_when_evicted);
	RUN_TEST(test_LauncherThumbnail_cacheGetDisplayedData_handles_null);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_invalidates_displayed_item);
	RUN_TEST(test_LauncherThumbnail_cacheEvict_keeps_displayed_valid_if_not_evicted);
	RUN_TEST(test_LauncherThumbnail_cacheInit_clears_displayed_tracking);
	RUN_TEST(test_LauncherThumbnail_cacheClear_resets_displayed_tracking);

	// Integration scenarios
	RUN_TEST(test_cache_fifo_eviction_order);
	RUN_TEST(test_fade_full_cycle);

	return UNITY_END();
}
