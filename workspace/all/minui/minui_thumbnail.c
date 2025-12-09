/**
 * minui_thumbnail.c - Thumbnail cache and fade animation
 *
 * Implements pure cache management logic and fade animation math.
 * All SDL dependencies are kept in minui.c; this module is fully testable.
 */

#include "minui_thumbnail.h"

#include <string.h>

///////////////////////////////
// Thumbnail Cache
///////////////////////////////

void MinUIThumbnail_cacheInit(MinUIThumbnailCache* cache) {
	if (!cache)
		return;
	memset(cache, 0, sizeof(MinUIThumbnailCache));
	cache->size = 0;
	cache->displayed_index = -1;
	cache->displayed_valid = 0;
}

int MinUIThumbnail_cacheFind(const MinUIThumbnailCache* cache, int entry_index) {
	if (!cache)
		return -1;

	for (int i = 0; i < cache->size; i++) {
		if (cache->slots[i].entry_index == entry_index) {
			return i;
		}
	}
	return -1;
}

int MinUIThumbnail_cacheIsFull(const MinUIThumbnailCache* cache) {
	if (!cache)
		return 0;
	return cache->size >= MINUI_THUMBNAIL_CACHE_SIZE;
}

int MinUIThumbnail_cacheGetEvictSlot(const MinUIThumbnailCache* cache) {
	if (!cache)
		return -1;
	// FIFO policy: evict slot 0 (oldest) when full
	return (cache->size >= MINUI_THUMBNAIL_CACHE_SIZE) ? 0 : -1;
}

int MinUIThumbnail_cacheAdd(MinUIThumbnailCache* cache, int entry_index, const char* path,
                            void* data) {
	if (!cache || !path)
		return 0;

	// Fail if cache is full - caller must evict first
	if (cache->size >= MINUI_THUMBNAIL_CACHE_SIZE)
		return 0;

	// Add to end
	MinUIThumbnailCacheSlot* slot = &cache->slots[cache->size];
	slot->entry_index = entry_index;
	strncpy(slot->path, path, MAX_PATH - 1);
	slot->path[MAX_PATH - 1] = '\0';
	slot->data = data;
	cache->size++;

	return 1;
}

int MinUIThumbnail_cacheEvict(MinUIThumbnailCache* cache) {
	if (!cache || cache->size == 0)
		return 0;

	// Check if we're evicting the displayed item - auto-invalidate
	if (cache->displayed_valid && cache->slots[0].entry_index == cache->displayed_index) {
		cache->displayed_valid = 0;
	}

	// Clear evicted slot data pointer (caller already freed it)
	cache->slots[0].data = NULL;

	// Shift remaining items left
	if (cache->size > 1) {
		memmove(&cache->slots[0], &cache->slots[1],
		        (cache->size - 1) * sizeof(MinUIThumbnailCacheSlot));
	}

	// Clear the now-empty last slot
	cache->size--;
	memset(&cache->slots[cache->size], 0, sizeof(MinUIThumbnailCacheSlot));

	return 1;
}

void* MinUIThumbnail_cacheGetData(const MinUIThumbnailCache* cache, int slot) {
	if (!cache || slot < 0 || slot >= cache->size)
		return NULL;
	return cache->slots[slot].data;
}

MinUIThumbnailCacheSlot* MinUIThumbnail_cacheGetSlot(MinUIThumbnailCache* cache, int slot) {
	if (!cache || slot < 0 || slot >= cache->size)
		return NULL;
	return &cache->slots[slot];
}

void MinUIThumbnail_cacheClear(MinUIThumbnailCache* cache) {
	if (!cache)
		return;
	// Caller must free all data pointers before calling this
	memset(cache->slots, 0, sizeof(cache->slots));
	cache->size = 0;
	cache->displayed_index = -1;
	cache->displayed_valid = 0;
}

///////////////////////////////
// Displayed Item Tracking
///////////////////////////////

void MinUIThumbnail_cacheSetDisplayed(MinUIThumbnailCache* cache, int entry_index) {
	if (!cache)
		return;
	cache->displayed_index = entry_index;
	cache->displayed_valid = (MinUIThumbnail_cacheFind(cache, entry_index) >= 0);
}

void MinUIThumbnail_cacheClearDisplayed(MinUIThumbnailCache* cache) {
	if (!cache)
		return;
	cache->displayed_index = -1;
	cache->displayed_valid = 0;
}

int MinUIThumbnail_cacheIsDisplayedValid(const MinUIThumbnailCache* cache) {
	if (!cache)
		return 0;
	return cache->displayed_valid;
}

int MinUIThumbnail_cacheGetDisplayedIndex(const MinUIThumbnailCache* cache) {
	if (!cache)
		return -1;
	return cache->displayed_index;
}

void* MinUIThumbnail_cacheGetDisplayedData(const MinUIThumbnailCache* cache) {
	if (!cache || !cache->displayed_valid || cache->displayed_index < 0)
		return NULL;
	int slot = MinUIThumbnail_cacheFind(cache, cache->displayed_index);
	if (slot < 0)
		return NULL;
	return cache->slots[slot].data;
}

///////////////////////////////
// Preload Calculation
///////////////////////////////

int MinUIThumbnail_preloadGetHintIndex(int current_index, int last_index, int total_count) {
	if (total_count <= 0)
		return -1;

	// No preload hint if position hasn't changed
	if (current_index == last_index)
		return -1;

	// Determine scroll direction
	int direction = (current_index > last_index) ? 1 : -1;

	// Calculate hint index in scroll direction
	int hint_index = current_index + direction;

	// Check bounds
	if (hint_index < 0 || hint_index >= total_count)
		return -1;

	return hint_index;
}

///////////////////////////////
// Fade Animation
///////////////////////////////

void MinUIThumbnail_fadeInit(MinUIThumbnailFadeState* state, int duration_ms) {
	if (!state)
		return;
	state->start_ms = 0;
	state->alpha = MINUI_THUMBNAIL_ALPHA_MAX;
	state->duration_ms = duration_ms > 0 ? duration_ms : MINUI_THUMBNAIL_FADE_DURATION_MS;
}

void MinUIThumbnail_fadeStart(MinUIThumbnailFadeState* state, unsigned long now_ms) {
	if (!state)
		return;
	state->start_ms = now_ms;
	state->alpha = MINUI_THUMBNAIL_ALPHA_MIN;
}

void MinUIThumbnail_fadeReset(MinUIThumbnailFadeState* state) {
	if (!state)
		return;
	state->start_ms = 0;
	state->alpha = MINUI_THUMBNAIL_ALPHA_MAX;
}

int MinUIThumbnail_fadeUpdate(MinUIThumbnailFadeState* state, unsigned long now_ms) {
	if (!state || state->start_ms == 0)
		return 0;

	unsigned long elapsed = now_ms - state->start_ms;

	if (elapsed >= (unsigned long)state->duration_ms) {
		// Fade complete
		state->alpha = MINUI_THUMBNAIL_ALPHA_MAX;
		state->start_ms = 0;
		return 1; // One final update to mark completion
	}

	// Calculate eased alpha
	state->alpha =
	    MinUIThumbnail_fadeCalculateAlpha(elapsed, state->duration_ms, MINUI_THUMBNAIL_ALPHA_MAX);
	return 1;
}

int MinUIThumbnail_fadeIsActive(const MinUIThumbnailFadeState* state) {
	if (!state)
		return 0;
	return state->start_ms != 0;
}

int MinUIThumbnail_fadeCalculateAlpha(unsigned long elapsed_ms, unsigned long duration_ms,
                                      int max_alpha) {
	if (duration_ms == 0)
		return max_alpha;

	if (elapsed_ms >= duration_ms)
		return max_alpha;

	// Smoothstep easing: f(t) = t * t * (3 - 2 * t)
	// This gives a smooth S-curve acceleration/deceleration
	float t = (float)elapsed_ms / (float)duration_ms;
	float eased = t * t * (3.0f - 2.0f * t);

	return (int)(eased * max_alpha);
}
