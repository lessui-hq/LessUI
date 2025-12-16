/**
 * launcher_thumbnail.c - Thumbnail cache and fade animation
 *
 * Implements pure cache management logic and fade animation math.
 * All SDL dependencies are kept in launcher.c; this module is fully testable.
 */

#include "launcher_thumbnail.h"

#include <string.h>

///////////////////////////////
// Thumbnail Cache
///////////////////////////////

void LauncherThumbnail_cacheInit(LauncherThumbnailCache* cache) {
	if (!cache)
		return;
	memset(cache, 0, sizeof(LauncherThumbnailCache));
	cache->size = 0;
	cache->displayed_index = -1;
	cache->displayed_valid = 0;
}

int LauncherThumbnail_cacheFind(const LauncherThumbnailCache* cache, int entry_index) {
	if (!cache)
		return -1;

	for (int i = 0; i < cache->size; i++) {
		if (cache->slots[i].entry_index == entry_index) {
			return i;
		}
	}
	return -1;
}

int LauncherThumbnail_cacheIsFull(const LauncherThumbnailCache* cache) {
	if (!cache)
		return 0;
	return cache->size >= LAUNCHER_THUMBNAIL_CACHE_SIZE;
}

int LauncherThumbnail_cacheGetEvictSlot(const LauncherThumbnailCache* cache) {
	if (!cache)
		return -1;
	// FIFO policy: evict slot 0 (oldest) when full
	return (cache->size >= LAUNCHER_THUMBNAIL_CACHE_SIZE) ? 0 : -1;
}

int LauncherThumbnail_cacheAdd(LauncherThumbnailCache* cache, int entry_index, const char* path,
                               void* data) {
	if (!cache || !path)
		return 0;

	// Fail if cache is full - caller must evict first
	if (cache->size >= LAUNCHER_THUMBNAIL_CACHE_SIZE)
		return 0;

	// Add to end
	LauncherThumbnailCacheSlot* slot = &cache->slots[cache->size];
	slot->entry_index = entry_index;
	strncpy(slot->path, path, MAX_PATH - 1);
	slot->path[MAX_PATH - 1] = '\0';
	slot->data = data;
	cache->size++;

	return 1;
}

int LauncherThumbnail_cacheEvict(LauncherThumbnailCache* cache) {
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
		        (cache->size - 1) * sizeof(LauncherThumbnailCacheSlot));
	}

	// Clear the now-empty last slot
	cache->size--;
	memset(&cache->slots[cache->size], 0, sizeof(LauncherThumbnailCacheSlot));

	return 1;
}

void* LauncherThumbnail_cacheGetData(const LauncherThumbnailCache* cache, int slot) {
	if (!cache || slot < 0 || slot >= cache->size)
		return NULL;
	return cache->slots[slot].data;
}

LauncherThumbnailCacheSlot* LauncherThumbnail_cacheGetSlot(LauncherThumbnailCache* cache,
                                                           int slot) {
	if (!cache || slot < 0 || slot >= cache->size)
		return NULL;
	return &cache->slots[slot];
}

void LauncherThumbnail_cacheClear(LauncherThumbnailCache* cache) {
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

void LauncherThumbnail_cacheSetDisplayed(LauncherThumbnailCache* cache, int entry_index) {
	if (!cache)
		return;
	cache->displayed_index = entry_index;
	cache->displayed_valid = (LauncherThumbnail_cacheFind(cache, entry_index) >= 0);
}

void LauncherThumbnail_cacheClearDisplayed(LauncherThumbnailCache* cache) {
	if (!cache)
		return;
	cache->displayed_index = -1;
	cache->displayed_valid = 0;
}

int LauncherThumbnail_cacheIsDisplayedValid(const LauncherThumbnailCache* cache) {
	if (!cache)
		return 0;
	return cache->displayed_valid;
}

int LauncherThumbnail_cacheGetDisplayedIndex(const LauncherThumbnailCache* cache) {
	if (!cache)
		return -1;
	return cache->displayed_index;
}

void* LauncherThumbnail_cacheGetDisplayedData(const LauncherThumbnailCache* cache) {
	if (!cache || !cache->displayed_valid || cache->displayed_index < 0)
		return NULL;
	int slot = LauncherThumbnail_cacheFind(cache, cache->displayed_index);
	if (slot < 0)
		return NULL;
	return cache->slots[slot].data;
}

///////////////////////////////
// Preload Calculation
///////////////////////////////

int LauncherThumbnail_preloadGetHintIndex(int current_index, int last_index, int total_count) {
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

void LauncherThumbnail_fadeInit(LauncherThumbnailFadeState* state, int duration_ms) {
	if (!state)
		return;
	state->start_ms = 0;
	state->alpha = LAUNCHER_THUMBNAIL_ALPHA_MAX;
	state->duration_ms = duration_ms > 0 ? duration_ms : LAUNCHER_THUMBNAIL_FADE_DURATION_MS;
}

void LauncherThumbnail_fadeStart(LauncherThumbnailFadeState* state, unsigned long now_ms) {
	if (!state)
		return;
	state->start_ms = now_ms;
	state->alpha = LAUNCHER_THUMBNAIL_ALPHA_MIN;
}

void LauncherThumbnail_fadeReset(LauncherThumbnailFadeState* state) {
	if (!state)
		return;
	state->start_ms = 0;
	state->alpha = LAUNCHER_THUMBNAIL_ALPHA_MAX;
}

int LauncherThumbnail_fadeUpdate(LauncherThumbnailFadeState* state, unsigned long now_ms) {
	if (!state || state->start_ms == 0)
		return 0;

	unsigned long elapsed = now_ms - state->start_ms;

	if (elapsed >= (unsigned long)state->duration_ms) {
		// Fade complete
		state->alpha = LAUNCHER_THUMBNAIL_ALPHA_MAX;
		state->start_ms = 0;
		return 1; // One final update to mark completion
	}

	// Calculate eased alpha
	state->alpha = LauncherThumbnail_fadeCalculateAlpha(elapsed, state->duration_ms,
	                                                    LAUNCHER_THUMBNAIL_ALPHA_MAX);
	return 1;
}

int LauncherThumbnail_fadeIsActive(const LauncherThumbnailFadeState* state) {
	if (!state)
		return 0;
	return state->start_ms != 0;
}

int LauncherThumbnail_fadeCalculateAlpha(unsigned long elapsed_ms, unsigned long duration_ms,
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
