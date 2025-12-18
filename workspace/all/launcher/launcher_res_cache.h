/**
 * launcher_res_cache.h - Thumbnail (.res) directory cache for Launcher
 *
 * Replaces per-entry exists() checks with lazy directory scanning and O(1) lookups.
 * Handles both regular directories and collections uniformly by caching per parent directory.
 *
 * Usage:
 *   ResCache_init();                              // Call once at startup
 *   int has = ResCache_hasThumbnail(entry->path); // O(1) lookup (lazy scan on first access)
 *   ResCache_free();                              // Call at shutdown
 *
 * Thread safety: All ResCache_* functions must be called from the main thread only.
 * The background thumbnail loader receives pre-validated paths and does not access the cache.
 */

#ifndef LAUNCHER_RES_CACHE_H
#define LAUNCHER_RES_CACHE_H

/**
 * Initializes the thumbnail cache.
 * Call once at launcher startup.
 */
void ResCache_init(void);

/**
 * Checks if a thumbnail exists for an entry.
 *
 * On first access to a directory, scans its .res/ subdirectory and caches
 * the filenames. Subsequent checks for entries in the same directory are O(1).
 *
 * @param entry_path Full path to entry (e.g., "/Roms/GB/Tetris.gb")
 * @return 1 if thumbnail exists at .res/Tetris.gb.png, 0 otherwise
 */
int ResCache_hasThumbnail(const char* entry_path);

/**
 * Builds the thumbnail path for an entry (for loading).
 *
 * Similar to Launcher_buildThumbPath but uses the cached directory info.
 * Only builds the path if the thumbnail exists (returns 0 otherwise).
 *
 * @param entry_path Full path to entry
 * @param out_path Output buffer for thumbnail path (min MAX_PATH bytes)
 * @return 1 if thumbnail exists and path was built, 0 otherwise
 */
int ResCache_getThumbPath(const char* entry_path, char* out_path);

/**
 * Invalidates cache for a specific directory.
 * Call if thumbnails are added/removed at runtime (rare).
 *
 * @param dir_path Directory path to invalidate
 */
void ResCache_invalidateDir(const char* dir_path);

/**
 * Frees all cached data.
 * Call at launcher shutdown.
 */
void ResCache_free(void);

/**
 * Returns number of directories currently cached.
 * Useful for debugging/stats.
 */
int ResCache_dirCount(void);

#endif // LAUNCHER_RES_CACHE_H
