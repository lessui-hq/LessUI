/**
 * minui_thumbnail.h - Thumbnail cache and fade animation
 *
 * Provides pure data structures and algorithms for thumbnail management:
 * - FIFO cache with fixed capacity
 * - Preload hint calculation based on scroll direction
 * - Smoothstep fade animation calculation
 *
 * Design: Cache uses opaque data pointers (void*) so logic is testable
 * without SDL dependencies. Caller manages surface allocation/freeing.
 */

#ifndef MINUI_THUMBNAIL_H
#define MINUI_THUMBNAIL_H

#include "defines.h"

///////////////////////////////
// Thumbnail Cache
///////////////////////////////

/** Maximum number of thumbnails to keep in cache */
#define THUMB_CACHE_SIZE 3

/**
 * A slot in the thumbnail cache.
 *
 * Stores the path, entry index, and an opaque data pointer.
 * In production, data points to SDL_Surface*.
 */
typedef struct {
	char path[MAX_PATH];
	int entry_index;
	void* data; // Opaque - caller manages (SDL_Surface* in production)
} ThumbCacheSlot;

/**
 * FIFO thumbnail cache.
 *
 * Evicts oldest entry when full (slot 0 is oldest).
 * Entry index is the cache key (position in current directory).
 */
typedef struct {
	ThumbCacheSlot slots[THUMB_CACHE_SIZE];
	int size; // Number of valid slots (0 to THUMB_CACHE_SIZE)
} ThumbCache;

/**
 * Initialize cache to empty state.
 *
 * @param cache Cache to initialize
 */
void ThumbCache_init(ThumbCache* cache);

/**
 * Find slot index by entry index.
 *
 * @param cache Cache to search
 * @param entry_index Entry index to find
 * @return Slot index (0 to size-1) if found, -1 if not found
 */
int ThumbCache_find(const ThumbCache* cache, int entry_index);

/**
 * Check if cache is full.
 *
 * @param cache Cache to check
 * @return 1 if full, 0 otherwise
 */
int ThumbCache_isFull(const ThumbCache* cache);

/**
 * Get slot that would be evicted if cache is full.
 * Always returns slot 0 (oldest) when full.
 *
 * @param cache Cache to check
 * @return Slot index to evict (0) if full, -1 if not full
 */
int ThumbCache_getEvictSlot(const ThumbCache* cache);

/**
 * Add item to cache.
 *
 * If cache is full, caller must evict first using ThumbCache_evict().
 * This separation allows caller to free resources before eviction.
 *
 * @param cache Cache to modify
 * @param entry_index Entry index (cache key)
 * @param path Path to thumbnail file
 * @param data Opaque data pointer (SDL_Surface* in production)
 * @return 1 on success, 0 if cache is full
 */
int ThumbCache_add(ThumbCache* cache, int entry_index, const char* path, void* data);

/**
 * Evict oldest slot from cache.
 *
 * Shifts remaining items left. Caller should free the evicted
 * slot's data before calling this.
 *
 * @param cache Cache to modify
 * @return 1 on success, 0 if cache is empty
 */
int ThumbCache_evict(ThumbCache* cache);

/**
 * Get data pointer for a slot.
 *
 * @param cache Cache to query
 * @param slot Slot index (0 to size-1)
 * @return Data pointer, or NULL if slot is invalid
 */
void* ThumbCache_getData(const ThumbCache* cache, int slot);

/**
 * Get slot by index for clearing.
 *
 * @param cache Cache to query
 * @param slot Slot index (0 to size-1)
 * @return Pointer to slot, or NULL if invalid
 */
ThumbCacheSlot* ThumbCache_getSlot(ThumbCache* cache, int slot);

/**
 * Clear cache to empty state.
 * Caller must free all slot data before calling.
 *
 * @param cache Cache to clear
 */
void ThumbCache_clear(ThumbCache* cache);

///////////////////////////////
// Preload Calculation
///////////////////////////////

/**
 * Calculate index to preload based on scroll direction.
 *
 * Predicts next thumbnail needed based on scroll direction.
 * Returns -1 if preload would be out of bounds.
 *
 * @param current_index Currently selected entry index
 * @param last_index Previously selected entry index
 * @param total_count Total entries in directory
 * @return Index to preload, or -1 if none
 */
int ThumbPreload_getHintIndex(int current_index, int last_index, int total_count);

///////////////////////////////
// Fade Animation
///////////////////////////////

/** Default fade duration in milliseconds */
#define THUMB_FADE_DURATION_MS 100

/** Maximum alpha value (fully opaque) */
#define THUMB_ALPHA_MAX 255

/** Minimum alpha value (fade start) */
#define THUMB_ALPHA_MIN 0

/**
 * Fade animation state.
 *
 * Tracks fade-in progress for smooth thumbnail appearance.
 */
typedef struct {
	unsigned long start_ms; // Fade start time (0 = not fading)
	int alpha; // Current alpha (THUMB_ALPHA_MIN to THUMB_ALPHA_MAX)
	int duration_ms; // Fade duration
} ThumbFadeState;

/**
 * Initialize fade state.
 *
 * @param state State to initialize
 * @param duration_ms Fade duration in milliseconds
 */
void ThumbFade_init(ThumbFadeState* state, int duration_ms);

/**
 * Start a fade-in animation.
 *
 * @param state Fade state
 * @param now_ms Current time in milliseconds
 */
void ThumbFade_start(ThumbFadeState* state, unsigned long now_ms);

/**
 * Reset fade to fully opaque (no animation).
 *
 * @param state Fade state
 */
void ThumbFade_reset(ThumbFadeState* state);

/**
 * Update fade animation.
 *
 * Uses smoothstep easing: f(t) = t * t * (3 - 2 * t)
 *
 * @param state Fade state
 * @param now_ms Current time in milliseconds
 * @return 1 if animation is active (alpha changed), 0 if complete/inactive
 */
int ThumbFade_update(ThumbFadeState* state, unsigned long now_ms);

/**
 * Check if fade animation is active.
 *
 * @param state Fade state
 * @return 1 if fading, 0 if complete or not started
 */
int ThumbFade_isActive(const ThumbFadeState* state);

/**
 * Calculate smoothstep eased alpha for a given elapsed time.
 *
 * Pure function for testing. Uses smoothstep: f(t) = t * t * (3 - 2 * t)
 *
 * @param elapsed_ms Time elapsed since fade start
 * @param duration_ms Total fade duration
 * @param max_alpha Maximum alpha value
 * @return Eased alpha value (0 to max_alpha)
 */
int ThumbFade_calculateAlpha(unsigned long elapsed_ms, unsigned long duration_ms, int max_alpha);

#endif // MINUI_THUMBNAIL_H
