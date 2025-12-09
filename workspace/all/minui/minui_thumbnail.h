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
#define MINUI_THUMBNAIL_CACHE_SIZE 3

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
} MinUIThumbnailCacheSlot;

/**
 * FIFO thumbnail cache.
 *
 * Evicts oldest entry when full (slot 0 is oldest).
 * Entry index is the cache key (position in current directory).
 *
 * Tracks which item is "displayed" to prevent dangling pointer bugs:
 * - Never exposes raw surface pointers for storage
 * - Auto-invalidates displayed_valid when displayed item is evicted
 * - All access through getter functions that return fresh lookups
 */
typedef struct {
	MinUIThumbnailCacheSlot slots[MINUI_THUMBNAIL_CACHE_SIZE];
	int size; // Number of valid slots (0 to MINUI_THUMBNAIL_CACHE_SIZE)
	int displayed_index; // entry_index of currently displayed item, or -1
	int displayed_valid; // 1 if displayed item is in cache, 0 if evicted
} MinUIThumbnailCache;

/**
 * Initialize cache to empty state.
 *
 * @param cache Cache to initialize
 */
void MinUIThumbnail_cacheInit(MinUIThumbnailCache* cache);

/**
 * Find slot index by entry index.
 *
 * @param cache Cache to search
 * @param entry_index Entry index to find
 * @return Slot index (0 to size-1) if found, -1 if not found
 */
int MinUIThumbnail_cacheFind(const MinUIThumbnailCache* cache, int entry_index);

/**
 * Check if cache is full.
 *
 * @param cache Cache to check
 * @return 1 if full, 0 otherwise
 */
int MinUIThumbnail_cacheIsFull(const MinUIThumbnailCache* cache);

/**
 * Get slot that would be evicted if cache is full.
 * Always returns slot 0 (oldest) when full.
 *
 * @param cache Cache to check
 * @return Slot index to evict (0) if full, -1 if not full
 */
int MinUIThumbnail_cacheGetEvictSlot(const MinUIThumbnailCache* cache);

/**
 * Add item to cache.
 *
 * If cache is full, caller must evict first using MinUIThumbnail_cacheEvict().
 * This separation allows caller to free resources before eviction.
 *
 * @param cache Cache to modify
 * @param entry_index Entry index (cache key)
 * @param path Path to thumbnail file
 * @param data Opaque data pointer (SDL_Surface* in production)
 * @return 1 on success, 0 if cache is full
 */
int MinUIThumbnail_cacheAdd(MinUIThumbnailCache* cache, int entry_index, const char* path,
                            void* data);

/**
 * Evict oldest slot from cache.
 *
 * Shifts remaining items left. Caller should free the evicted
 * slot's data before calling this.
 *
 * @param cache Cache to modify
 * @return 1 on success, 0 if cache is empty
 */
int MinUIThumbnail_cacheEvict(MinUIThumbnailCache* cache);

/**
 * Get data pointer for a slot.
 *
 * @param cache Cache to query
 * @param slot Slot index (0 to size-1)
 * @return Data pointer, or NULL if slot is invalid
 */
void* MinUIThumbnail_cacheGetData(const MinUIThumbnailCache* cache, int slot);

/**
 * Get slot by index for clearing.
 *
 * @param cache Cache to query
 * @param slot Slot index (0 to size-1)
 * @return Pointer to slot, or NULL if invalid
 */
MinUIThumbnailCacheSlot* MinUIThumbnail_cacheGetSlot(MinUIThumbnailCache* cache, int slot);

/**
 * Clear cache to empty state.
 * Caller must free all slot data before calling.
 *
 * @param cache Cache to clear
 */
void MinUIThumbnail_cacheClear(MinUIThumbnailCache* cache);

///////////////////////////////
// Displayed Item Tracking
///////////////////////////////

/**
 * Mark an entry as currently displayed.
 * The cache tracks this so it can invalidate if evicted.
 *
 * @param cache Cache to modify
 * @param entry_index Entry index to mark as displayed
 */
void MinUIThumbnail_cacheSetDisplayed(MinUIThumbnailCache* cache, int entry_index);

/**
 * Clear the displayed item tracking.
 *
 * @param cache Cache to modify
 */
void MinUIThumbnail_cacheClearDisplayed(MinUIThumbnailCache* cache);

/**
 * Check if the displayed item is still valid (in cache).
 *
 * @param cache Cache to query
 * @return 1 if displayed item is valid, 0 if evicted or none set
 */
int MinUIThumbnail_cacheIsDisplayedValid(const MinUIThumbnailCache* cache);

/**
 * Get the entry index of the displayed item.
 *
 * @param cache Cache to query
 * @return Entry index of displayed item, or -1 if none
 */
int MinUIThumbnail_cacheGetDisplayedIndex(const MinUIThumbnailCache* cache);

/**
 * Get the data pointer for the currently displayed item.
 * Returns NULL if no item is displayed or it was evicted.
 * Never store the returned pointer - always re-lookup each frame.
 *
 * @param cache Cache to query
 * @return Data pointer for displayed item, or NULL
 */
void* MinUIThumbnail_cacheGetDisplayedData(const MinUIThumbnailCache* cache);

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
int MinUIThumbnail_preloadGetHintIndex(int current_index, int last_index, int total_count);

///////////////////////////////
// Fade Animation
///////////////////////////////

/** Default fade duration in milliseconds */
#define MINUI_THUMBNAIL_FADE_DURATION_MS 100

/** Maximum alpha value (fully opaque) */
#define MINUI_THUMBNAIL_ALPHA_MAX 255

/** Minimum alpha value (fade start) */
#define MINUI_THUMBNAIL_ALPHA_MIN 0

/**
 * Fade animation state.
 *
 * Tracks fade-in progress for smooth thumbnail appearance.
 */
typedef struct {
	unsigned long start_ms; // Fade start time (0 = not fading)
	int alpha; // Current alpha (MINUI_THUMBNAIL_ALPHA_MIN to MINUI_THUMBNAIL_ALPHA_MAX)
	int duration_ms; // Fade duration
} MinUIThumbnailFadeState;

/**
 * Initialize fade state.
 *
 * @param state State to initialize
 * @param duration_ms Fade duration in milliseconds
 */
void MinUIThumbnail_fadeInit(MinUIThumbnailFadeState* state, int duration_ms);

/**
 * Start a fade-in animation.
 *
 * @param state Fade state
 * @param now_ms Current time in milliseconds
 */
void MinUIThumbnail_fadeStart(MinUIThumbnailFadeState* state, unsigned long now_ms);

/**
 * Reset fade to fully opaque (no animation).
 *
 * @param state Fade state
 */
void MinUIThumbnail_fadeReset(MinUIThumbnailFadeState* state);

/**
 * Update fade animation.
 *
 * Uses smoothstep easing: f(t) = t * t * (3 - 2 * t)
 *
 * @param state Fade state
 * @param now_ms Current time in milliseconds
 * @return 1 if animation is active (alpha changed), 0 if complete/inactive
 */
int MinUIThumbnail_fadeUpdate(MinUIThumbnailFadeState* state, unsigned long now_ms);

/**
 * Check if fade animation is active.
 *
 * @param state Fade state
 * @return 1 if fading, 0 if complete or not started
 */
int MinUIThumbnail_fadeIsActive(const MinUIThumbnailFadeState* state);

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
int MinUIThumbnail_fadeCalculateAlpha(unsigned long elapsed_ms, unsigned long duration_ms,
                                      int max_alpha);

#endif // MINUI_THUMBNAIL_H
