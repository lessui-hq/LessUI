/**
 * launcher_res_cache.c - Thumbnail (.res) directory cache for Launcher
 *
 * Replaces per-entry exists() checks with lazy directory scanning and O(1) lookups.
 * For a directory with 500 ROMs, this eliminates up to 500 syscalls per browse session.
 *
 * Two-level cache structure:
 * - Level 1: directory path → filename set (e.g., "/Roms/GB" → {...})
 * - Level 2: filename set = stb_ds hashmap of filenames (e.g., {"Tetris.gb.png", ...})
 *
 * Collections are handled uniformly - each unique parent directory is scanned once
 * on first access, then all entries from that directory use O(1) lookups.
 */

#include "launcher_res_cache.h"
#include "defines.h"
#include "stb_ds.h"
#include "utils.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

// Filename set entry (just need key presence, value unused)
typedef struct {
	char* key;
	int value;
} ResFileEntry;

// Directory cache entry: maps dir_path → filename set
typedef struct {
	char* key; // Directory path (e.g., "/Roms/GB")
	ResFileEntry* value; // Filename set (NULL if .res doesn't exist or is empty)
} ResDirEntry;

// Global cache - lazily populated per directory
static ResDirEntry* res_cache = NULL;

void ResCache_init(void) {
	ResCache_free();
	sh_new_strdup(res_cache);
}

/**
 * Scans a .res directory and returns a filename set.
 * Returns NULL if directory doesn't exist or is empty.
 */
static ResFileEntry* scanResDirectory(const char* res_path) {
	DIR* dh = opendir(res_path);
	if (!dh) {
		return NULL;
	}

	ResFileEntry* file_set = NULL;
	sh_new_strdup(file_set);

	struct dirent* dp;
	while ((dp = readdir(dh)) != NULL) {
		// Skip hidden files
		if (dp->d_name[0] == '.') {
			continue;
		}
		// Only cache .png files (thumbnail format)
		if (!suffixMatch(".png", dp->d_name)) {
			continue;
		}
		shput(file_set, dp->d_name, 1);
	}

	closedir(dh);

	// If empty, free and return NULL
	if (shlen(file_set) == 0) {
		shfree(file_set);
		return NULL;
	}

	return file_set;
}

/**
 * Gets the cached filename set for a directory, scanning if needed.
 * Returns NULL if the directory has no .res folder or no thumbnails.
 */
static ResFileEntry* getOrScanResDir(const char* dir_path) {
	// Must be initialized before use
	if (!res_cache) {
		return NULL;
	}

	// Check if already cached
	ptrdiff_t idx = shgeti(res_cache, dir_path);
	if (idx >= 0) {
		return res_cache[idx].value; // May be NULL (cached "no thumbnails")
	}

	// Not cached - scan .res directory
	char res_path[MAX_PATH];
	(void)snprintf(res_path, MAX_PATH, "%s/.res", dir_path);

	ResFileEntry* file_set = scanResDirectory(res_path);

	// Cache result (NULL means no thumbnails in this directory)
	shput(res_cache, dir_path, file_set);

	return file_set;
}

/**
 * Extracts the parent directory and filename from an entry path.
 * Returns 0 on failure.
 */
static int extractPathParts(const char* entry_path, char* dir_path, const char** filename) {
	if (!entry_path) {
		return 0;
	}

	const char* last_slash = strrchr(entry_path, '/');
	if (!last_slash || last_slash[1] == '\0') {
		return 0;
	}

	int dir_len = (int)(last_slash - entry_path);
	if (dir_len <= 0 || dir_len >= MAX_PATH) {
		return 0;
	}

	// Copy directory path
	memcpy(dir_path, entry_path, (size_t)dir_len);
	dir_path[dir_len] = '\0';

	// Set filename pointer (past the slash)
	*filename = last_slash + 1;

	return 1;
}

int ResCache_hasThumbnail(const char* entry_path) {
	char dir_path[MAX_PATH];
	const char* filename = NULL;

	if (!extractPathParts(entry_path, dir_path, &filename)) {
		return 0;
	}

	// Get cached filename set (scanning if needed)
	ResFileEntry* file_set = getOrScanResDir(dir_path);
	if (!file_set) {
		return 0;
	}

	// Build expected thumbnail filename: "basename.png"
	char thumb_filename[MAX_PATH];
	(void)snprintf(thumb_filename, MAX_PATH, "%s.png", filename);

	return shgeti(file_set, thumb_filename) >= 0;
}

int ResCache_getThumbPath(const char* entry_path, char* out_path) {
	out_path[0] = '\0';

	char dir_path[MAX_PATH];
	const char* filename = NULL;

	if (!extractPathParts(entry_path, dir_path, &filename)) {
		return 0;
	}

	// Get cached filename set (scanning if needed)
	ResFileEntry* file_set = getOrScanResDir(dir_path);
	if (!file_set) {
		return 0;
	}

	// Build expected thumbnail filename: "basename.png"
	char thumb_filename[MAX_PATH];
	(void)snprintf(thumb_filename, MAX_PATH, "%s.png", filename);

	// Check if it exists in the set
	if (shgeti(file_set, thumb_filename) < 0) {
		return 0;
	}

	// Build full path
	(void)snprintf(out_path, MAX_PATH, "%s/.res/%s", dir_path, thumb_filename);
	return 1;
}

void ResCache_invalidateDir(const char* dir_path) {
	if (!res_cache || !dir_path) {
		return;
	}

	ptrdiff_t idx = shgeti(res_cache, dir_path);
	if (idx >= 0) {
		// Free the filename set if present
		if (res_cache[idx].value) {
			shfree(res_cache[idx].value);
		}
		// Remove from cache
		shdel(res_cache, dir_path);
	}
}

void ResCache_free(void) {
	if (!res_cache) {
		return;
	}

	// Free all filename sets
	for (ptrdiff_t i = 0; i < shlen(res_cache); i++) {
		if (res_cache[i].value) {
			shfree(res_cache[i].value);
		}
	}

	shfree(res_cache);
	res_cache = NULL;
}

int ResCache_dirCount(void) {
	if (!res_cache) {
		return 0;
	}
	return (int)shlen(res_cache);
}
