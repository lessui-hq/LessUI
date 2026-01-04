/**
 * launcher.c - Launcher launcher application
 *
 * The main launcher UI for Launcher, providing a simple file browser interface
 * for navigating ROMs, recently played games, collections, and tools.
 *
 * Architecture:
 * - File browser with directory stack navigation
 * - Recently played games tracking (up to 24 entries)
 * - ROM collections support via .txt files
 * - Multi-disc game support via .m3u playlists
 * - Display name aliasing via map.txt files
 * - Auto-resume support for returning to last played game
 * - Alphabetical indexing with L1/R1 shoulder button navigation
 *
 * Key Features:
 * - Platform-agnostic ROM paths (stored relative to SDCARD_PATH)
 * - Collating ROM folders (e.g., "GB (Game Boy)" and "GB (Game Boy Color)" appear as "GB")
 * - Thumbnail support from .res/ subdirectories
 * - Simple mode (hides Tools, disables sleep)
 * - HDMI hotplug detection and restart
 *
 * Data Structures:
 * - Array: Dynamic array for entries, directories, recents
 * - MapEntry: O(1) hash map for name aliasing (stb_ds)
 * - Directory: Represents a folder with entries and rendering state
 * - Entry: Represents a file/folder (ROM, PAK, or directory)
 * - Recent: Recently played game with path and optional alias
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <msettings.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "directory_index.h"
#include "launcher_context.h"
#include "launcher_directory.h"
#include "launcher_emu_cache.h"
#include "launcher_entry.h"
#include "launcher_file_utils.h"
#include "launcher_launcher.h"
#include "launcher_m3u.h"
#include "launcher_map.h"
#include "launcher_navigation.h"
#include "launcher_res_cache.h"
#include "launcher_state.h"
#include "launcher_str_compare.h"
#include "launcher_thumbnail.h"
#include "paths.h"
#include "platform_variant.h"
#include "recent_file.h"
#include "utils.h"

///////////////////////////////
// List View Configuration
//
// Tunable parameters for the list view rendering.
// All values are easily adjustable for tweaking the UI layout.
///////////////////////////////

// Thumbnail layout (percentages of screen width)
#define THUMB_TEXT_WIDTH_PERCENT 60 // Text area width when thumbnail shown (unselected items)
#define THUMB_SELECTED_WIDTH_PERCENT 100 // Selected item text width when thumbnail shown
#define THUMB_MAX_WIDTH_PERCENT 40 // Maximum thumbnail width

// Note: LAUNCHER_THUMBNAIL_* constants now in launcher_thumbnail.h

///////////////////////////////
// Async thumbnail loader
//
// Loads thumbnails in a background thread to prevent UI stutter during scrolling.
// Design: Single worker thread with request superseding (new requests cancel pending).
// Thread-safe handoff via mutex-protected result surface.
///////////////////////////////

// Thumbnail loader state
static pthread_t thumb_thread;
static int thumb_thread_valid; // Whether thread was successfully created
static pthread_mutex_t thumb_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thumb_cond = PTHREAD_COND_INITIALIZER;

// Request state (protected by thumb_mutex)
static char thumb_request_path[MAX_PATH]; // Path to load (empty = no request)
static int thumb_request_width; // Max width for scaling
static int thumb_request_height; // Max height for scaling
static int thumb_request_entry_index; // Entry index for cache tracking
static char thumb_preload_hint_path[MAX_PATH]; // Path to preload after current
static int thumb_preload_hint_index; // Entry index of preload hint
static int thumb_is_preload; // Whether current request is a preload
static int thumb_shutdown; // Signal thread to exit

// Result state (protected by thumb_mutex)
static SDL_Surface* thumb_result; // Loaded surface (NULL if no thumbnail)
static char thumb_result_path[MAX_PATH]; // Path that was loaded
static int thumb_result_entry_index; // Entry index of result

/**
 * Background thread function for loading thumbnails.
 * Waits for requests, loads and scales images, posts results.
 */
static void* thumb_loader_thread(void* arg) {
	(void)arg;
	LOG_debug("Thumbnail thread started");

	char path[MAX_PATH];
	int max_w, max_h, entry_index;
	int is_preload;
	while (1) {
		// Wait for a request
		pthread_mutex_lock(&thumb_mutex);
		while (thumb_request_path[0] == '\0' && !thumb_shutdown) {
			pthread_cond_wait(&thumb_cond, &thumb_mutex);
		}

		if (thumb_shutdown) {
			pthread_mutex_unlock(&thumb_mutex);
			break;
		}

		// Copy request parameters
		SAFE_STRCPY(path, thumb_request_path);
		max_w = thumb_request_width;
		max_h = thumb_request_height;
		entry_index = thumb_request_entry_index;
		is_preload = thumb_is_preload;
		thumb_request_path[0] = '\0'; // Clear request
		pthread_mutex_unlock(&thumb_mutex);

		// Load and scale (slow operations, done without lock)
		// Path already validated by ResCache before being queued
		SDL_Surface* loaded = NULL;
		SDL_Surface* orig = IMG_Load(path);
		if (orig) {
			loaded = GFX_scaleToFit(orig, max_w, max_h);
			if (loaded != orig)
				SDL_FreeSurface(orig);
		}

		// Post result
		pthread_mutex_lock(&thumb_mutex);
		// Check if request was superseded while we were loading
		// Accept result if: queue is empty OR same path was re-requested (fast scrolling case)
		if (thumb_request_path[0] == '\0' || exactMatch(thumb_request_path, path)) {
			// No new request or same path re-requested - post our result
			if (thumb_result)
				SDL_FreeSurface(thumb_result);
			thumb_result = loaded;
			SAFE_STRCPY(thumb_result_path, path);
			thumb_result_entry_index = entry_index;
			LOG_debug("thumb: loaded idx=%d%s", entry_index, is_preload ? " (preload)" : "");

			// Clear the request if it matched (avoid re-processing same request)
			if (thumb_request_path[0] != '\0') {
				thumb_request_path[0] = '\0';
			}

			// If this was a current (not preload) request and we have a hint, queue it
			if (!is_preload && thumb_preload_hint_path[0] != '\0') {
				// Small delay to avoid starving main thread
				pthread_mutex_unlock(&thumb_mutex);
				usleep(5000); // 5ms
				pthread_mutex_lock(&thumb_mutex);

				// Only queue preload if no new request came in
				if (thumb_request_path[0] == '\0') {
					SAFE_STRCPY(thumb_request_path, thumb_preload_hint_path);
					thumb_request_entry_index = thumb_preload_hint_index;
					thumb_is_preload = 1;
					thumb_preload_hint_path[0] = '\0'; // Clear hint
					LOG_debug("thumb: preloading idx=%d", thumb_preload_hint_index);
				}
			}
		} else {
			// Request was superseded by different path - discard our result
			LOG_debug("thumb: idx=%d discarded (superseded)", entry_index);
			if (loaded)
				SDL_FreeSurface(loaded);
		}
		pthread_mutex_unlock(&thumb_mutex);
	}

	return NULL;
}

/**
 * Starts the thumbnail loader thread.
 * Call once at startup.
 */
static void LauncherThumbnail_loaderInit(void) {
	thumb_request_path[0] = '\0';
	thumb_result_path[0] = '\0';
	thumb_preload_hint_path[0] = '\0';
	thumb_result = NULL;
	thumb_request_entry_index = -1;
	thumb_result_entry_index = -1;
	thumb_preload_hint_index = -1;
	thumb_is_preload = 0;
	thumb_shutdown = 0;
	thumb_thread_valid = 0;
	int rc = pthread_create(&thumb_thread, NULL, thumb_loader_thread, NULL);
	if (rc != 0) {
		LOG_error("Failed to create thumbnail thread: %d", rc);
	} else {
		thumb_thread_valid = 1;
	}
}

/**
 * Stops the thumbnail loader thread and frees resources.
 * Call once at shutdown.
 */
static void LauncherThumbnail_loaderQuit(void) {
	if (thumb_thread_valid) {
		pthread_mutex_lock(&thumb_mutex);
		thumb_shutdown = 1;
		pthread_cond_signal(&thumb_cond);
		pthread_mutex_unlock(&thumb_mutex);

		pthread_join(thumb_thread, NULL);
	}

	if (thumb_result) {
		SDL_FreeSurface(thumb_result);
		thumb_result = NULL;
	}
}

/**
 * Requests a thumbnail to be loaded asynchronously.
 * Supersedes any pending request. Returns immediately.
 *
 * @param path Path to image file
 * @param max_w Maximum width after scaling
 * @param max_h Maximum height after scaling
 * @param entry_index Index of entry in directory (for cache tracking)
 * @param is_preload Whether this is a preload (superseded by non-preload requests)
 * @param hint_path Optional path to preload after this one (can be NULL)
 * @param hint_index Entry index of hint path
 */
static void LauncherThumbnail_loaderRequest(const char* path, int max_w, int max_h, int entry_index,
                                            int is_preload, const char* hint_path, int hint_index) {
	pthread_mutex_lock(&thumb_mutex);

	// Current (non-preload) requests always supersede preload requests
	if (!is_preload || !thumb_is_preload) {
		SAFE_STRCPY(thumb_request_path, path);
		thumb_request_width = max_w;
		thumb_request_height = max_h;
		thumb_request_entry_index = entry_index;
		thumb_is_preload = is_preload;

		// Set preload hint if provided and this is not already a preload
		if (!is_preload && hint_path && hint_path[0] != '\0') {
			SAFE_STRCPY(thumb_preload_hint_path, hint_path);
			thumb_preload_hint_index = hint_index;
		} else {
			thumb_preload_hint_path[0] = '\0';
			thumb_preload_hint_index = -1;
		}

		pthread_cond_signal(&thumb_cond);
	}

	pthread_mutex_unlock(&thumb_mutex);
}

/**
 * Checks if a thumbnail is ready and retrieves it.
 * Non-blocking - returns NULL if not ready.
 * Returns ANY completed result (current or preload) so caller can cache it.
 *
 * @param out_entry_index Output parameter for entry index (can be NULL)
 * @param out_path Output buffer for result path (must be MAX_PATH, can be NULL)
 * @return Surface if ready (caller takes ownership), NULL otherwise
 */
static SDL_Surface* LauncherThumbnail_loaderGet(int* out_entry_index, char* out_path) {
	SDL_Surface* result = NULL;

	pthread_mutex_lock(&thumb_mutex);
	if (thumb_result) {
		result = thumb_result;
		if (out_entry_index)
			*out_entry_index = thumb_result_entry_index;
		if (out_path)
			safe_strcpy(out_path, thumb_result_path, MAX_PATH);
		thumb_result = NULL;
		thumb_result_path[0] = '\0';
		thumb_result_entry_index = -1;
	}
	pthread_mutex_unlock(&thumb_mutex);

	return result;
}

///////////////////////////////
// Thumbnail cache SDL wrappers
//
// Cache logic moved to launcher_thumbnail.c for testability.
// These wrappers handle SDL_Surface allocation/freeing.
// Tracks displayed item to prevent dangling pointer bugs.
///////////////////////////////

/**
 * Add surface to cache, freeing evicted surface if necessary.
 */
static void thumb_cache_push(LauncherThumbnailCache* cache, SDL_Surface* surface, const char* path,
                             int entry_index) {
	// If cache is full, free the evicted surface first
	if (LauncherThumbnail_cacheIsFull(cache)) {
		SDL_Surface* evicted = (SDL_Surface*)LauncherThumbnail_cacheGetData(cache, 0);
		if (evicted)
			SDL_FreeSurface(evicted);
		LauncherThumbnail_cacheEvict(cache);
	}
	LauncherThumbnail_cacheAdd(cache, entry_index, path, surface);
}

/**
 * Clear cache and free all surfaces.
 */
static void thumb_cache_clear(LauncherThumbnailCache* cache) {
	for (int i = 0; i < cache->size; i++) {
		SDL_Surface* surface = (SDL_Surface*)LauncherThumbnail_cacheGetData(cache, i);
		if (surface)
			SDL_FreeSurface(surface);
	}
	LauncherThumbnail_cacheClear(cache);
}

// build_thumb_path moved to launcher_file_utils.c as Launcher_buildThumbPath

///////////////////////////////
// Note: Entry, IntArray and related functions moved to
// workspace/all/common/launcher_entry.c for testability.
///////////////////////////////

///////////////////////////////
// Note: Directory structure moved to launcher_directory.h for type safety.
// Directory_new and Directory_index stay here (depend on globals).
// Directory_free, DirectoryArray_* moved to launcher_directory.c (pure functions).
///////////////////////////////

/**
 * Indexes a directory's entries and applies name aliasing.
 *
 * This function performs several important tasks:
 * 1. Loads map.txt (if present) to alias display names
 * 2. Filters out entries marked as hidden via map.txt
 * 3. Re-sorts entries if any names were aliased
 * 4. Detects duplicate display names and generates unique names
 * 5. Builds alphabetical index for L1/R1 navigation
 *
 * Map.txt format: Each line is "filename<TAB>display name"
 * - If display name starts with '.', the entry is hidden
 * - Collections use a shared map.txt in g_collections_path
 *
 * Duplicate handling:
 * - If two entries have the same display name but different filenames,
 *   shows the filename to disambiguate
 * - If filenames are also identical (cross-platform ROMs), appends
 *   the emulator name in parentheses
 *
 * @param self Directory to index (modified in place)
 */
void Directory_index(Directory* self) {
	int is_collection = prefixMatch(g_collections_path, self->path);
	int skip_index =
	    exactMatch(g_faux_recent_path, self->path) || is_collection; // not alphabetized

	// Load maps for name aliasing (pak-bundled + user overrides)
	// For collections, just load collection map.txt directly
	MapEntry* map;
	if (is_collection) {
		char map_path[256];
		(void)snprintf(map_path, sizeof(map_path), "%s/map.txt", g_collections_path);
		map = Map_load(map_path);
	} else {
		// Load merged pak + user maps for ROM directories
		map = Map_loadForDirectory(self->path);
	}

	// Use DirectoryIndex module for aliasing, filtering, duplicate detection, and alpha index
	Entry** indexed = DirectoryIndex_index(self->entries, self->alphas, map, skip_index);
	if (indexed != self->entries) {
		// Entries were filtered - update our reference
		// Don't use EntryArray_free on old array since entries were moved, not copied
		arrfree(self->entries);
		self->entries = indexed;
	}

	Map_free(map);
}

// Forward declarations for directory entry getters
static Entry** getRoot(void);
static Entry** getRecents(void);
static Entry** getCollection(char* path);
static Entry** getDiscs(char* path);
static Entry** getEntries(char* path);

/**
 * Creates a new directory from a path.
 *
 * Automatically determines which type of directory this is and
 * populates its entries accordingly:
 * - Root (SDCARD_PATH): Shows systems, recents, collections, tools
 * - Recently played (g_faux_recent_path): Shows recent games
 * - Collection (.txt file): Loads games from text file
 * - Multi-disc (.m3u file): Shows disc list
 * - Regular directory: Shows files and subdirectories
 *
 * @param path Full path to directory
 * @param selected Initial selected index
 * @return Pointer to allocated Directory
 *
 * @warning Caller must free with Directory_free()
 */
Directory* Directory_new(char* path, int selected) {
	char display_name[256];
	getDisplayName(path, display_name);

	Directory* self = malloc(sizeof(Directory));
	if (!self)
		return NULL;
	self->path = strdup(path);
	if (!self->path) {
		free(self);
		return NULL;
	}
	self->name = strdup(display_name);
	if (!self->name) {
		free(self->path);
		free(self);
		return NULL;
	}
	if (exactMatch(path, g_sdcard_path)) {
		self->entries = getRoot();
	} else if (exactMatch(path, g_faux_recent_path)) {
		self->entries = getRecents();
	} else if (!exactMatch(path, g_collections_path) && prefixMatch(g_collections_path, path) &&
	           suffixMatch(".txt", path)) {
		self->entries = getCollection(path);
	} else if (suffixMatch(".m3u", path)) {
		self->entries = getDiscs(path);
	} else {
		self->entries = getEntries(path);
	}
	self->alphas = IntArray_new();
	if (!self->alphas) {
		EntryArray_free(self->entries);
		free(self->name);
		free(self->path);
		free(self);
		return NULL;
	}
	self->selected = selected;
	Directory_index(self);
	return self;
}

// Directory_free, DirectoryArray_pop, DirectoryArray_free moved to launcher_directory.c

///////////////////////////////
// Note: Recent structure moved to recent_file.h for type safety.
// Recent_new now in recent_file.c (takes hasEmu callback for testability).
// RecentArray functions moved to recent_file.c (pure array operations).
///////////////////////////////

// Global used to pass alias when opening ROM from recents/collections
// This is a workaround to avoid changing function signatures
static char* recent_alias = NULL;

static int hasEmu(char* emu_name);

// Recent_new, Recent_free, RecentArray_* moved to recent_file.c
// Local wrappers for convenience:

static Recent* Recent_new_local(char* path, char* alias) {
	return Recent_new(path, alias, g_sdcard_path, hasEmu);
}

///////////////////////////////
// Global state
///////////////////////////////

static Directory* top; // Current directory being viewed
static Directory** stack; // Stack of open directories (stb_ds dynamic array)
static Recent** recents; // Recently played games list (stb_ds dynamic array)

static int quit = 0; // Set to 1 to exit main loop
static int can_resume = 0; // 1 if selected ROM has a save state
static int should_resume = 0; // Set to 1 when X button pressed to resume
static int simple_mode = 0; // 1 if simple mode enabled (hides Tools, disables sleep)
static char slot_path[MAX_PATH]; // Path to save state slot file for can_resume check

// State restoration variables for preserving selection when navigating
// Restore state (for state restoration after launching a game)
static LauncherRestoreState g_restore_state = {
    .depth = -1, .relative = -1, .selected = 0, .start = 0, .end = 0};
// Legacy aliases for gradual migration
#define restore_depth g_restore_state.depth
#define restore_relative g_restore_state.relative
#define restore_selected g_restore_state.selected
#define restore_start g_restore_state.start
#define restore_end g_restore_state.end

///////////////////////////////
// Recents management
///////////////////////////////

#define MAX_RECENTS 24 // A multiple of all menu row counts (4, 6, 8, 12)

/**
 * Saves the recently played list to disk.
 *
 * Format: One entry per line, "path\talias\n" or just "path\n"
 * Paths are relative to SDCARD_PATH for platform portability.
 */
static void saveRecents(void) {
	FILE* file = fopen(g_recent_path, "w");
	if (!file) {
		LOG_errno("Failed to save recent games to %s", g_recent_path);
		return;
	}

	int count = (int)arrlen(recents);
	for (int i = 0; i < count; i++) {
		Recent* recent = recents[i];
		(void)fputs(recent->path, file);
		if (recent->alias) {
			(void)fputs("\t", file);
			(void)fputs(recent->alias, file);
		}
		(void)putc('\n', file);
	}
	(void)fclose(file); // Read-only file
	LOG_info("Saved %d recent games", count);
}

/**
 * Adds a ROM to the recently played list.
 *
 * If the ROM is already in the list, it's moved to the top.
 * If the list is full, the oldest entry is removed.
 *
 * @param path Full ROM path (will be made relative to SDCARD_PATH)
 * @param alias Optional custom display name, or NULL
 */
static void addRecent(char* path, char* alias) {
	path += strlen(g_sdcard_path); // makes paths platform agnostic
	int id = RecentArray_indexOf(recents, path);
	if (id == -1) { // add new entry
		while ((int)arrlen(recents) >= MAX_RECENTS) {
			Recent_free(arrpop(recents));
		}
		Recent* new_recent = Recent_new_local(path, alias);
		if (new_recent)
			arrins(recents, 0, new_recent);
	} else if (id > 0) { // bump existing entry to top
		for (int i = id; i > 0; i--) {
			Recent* tmp = recents[i - 1];
			recents[i - 1] = recents[i];
			recents[i] = tmp;
		}
	}
	// If id == 0, already at top, no action needed
	saveRecents();
}

///////////////////////////////
// ROM/emulator detection
///////////////////////////////

/**
 * Checks if an emulator is installed.
 *
 * Uses cached lookup (O(1)) instead of filesystem checks.
 * Cache is initialized at startup by EmuCache_init().
 */
static int hasEmu(char* emu_name) {
	return EmuCache_hasEmu(emu_name);
}

/**
 * Checks if a directory contains a .cue file for multi-disc games.
 * Wrapper around Launcher_hasCue.
 */
static int hasCue(char* dir_path, char* cue_path) {
	return Launcher_hasCue(dir_path, cue_path);
}

/**
 * Checks if a ROM has an associated .m3u playlist for multi-disc games.
 * Wrapper around Launcher_hasM3u.
 */
static int hasM3u(char* rom_path, char* m3u_path) {
	return Launcher_hasM3u(rom_path, m3u_path);
}

/**
 * Loads recently played games from disk.
 *
 * This function performs several important tasks:
 * 1. Handles disc change requests (from in-game disc swapping)
 * 2. Loads recent games from g_recent_path file
 * 3. Filters out games whose emulators no longer exist
 * 4. Deduplicates multi-disc games (shows only most recent disc)
 * 5. Populates the global recents array
 *
 * Multi-disc handling:
 * - If a game has an .m3u file, only the most recently played disc
 *   from that game is shown in recents
 * - This prevents the recents list from being flooded with discs
 *   from the same game
 *
 * @return 1 if any playable recents exist, 0 otherwise
 */
static int hasRecents(void) {
	LOG_debug("hasRecents %s", g_recent_path);
	int has = 0;

	// Track parent directories to avoid duplicate multi-disc entries
	char** parent_paths = NULL;
	if (exists(CHANGE_DISC_PATH)) {
		char sd_path[256];
		getFile(CHANGE_DISC_PATH, sd_path, 256);
		if (exists(sd_path)) {
			char* disc_path = sd_path + strlen(g_sdcard_path); // makes path platform agnostic
			Recent* recent = Recent_new_local(disc_path, NULL);
			if (recent) {
				if (recent->available)
					has += 1;
				arrpush(recents, recent);

				char parent_path[256];
				safe_strcpy(parent_path, disc_path, sizeof(parent_path));
				char* tmp = strrchr(parent_path, '/') + 1;
				tmp[0] = '\0';
				char* parent_copy = strdup(parent_path);
				if (parent_copy)
					arrpush(parent_paths, parent_copy);
			}
		}
		unlink(CHANGE_DISC_PATH);
	}

	FILE* file = fopen(g_recent_path, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// LOG_info("line: %s", line);

			char* path = line;
			char* alias = NULL;
			char* tmp = strchr(line, '\t');
			if (tmp) {
				tmp[0] = '\0';
				alias = tmp + 1;
			}

			char sd_path[MAX_PATH];
			(void)snprintf(sd_path, sizeof(sd_path), "%s%s", g_sdcard_path, path);
			if (exists(sd_path)) {
				if ((int)arrlen(recents) < MAX_RECENTS) {
					// this logic replaces an existing disc from a multi-disc game with the last used
					char m3u_path[MAX_PATH];
					if (hasM3u(sd_path, m3u_path)) { // TODO: this might tank launch speed
						char parent_path[MAX_PATH];
						SAFE_STRCPY(parent_path, path);
						char* sep = strrchr(parent_path, '/') + 1;
						sep[0] = '\0';

						int found = 0;
						int parent_count = (int)arrlen(parent_paths);
						for (int i = 0; i < parent_count; i++) {
							char* item_path = parent_paths[i];
							if (prefixMatch(item_path, parent_path)) {
								found = 1;
								break;
							}
						}
						if (found)
							continue;

						char* parent_copy = strdup(parent_path);
						if (parent_copy)
							arrpush(parent_paths, parent_copy);
					}

					// LOG_info("path:%s alias:%s", path, alias);

					Recent* recent = Recent_new_local(path, alias);
					if (recent) {
						if (recent->available)
							has += 1;
						arrpush(recents, recent);
					}
				}
			}
		}
		(void)fclose(file); // Read-only file
	}

	saveRecents();

	// Free parent_paths (string array)
	int parent_count = (int)arrlen(parent_paths);
	for (int i = 0; i < parent_count; i++) {
		free(parent_paths[i]);
	}
	arrfree(parent_paths);
	return has > 0;
}

/**
 * Checks if any ROM collections exist.
 * Wrapper around Launcher_hasNonHiddenFiles.
 */
static int hasCollections(void) {
	return Launcher_hasNonHiddenFiles(g_collections_path);
}

/**
 * Checks if a ROM system directory has any playable ROMs.
 * Wrapper around LauncherDir_hasRoms with platform-specific paths.
 */
static int hasRoms(char* dir_name) {
	return LauncherDir_hasRoms(dir_name, g_roms_path, g_paks_path, g_sdcard_path, PLATFORM);
}

///////////////////////////////
// Directory entry generation
///////////////////////////////

/**
 * Generates the root directory entry list.
 *
 * Root shows:
 * 1. Recently Played (if any recent games exist)
 * 2. ROM systems (folders in Roms/ with available emulators)
 *    - Deduplicates systems with the same display name (collating)
 *    - Applies aliases from Roms/map.txt
 * 3. Collections (if any exist)
 *    - Either as a "Collections" folder or promoted to root if no systems
 * 4. Tools (platform-specific, hidden in simple mode)
 *
 * @return Array of Entry pointers for root directory
 */
static Entry** getRoot(void) {
	Entry** root = NULL;

	LOG_debug("getRoot: g_roms_path=%s", g_roms_path);
	LOG_debug("getRoot: g_paks_path=%s", g_paks_path);
	LOG_debug("getRoot: PLATFORM=%s", PLATFORM);

	if (hasRecents())
		arrpush(root, Entry_new(g_faux_recent_path, ENTRY_DIR));

	Entry** entries = NULL;
	DIR* dh = opendir(g_roms_path);
	if (dh != NULL) {
		LOG_debug("getRoot: Opened g_roms_path successfully");
		struct dirent* dp;
		char* tmp;
		char full_path[256];
		(void)snprintf(full_path, sizeof(full_path), "%s/", g_roms_path);
		tmp = full_path + strlen(full_path);
		Entry** emus = NULL;
		int dir_count = 0;
		int has_roms_count = 0;
		int total_entries = 0;
		while ((dp = readdir(dh)) != NULL) {
			total_entries++;
			LOG_debug("getRoot: readdir entry='%s' d_type=%d", dp->d_name, dp->d_type);
			if (hide(dp->d_name))
				continue;
			dir_count++;
			int has = hasRoms(dp->d_name);
			LOG_debug("getRoot: dir='%s' hasRoms=%d", dp->d_name, has);
			if (has) {
				has_roms_count++;
				safe_strcpy(tmp, dp->d_name, sizeof(full_path) - (tmp - full_path));
				arrpush(emus, Entry_new(full_path, ENTRY_DIR));
			}
		}
		LOG_debug("getRoot: readdir returned %d entries, %d visible dirs, %d have ROMs",
		          total_entries, dir_count, has_roms_count);
		EntryArray_sort(emus);
		Entry* prev_entry = NULL;
		int emus_count = (int)arrlen(emus);
		for (int i = 0; i < emus_count; i++) {
			Entry* entry = emus[i];
			if (prev_entry != NULL) {
				if (exactMatch(prev_entry->name, entry->name)) {
					Entry_free(entry);
					continue;
				}
			}
			arrpush(entries, entry);
			prev_entry = entry;
		}
		arrfree(emus); // just free the array part, entries now owns emus entries
		closedir(dh);
	} else {
		LOG_error("getRoot: Failed to open g_roms_path '%s'", g_roms_path);
	}

	LOG_debug("getRoot: Found %d system entries", (int)arrlen(entries));

	// Apply aliases from Roms/map.txt (we don't support hidden remaps here)
	char map_path[256];
	(void)snprintf(map_path, sizeof(map_path), "%s/map.txt", g_roms_path);
	if (arrlen(entries) > 0) {
		MapEntry* map = Map_load(map_path);
		if (map) {
			int resort = 0;
			int entries_count = (int)arrlen(entries);
			for (int i = 0; i < entries_count; i++) {
				Entry* entry = entries[i];
				char* filename = strrchr(entry->path, '/') + 1;
				char* alias = shget(map, filename);
				if (alias) {
					if (Entry_setName(entry, alias))
						resort = 1;
				}
			}
			if (resort)
				EntryArray_sort(entries);
			Map_free(map);
		}
	}

	if (hasCollections()) {
		if (arrlen(entries))
			arrpush(root, Entry_new(g_collections_path, ENTRY_DIR));
		else { // no visible systems, promote collections to root
			dh = opendir(g_collections_path);
			if (dh != NULL) {
				struct dirent* dp;
				char* tmp;
				char full_path[256];
				(void)snprintf(full_path, sizeof(full_path), "%s/", g_collections_path);
				tmp = full_path + strlen(full_path);
				Entry** collections = NULL;
				while ((dp = readdir(dh)) != NULL) {
					if (hide(dp->d_name))
						continue;
					safe_strcpy(tmp, dp->d_name, sizeof(full_path) - (tmp - full_path));
					arrpush(
					    collections,
					    Entry_new(full_path, ENTRY_DIR)); // yes, collections are fake directories
				}
				EntryArray_sort(collections);
				int collections_count = (int)arrlen(collections);
				for (int i = 0; i < collections_count; i++) {
					arrpush(entries, collections[i]);
				}
				arrfree(
				    collections); // just free the array part, entries now owns collections entries
				closedir(dh);
			}
		}
	}

	// add systems to root
	int entries_count = (int)arrlen(entries);
	for (int i = 0; i < entries_count; i++) {
		arrpush(root, entries[i]);
	}
	arrfree(entries); // root now owns entries' entries

	char tools_path[MAX_PATH];
	(void)snprintf(tools_path, sizeof(tools_path), "%s/Tools/%s", g_sdcard_path, PLATFORM);
	if (exists(tools_path) && !simple_mode)
		arrpush(root, Entry_new(tools_path, ENTRY_DIR));

	return root;
}

/**
 * Generates the Recently Played directory entry list.
 *
 * Filters out games whose emulators no longer exist.
 * Applies custom aliases if present.
 *
 * @return Array of Entry pointers for recently played games
 */
static Entry** getRecents(void) {
	Entry** entries = NULL;
	int count = (int)arrlen(recents);
	for (int i = 0; i < count; i++) {
		Recent* recent = recents[i];
		if (!recent->available)
			continue;

		char sd_path[256];
		(void)snprintf(sd_path, sizeof(sd_path), "%s%s", g_sdcard_path, recent->path);
		int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM;
		Entry* entry = Entry_new(sd_path, type);
		if (!entry)
			continue;
		if (recent->alias) {
			Entry_setName(entry, recent->alias);
		}
		arrpush(entries, entry);
	}
	return entries;
}

/**
 * Generates entry list from a collection text file.
 *
 * Collection format: One ROM path per line (relative to SDCARD_PATH)
 * Example: /Roms/GB/Tetris.gb
 *
 * Only includes ROMs that currently exist on the SD card.
 *
 * @param path Full path to collection .txt file
 * @return Array of Entry pointers for collection items
 */
static Entry** getCollection(char* path) {
	Entry** entries = NULL;
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			char sd_path[MAX_PATH];
			(void)snprintf(sd_path, sizeof(sd_path), "%s%s", g_sdcard_path, line);
			if (exists(sd_path)) {
				int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM;
				arrpush(entries, Entry_new(sd_path, type));
			}
		}
		(void)fclose(file); // Read-only file
	}
	return entries;
}

/**
 * Generates disc list from an .m3u playlist file.
 *
 * M3U format: One disc file per line (relative to .m3u file location)
 * Example: disc1.bin
 *
 * Entries are named "Disc 1", "Disc 2", etc.
 *
 * @param path Full path to .m3u file
 * @return Array of Entry pointers for each disc
 */
static Entry** getDiscs(char* path) {
	Entry** entries = NULL;

	int disc_count = 0;
	M3U_Disc** discs = M3U_getAllDiscs(path, &disc_count);
	if (discs) {
		for (int i = 0; i < disc_count; i++) {
			Entry* entry = Entry_new(discs[i]->path, ENTRY_ROM);
			if (!entry)
				continue;
			if (!Entry_setName(entry, discs[i]->name)) {
				Entry_free(entry);
				continue;
			}
			arrpush(entries, entry);
		}
		M3U_freeDiscs(discs, disc_count);
	}
	return entries;
}

// getFirstDisc is now provided by launcher_m3u.c as M3U_M3U_getFirstDisc()

static void addEntries(Entry*** entries, char* path) {
	DIR* dh = opendir(path);
	if (dh != NULL) {
		struct dirent* dp;
		char* tmp;
		char full_path[256];
		(void)snprintf(full_path, sizeof(full_path), "%s/", path);
		tmp = full_path + strlen(full_path);
		while ((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name))
				continue;
			safe_strcpy(tmp, dp->d_name, sizeof(full_path) - (tmp - full_path));
			int is_dir = dp->d_type == DT_DIR;
			int type = LauncherDir_determineEntryType(dp->d_name, is_dir, path, g_collections_path);
			arrpush(*entries, Entry_new(full_path, type));
		}
		closedir(dh);
	}
}

/**
 * Checks if a path is a top-level console directory.
 * Wrapper around LauncherDir_isConsoleDir.
 */
static int isConsoleDir(char* path) {
	return LauncherDir_isConsoleDir(path, g_roms_path);
}

static Entry** getEntries(char* path) {
	Entry** entries = NULL;

	if (isConsoleDir(path)) { // top-level console folder, might collate
		char collation_prefix[LAUNCHER_DIR_MAX_PATH];
		if (LauncherDir_buildCollationPrefix(path, collation_prefix)) {
			// Collated console directory (e.g., "Game Boy (USA)" matches "Game Boy (Japan)")
			DIR* dh = opendir(g_roms_path);
			if (dh != NULL) {
				struct dirent* dp;
				char full_path[LAUNCHER_DIR_MAX_PATH];
				while ((dp = readdir(dh)) != NULL) {
					if (hide(dp->d_name))
						continue;
					if (dp->d_type != DT_DIR)
						continue;
					(void)snprintf(full_path, sizeof(full_path), "%s/%s", g_roms_path, dp->d_name);

					if (!LauncherDir_matchesCollation(full_path, collation_prefix))
						continue;
					addEntries(&entries, full_path);
				}
				closedir(dh);
			}
		} else {
			// Non-collated console directory (no region suffix)
			addEntries(&entries, path);
		}
	} else {
		addEntries(&entries, path); // just a subfolder
	}

	EntryArray_sort(entries);
	return entries;
}

///////////////////////////////////////

///////////////////////////////
// Command execution
///////////////////////////////

/**
 * Queues a command to run after launcher exits.
 *
 * Writes the command to /tmp/next and sets quit flag.
 * The system's init script watches for this file and executes it.
 *
 * @param cmd Shell command to execute (must be properly quoted)
 */
static void queueNext(char* cmd) {
	LOG_info("cmd: %s", cmd);
	putFile("/tmp/next", cmd);
	quit = 1;
}

///////////////////////////////
// Resume state checking
///////////////////////////////

/**
 * Checks if a ROM has a save state and prepares resume state.
 *
 * Sets global can_resume flag and slot_path if a state exists.
 * Handles multi-disc games by checking for .m3u files.
 *
 * Save state path format:
 * /.userdata/.launcher/<emu>/<romname>.ext.txt
 *
 * @param rom_path Full ROM path
 * @param type ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
 */
static void readyResumePath(char* rom_path, int type) {
	char* tmp;
	can_resume = 0;
	char path[MAX_PATH];
	SAFE_STRCPY(path, rom_path);

	if (!prefixMatch(g_roms_path, path))
		return;

	char auto_path[MAX_PATH];
	if (type == ENTRY_DIR) {
		if (!hasCue(path, auto_path)) { // no cue?
			tmp = strrchr(auto_path, '.') + 1; // extension
			safe_strcpy(tmp, "m3u", sizeof(auto_path) - (tmp - auto_path)); // replace with m3u
			if (!exists(auto_path))
				return; // no m3u
		}
		SAFE_STRCPY(path, auto_path); // cue or m3u if one exists
	}

	if (!suffixMatch(".m3u", path)) {
		char m3u_path[MAX_PATH];
		if (hasM3u(path, m3u_path)) {
			// change path to m3u path
			SAFE_STRCPY(path, m3u_path);
		}
	}

	char emu_name[MAX_PATH];
	getEmuName(path, emu_name);

	char rom_file[MAX_PATH];
	tmp = strrchr(path, '/') + 1;
	SAFE_STRCPY(rom_file, tmp);

	(void)snprintf(slot_path, sizeof(slot_path), "%s/.launcher/%s/%s.txt", g_shared_userdata_path,
	               emu_name,
	               rom_file); // /.userdata/.launcher/<EMU>/<romname>.ext.txt

	can_resume = exists(slot_path);
}
static void readyResume(Entry* entry) {
	readyResumePath(entry->path, entry->type);
}

static void saveLast(char* path);
static void loadLast(void);

static int autoResume(void) {
	// NOTE: bypasses recents

	if (!exists(g_auto_resume_path))
		return 0;

	char path[MAX_PATH];
	getFile(g_auto_resume_path, path, MAX_PATH);
	unlink(g_auto_resume_path);
	sync();

	// make sure rom still exists
	char sd_path[MAX_PATH];
	(void)snprintf(sd_path, sizeof(sd_path), "%s%s", g_sdcard_path, path);
	if (!exists(sd_path))
		return 0;

	// make sure emu still exists
	char emu_name[MAX_PATH];
	getEmuName(sd_path, emu_name);

	char emu_path[MAX_PATH];
	getEmuPath(emu_name, emu_path);

	if (!exists(emu_path))
		return 0;

	// putFile(LAST_PATH, g_faux_recent_path); // saveLast() will crash here because top is NULL

	char cmd[MAX_PATH * 2];
	(void)snprintf(cmd, sizeof(cmd), "'%s' '%s'", Launcher_escapeSingleQuotes(emu_path),
	               Launcher_escapeSingleQuotes(sd_path));
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	queueNext(cmd);
	return 1;
}

///////////////////////////////
// Entry opening (launching ROMs/apps)
///////////////////////////////

/**
 * Launches a .pak application (context-aware version).
 *
 * .pak folders are applications (tools, emulators) with a launch.sh script.
 * Saves to recents if in Roms path. Saves current path for state restoration.
 *
 * @param ctx Launcher context
 * @param path Full path to .pak directory
 */
static void openPak_ctx(LauncherContext* ctx, char* path) {
	// Save path before escaping (escapeSingleQuotes modifies string)
	if (prefixMatch(g_roms_path, path)) {
		if (ctx->callbacks && ctx->callbacks->add_recent) {
			ctx->callbacks->add_recent(path, NULL);
		}
	}
	if (ctx->callbacks && ctx->callbacks->save_last) {
		ctx->callbacks->save_last(path);
	}

	char cmd[256];
	(void)snprintf(cmd, sizeof(cmd), "'%s/launch.sh'", Launcher_escapeSingleQuotes(path));
	if (ctx->callbacks && ctx->callbacks->queue_next) {
		ctx->callbacks->queue_next(cmd);
	}
}

/**
 * Launches a .pak application (legacy wrapper).
 */
static void openPak(char* path) {
	openPak_ctx(LauncherContext_get(), path);
}

/**
 * Launches a ROM with its emulator (context-aware version).
 *
 * This function handles:
 * - Multi-disc games (.m3u playlists)
 * - Resume states (saves/loads save state slot)
 * - Disc swapping for multi-disc games
 * - Adding to recently played list
 * - State restoration path tracking
 *
 * @param ctx Launcher context
 * @param path Full ROM path (may be .m3u or actual disc file)
 * @param last Path to save for state restoration (may differ from path)
 */
static void openRom_ctx(LauncherContext* ctx, char* path, char* last) {
	LOG_info("openRom(%s,%s)", path, last);

	char sd_path[MAX_PATH];
	SAFE_STRCPY(sd_path, path);

	char m3u_path[MAX_PATH];
	int has_m3u = hasM3u(sd_path, m3u_path);

	char recent_path[MAX_PATH];
	SAFE_STRCPY(recent_path, has_m3u ? m3u_path : sd_path);

	if (has_m3u && suffixMatch(".m3u", sd_path)) {
		M3U_getFirstDisc(m3u_path, sd_path);
	}

	char emu_name[MAX_PATH];
	getEmuName(sd_path, emu_name);

	if (ctx->should_resume && *ctx->should_resume) {
		char slot[16];
		getFile(ctx->slot_path, slot, 16);
		putFile(RESUME_SLOT_PATH, slot);
		*ctx->should_resume = 0;

		if (has_m3u) {
			char rom_file[MAX_PATH];
			SAFE_STRCPY(rom_file, strrchr(m3u_path, '/') + 1);

			// get disc for state
			char disc_path_path[MAX_PATH];
			(void)snprintf(disc_path_path, sizeof(disc_path_path), "%s/.launcher/%s/%s.%s.txt",
			               g_shared_userdata_path, emu_name, rom_file,
			               slot); // /.userdata/arm-480/.launcher/<EMU>/<romname>.ext.0.txt

			if (exists(disc_path_path)) {
				// switch to disc path
				char disc_path[MAX_PATH];
				getFile(disc_path_path, disc_path, MAX_PATH);
				if (disc_path[0] == '/')
					SAFE_STRCPY(sd_path, disc_path); // absolute
				else { // relative
					SAFE_STRCPY(sd_path, m3u_path);
					char* tmp = strrchr(sd_path, '/') + 1;
					safe_strcpy(tmp, disc_path, sizeof(sd_path) - (tmp - sd_path));
				}
			}
		}
	} else {
		putInt(RESUME_SLOT_PATH, 8); // resume hidden default state
	}

	char emu_path[MAX_PATH];
	getEmuPath(emu_name, emu_path);

	// NOTE: Launcher_escapeSingleQuotes() modifies the passed string
	// so we need to save the path before we call that
	char* alias = (ctx->recent_alias) ? *ctx->recent_alias : NULL;
	if (ctx->callbacks && ctx->callbacks->add_recent) {
		ctx->callbacks->add_recent(recent_path, alias);
	}
	if (ctx->callbacks && ctx->callbacks->save_last) {
		ctx->callbacks->save_last(last == NULL ? sd_path : last);
	}

	char cmd[MAX_PATH * 2];
	(void)snprintf(cmd, sizeof(cmd), "'%s' '%s'", Launcher_escapeSingleQuotes(emu_path),
	               Launcher_escapeSingleQuotes(sd_path));
	if (ctx->callbacks && ctx->callbacks->queue_next) {
		ctx->callbacks->queue_next(cmd);
	}
}

/**
 * Launches a ROM with its emulator (legacy wrapper).
 */
static void openRom(char* path, char* last) {
	openRom_ctx(LauncherContext_get(), path, last);
}
/**
 * Opens a directory for browsing or auto-launches its contents (context-aware).
 *
 * Auto-launch logic (when auto_launch=1):
 * - If directory contains a .cue file, launch it
 * - If directory contains a .m3u file, launch first disc
 * - Otherwise, open directory for browsing
 *
 * @param ctx Launcher context
 * @param path Full path to directory
 * @param auto_launch 1 to auto-launch contents, 0 to browse
 */
static void openDirectory_ctx(LauncherContext* ctx, char* path, int auto_launch) {
	char auto_path[256];
	// Auto-launch .cue file if present
	if (hasCue(path, auto_path) && auto_launch) {
		openRom_ctx(ctx, auto_path, path);
		return;
	}

	// Auto-launch .m3u playlist if present
	char m3u_path[256];
	safe_strcpy(m3u_path, auto_path, sizeof(m3u_path));
	char* tmp = strrchr(m3u_path, '.') + 1; // extension
	safe_strcpy(tmp, "m3u", sizeof(m3u_path) - (tmp - m3u_path)); // replace with m3u
	if (exists(m3u_path) && auto_launch) {
		auto_path[0] = '\0';
		if (M3U_getFirstDisc(m3u_path, auto_path)) {
			openRom_ctx(ctx, auto_path, path);
			return;
		}
	}

	// Get current directory from context
	Directory* current_top = (Directory*)*ctx->top;
	Directory** current_stack = *ctx->stack;
	LauncherRestoreState* restore = ctx->restore;

	int selected = 0;
	int start = selected;
	int end = 0;
	int entries_count = current_top ? (int)arrlen(current_top->entries) : 0;
	if (current_top && entries_count > 0) {
		if (restore->depth == (int)arrlen(current_stack) &&
		    current_top->selected == restore->relative) {
			selected = restore->selected;
			start = restore->start;
			end = restore->end;
		}
	}

	Directory* new_dir = Directory_new(path, selected);
	if (new_dir) {
		int new_entries_count = (int)arrlen(new_dir->entries);
		new_dir->start = start;
		new_dir->end =
		    end ? end : ((new_entries_count < ui.row_count) ? new_entries_count : ui.row_count);
		arrpush(current_stack, new_dir);
		*ctx->stack = current_stack; // arrpush may reallocate
		*ctx->top = new_dir;
	}
}

/**
 * Opens a directory for browsing or auto-launches its contents (legacy wrapper).
 */
static void openDirectory(char* path, int auto_launch) {
	openDirectory_ctx(LauncherContext_get(), path, auto_launch);
}
/**
 * Closes the current directory and returns to parent (context-aware).
 *
 * Saves current scroll position and selection for potential restoration.
 * Updates restore state and pops directory from stack.
 *
 * @param ctx Launcher context
 */
static void closeDirectory_ctx(LauncherContext* ctx) {
	Directory* current_top = (Directory*)*ctx->top;
	Directory** current_stack = *ctx->stack;
	LauncherRestoreState* restore = ctx->restore;

	restore->selected = current_top->selected;
	restore->start = current_top->start;
	restore->end = current_top->end;
	DirectoryArray_pop(current_stack);
	int stack_len = (int)arrlen(current_stack);
	restore->depth = stack_len;
	*ctx->top = current_stack[stack_len - 1];
	restore->relative = ((Directory*)*ctx->top)->selected;
}

/**
 * Closes the current directory and returns to parent (legacy wrapper).
 */
static void closeDirectory(void) {
	closeDirectory_ctx(LauncherContext_get());
}

/**
 * Opens an entry (ROM, directory, or application) - context-aware.
 *
 * Dispatches to appropriate handler based on entry type:
 * - ENTRY_ROM: Launch with emulator
 * - ENTRY_PAK: Launch application
 * - ENTRY_DIR: Open for browsing (with auto-launch)
 *
 * Special handling for collections: Uses collection path for
 * state restoration instead of actual ROM path.
 *
 * @param ctx Launcher context
 * @param self Entry to open
 */
static void Entry_open_ctx(LauncherContext* ctx, Entry* self) {
	// Set recent_alias via context
	if (ctx->recent_alias) {
		*ctx->recent_alias = self->name;
	}

	Directory* current_top = (Directory*)*ctx->top;

	if (self->type == ENTRY_ROM) {
		char* last = NULL;
		char last_path[MAX_PATH]; // Moved outside if block to fix invalidLifetime bug
		// Collection ROMs use collection path for state restoration
		if (prefixMatch(g_collections_path, current_top->path)) {
			char* tmp;
			char filename[MAX_PATH];

			tmp = strrchr(self->path, '/');
			if (tmp)
				SAFE_STRCPY(filename, tmp + 1);

			(void)snprintf(last_path, sizeof(last_path), "%s/%s", current_top->path, filename);
			last = last_path;
		}
		openRom_ctx(ctx, self->path, last);
	} else if (self->type == ENTRY_PAK) {
		openPak_ctx(ctx, self->path);
	} else if (self->type == ENTRY_DIR) {
		openDirectory_ctx(ctx, self->path, 1);
	}
}

/**
 * Opens an entry (ROM, directory, or application) - legacy wrapper.
 */
static void Entry_open(Entry* self) {
	Entry_open_ctx(LauncherContext_get(), self);
}

///////////////////////////////
// State persistence (last played/position)
///////////////////////////////

/**
 * Saves the last accessed path for state restoration.
 *
 * Special case: Recently played path is implicit (always first item)
 * so we don't need to save the specific ROM, just that recents was open.
 *
 * @param path Path to save
 */
static void saveLast(char* path) {
	// special case for recently played
	if (exactMatch(top->path, g_faux_recent_path)) {
		// Most recent game is always at top, no need to save specific ROM
		path = g_faux_recent_path;
	}
	putFile(LAST_PATH, path);
}

/**
 * Loads and restores the last accessed path and selection.
 *
 * Rebuilds the directory stack from the saved path, restoring:
 * - Which directories were open
 * - Which item was selected
 * - Scroll position
 *
 * Handles special cases:
 * - Collated ROM folders (matches by prefix)
 * - Collection entries (matches by filename)
 * - Auto-launch directories (doesn't re-launch)
 *
 * Called after loading root directory during startup.
 */
static void loadLast(void) {
	if (!exists(LAST_PATH))
		return;

	char last_path[256];
	getFile(LAST_PATH, last_path, 256);

	char full_path[256];
	safe_strcpy(full_path, last_path, sizeof(full_path));

	char* tmp;
	char filename[256];
	tmp = strrchr(last_path, '/');
	if (tmp)
		safe_strcpy(filename, tmp, sizeof(filename));

	char** last = NULL;
	while (!exactMatch(last_path, g_sdcard_path)) {
		char* path_copy = strdup(last_path);
		if (path_copy)
			arrpush(last, path_copy);

		char* slash = strrchr(last_path, '/');
		last_path[(slash - last_path)] = '\0';
	}

	while (arrlen(last) > 0) {
		char* path = arrpop(last);
		if (!exactMatch(
		        path,
		        g_roms_path)) { // romsDir is effectively root as far as restoring state after a game
			// Extract collation prefix if this is a collated console dir (e.g., "Game Boy (USA)")
			// This allows matching against other regions like "Game Boy (Japan)"
			char collated_path[LAUNCHER_STATE_MAX_PATH];
			collated_path[0] = '\0';
			if (isConsoleDir(path)) {
				LauncherState_getCollationPrefix(path, collated_path);
			}

			int entries_count = (int)arrlen(top->entries);
			for (int i = 0; i < entries_count; i++) {
				Entry* entry = top->entries[i];

				// NOTE: strlen() is required for collated_path, '\0' wasn't reading as NULL for some reason
				if (exactMatch(entry->path, path) ||
				    (strlen(collated_path) && prefixMatch(collated_path, entry->path)) ||
				    (prefixMatch(g_collections_path, full_path) &&
				     suffixMatch(filename, entry->path))) {
					top->selected = i;
					if (i >= top->end) {
						top->start = i;
						top->end = top->start + ui.row_count;
						if (top->end > entries_count) {
							top->end = entries_count;
							top->start = top->end - ui.row_count;
						}
					}
					if (arrlen(last) == 0 && !exactMatch(entry->path, g_faux_recent_path) &&
					    !(!exactMatch(entry->path, g_collections_path) &&
					      prefixMatch(g_collections_path, entry->path)))
						break; // don't show contents of auto-launch dirs

					if (entry->type == ENTRY_DIR) {
						openDirectory(entry->path, 0);
						break;
					}
				}
			}
		}
		free(path); // we took ownership when we popped it
	}

	// Free last (string array)
	arrfree(last);
}

///////////////////////////////////////

///////////////////////////////
// Menu initialization and cleanup
///////////////////////////////

/**
 * Initializes the menu system.
 *
 * Sets up:
 * - Directory navigation stack
 * - Recently played games list
 * - Root directory
 * - Last accessed path restoration
 */
static void Menu_init(void) {
	stack = NULL; // stb_ds dynamic array, starts as NULL
	recents = NULL; // stb_ds dynamic array, starts as NULL

	openDirectory(g_sdcard_path, 0);
	loadLast(); // restore state when available
}

/**
 * Cleans up menu system resources.
 *
 * Frees all allocated memory for recents and directory stack.
 */
static void Menu_quit(void) {
	RecentArray_free(recents);
	DirectoryArray_free(stack);
	EmuCache_free();
	ResCache_free();
}

///////////////////////////////
// Context initialization
///////////////////////////////

/**
 * Sets up the Launcher context with pointers to globals.
 *
 * This enables navigation functions to be tested with mock contexts
 * and allows gradual migration to context-based function signatures.
 */
static void LauncherContext_setup(void) {
	LauncherContext* ctx = LauncherContext_get();

	// Wire up navigation state (now properly typed)
	ctx->top = &top;
	ctx->stack = &stack;
	ctx->recents = &recents;

	// Wire up runtime flags
	ctx->quit = &quit;
	ctx->can_resume = &can_resume;
	ctx->should_resume = &should_resume;
	ctx->simple_mode = &simple_mode;

	// Wire up resume state
	ctx->slot_path = slot_path;
	ctx->slot_path_size = sizeof(slot_path);

	// Wire up state restoration
	ctx->restore = &g_restore_state;

	// Wire up UI state (from api.h)
	ctx->ui = &ui;

	// Wire up recent alias
	ctx->recent_alias = &recent_alias;

	LauncherContext_initGlobals(ctx);

	// Initialize callbacks for navigation module
	// Note: static to ensure pointer remains valid after function returns
	static LauncherCallbacks callbacks = {0};
	callbacks.add_recent = addRecent;
	callbacks.save_recents = saveRecents;
	callbacks.queue_next = queueNext;
	callbacks.save_last = saveLast;
	callbacks.open_directory = openDirectory;
	callbacks.directory_new = (LauncherDirectoryNewFunc)Directory_new;
	callbacks.exists = exists;
	callbacks.put_file = putFile;
	callbacks.get_file = getFile;
	callbacks.put_int = putInt;
	LauncherContext_initCallbacks(ctx, &callbacks);
}

///////////////////////////////
// Main entry point
///////////////////////////////

/**
 * Launcher launcher main function.
 *
 * Initialization:
 * 1. Check for auto-resume (return from sleep with game running)
 * 2. Initialize graphics, input, power management
 * 3. Load menu state and recents
 *
 * Main Loop:
 * - Polls input (D-pad, buttons, shoulder buttons)
 * - Updates selection and scroll window
 * - Handles:
 *   - Navigation (up/down/left/right)
 *   - Alphabetical jump (L1/R1 shoulder buttons)
 *   - Open entry (A button)
 *   - Go back (B button)
 *   - Resume game (X button if save state exists)
 *   - Menu button (show version info or sleep)
 *   - Hardware settings (brightness/volume via PWR_update)
 * - Renders:
 *   - Entry list with selection highlight
 *   - Thumbnails from .res/ folders (if available)
 *   - Hardware status icons (battery, brightness, etc.)
 *   - Button hints at bottom
 * - Handles HDMI hotplug detection
 *
 * Exit:
 * - Saves state and cleans up resources
 * - If a ROM/app was launched, it's queued in /tmp/next
 */
int main(int argc, char* argv[]) {
	// Initialize logging early (reads LOG_FILE and LOG_SYNC from environment)
	// This must happen before any LOG_* calls to ensure crash-safe logging
	log_open(NULL);

	// Initialize runtime paths from environment (supports LessOS dynamic storage)
	Paths_init();

	// Detect platform variant early (before any code that may need variant info)
	PLAT_detectVariant(&platform_variant);

	// Check for auto-resume first (fast path)
	if (autoResume()) {
		log_close();
		return 0;
	}

	simple_mode = exists(g_simple_mode_path);

	// Initialize context with pointers to globals
	// This enables navigation functions to be tested and migrated incrementally
	LauncherContext_setup();

	LOG_info("Starting Launcher on %s", PLATFORM);

	LOG_debug("InitSettings");
	InitSettings();

	LOG_debug("GFX_init");
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	if (screen == NULL) {
		LOG_error("Failed to initialize video");
		log_close();
		return EXIT_FAILURE;
	}

	LOG_debug("PAD_init");
	PAD_init();

	LOG_debug("PWR_init");
	PWR_init();
	if (!HAS_POWER_BUTTON && !simple_mode)
		PWR_disableSleep();

	SDL_Surface* version = NULL;

	LOG_debug("LauncherThumbnail_loaderInit");
	LauncherThumbnail_loaderInit();

	LOG_debug("EmuCache_init");
	int emu_count = EmuCache_init(g_paks_path, g_sdcard_path, PLATFORM);
	LOG_info("Cached %d emulators", emu_count);

	LOG_debug("ResCache_init");
	ResCache_init();

	LOG_debug("Menu_init");
	Menu_init();

	// Reduce CPU speed for menu browsing (saves power and heat)
	PWR_setCPUSpeed(CPU_SPEED_POWERSAVE);

	PAD_reset();
	int dirty = 1; // Set to 1 when screen needs redraw
	int show_version = 0; // 1 when showing version overlay
	int show_setting = 0; // 1=brightness, 2=volume overlay
	int was_online = PLAT_isOnline();

	///////////////////////////////
	// List Rendering Caches
	//
	// Two caching systems optimize list rendering:
	//
	// 1. THUMBNAIL CACHE (thumb_cache)
	//    - Holds scaled thumbnail surfaces for nearby entries
	//    - LRU eviction with preloading in scroll direction
	//    - Async loading via background thread (see thumb_loader_thread)
	//    - Key: entry index (invalidated on directory change)
	//
	// 2. TEXT CACHE (text_cache)
	//    - Holds rendered TTF text surfaces for visible entries
	//    - Round-robin eviction when full
	//    - Keyed by entry pointer + width (survives scrolling)
	//    - Also caches unique_surface for disambiguation text
	//
	// Both caches are cleared on directory change since entry
	// pointers become invalid when directories are freed/reallocated.
	///////////////////////////////

	// Thumbnail cache - FIFO with preloading, tracks displayed item (logic in launcher_thumbnail.c)
	LauncherThumbnailCache thumb_cache;
	LauncherThumbnail_cacheInit(&thumb_cache);
	int last_selected_index = -1; // For scroll direction detection
	Directory* last_directory = NULL; // Detect directory changes

	// Thumbnail display state (display tracking is inside thumb_cache)
	Entry* last_rendered_entry = NULL; // Entry we last processed (change detection)
	int thumb_exists = 0; // Whether current entry has a thumbnail file

	// Thumbnail fade animation (logic in launcher_thumbnail.c)
	LauncherThumbnailFadeState thumb_fade;
	LauncherThumbnail_fadeInit(&thumb_fade, LAUNCHER_THUMBNAIL_FADE_DURATION_MS);

	// Thumbnail dimensions (constant for session)
	int thumb_padding = DP(ui.edge_padding);
	int thumb_max_width = (ui.screen_width_px * THUMB_MAX_WIDTH_PERCENT) / 100 - thumb_padding;
	int thumb_max_height = ui.screen_height_px - (thumb_padding * 2);

	// Text cache - round-robin eviction
	typedef struct {
		SDL_Surface* surface; // Main text (white for unselected)
		SDL_Surface* unique_surface; // Disambiguation text (dark, shown behind main)
		Entry* entry; // Cache key (NULL = empty slot)
		int width; // Rendered width (part of cache key)
	} TextCacheItem;
#define TEXT_CACHE_SIZE 16 // >= max visible rows
	TextCacheItem text_cache[TEXT_CACHE_SIZE] = {0};
	int text_cache_next_evict = 0;

	LOG_debug("Entering main loop");
	while (!quit) {
		GFX_startFrame();
		unsigned long now = SDL_GetTicks();

		PAD_poll();

		int selected = top->selected;
		int total = (int)arrlen(top->entries);

		// Update power management (handles brightness/volume adjustments)
		PWR_update(&dirty, &show_setting, NULL, NULL);

		// Track online status changes (wifi icon)
		int is_online = PLAT_isOnline();
		if (was_online != is_online)
			dirty = 1;
		was_online = is_online;

		// Input handling - version overlay mode
		if (show_version) {
			if (PAD_justPressed(BTN_B) || PAD_tappedMenu(now)) {
				show_version = 0;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode)
					PWR_disableSleep();
			}
		} else {
			// Input handling - normal browsing mode
			if (PAD_tappedMenu(now)) {
				show_version = 1;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode)
					PWR_enableSleep();
			} else if (total > 0) {
				if (PAD_justRepeated(BTN_UP)) {
					if (selected == 0 && !PAD_justPressed(BTN_UP)) {
						// stop at top
					} else {
						selected -= 1;
						if (selected < 0) {
							selected = total - 1;
							int start = total - ui.row_count;
							top->start = (start < 0) ? 0 : start;
							top->end = total;
						} else if (selected < top->start) {
							top->start -= 1;
							top->end -= 1;
						}
					}
				} else if (PAD_justRepeated(BTN_DOWN)) {
					if (selected == total - 1 && !PAD_justPressed(BTN_DOWN)) {
						// stop at bottom
					} else {
						selected += 1;
						if (selected >= total) {
							selected = 0;
							top->start = 0;
							top->end = (total < ui.row_count) ? total : ui.row_count;
						} else if (selected >= top->end) {
							top->start += 1;
							top->end += 1;
						}
					}
				}
				if (PAD_justRepeated(BTN_LEFT)) {
					selected -= ui.row_count;
					if (selected < 0) {
						selected = 0;
						top->start = 0;
						top->end = (total < ui.row_count) ? total : ui.row_count;
					} else if (selected < top->start) {
						top->start -= ui.row_count;
						if (top->start < 0)
							top->start = 0;
						top->end = top->start + ui.row_count;
					}
				} else if (PAD_justRepeated(BTN_RIGHT)) {
					selected += ui.row_count;
					if (selected >= total) {
						selected = total - 1;
						int start = total - ui.row_count;
						top->start = (start < 0) ? 0 : start;
						top->end = total;
					} else if (selected >= top->end) {
						top->end += ui.row_count;
						if (top->end > total)
							top->end = total;
						top->start = top->end - ui.row_count;
					}
				}
			}

			// Alphabetical navigation with shoulder buttons
			if (PAD_justRepeated(BTN_L1) && !PAD_isPressed(BTN_R1) &&
			    !PWR_ignoreSettingInput(BTN_L1, show_setting)) {
				Entry* entry = top->entries[selected];
				int i = entry->alpha - 1;
				if (i >= 0) {
					selected = top->alphas->items[i];
					if (total > ui.row_count) {
						top->start = selected;
						top->end = top->start + ui.row_count;
						if (top->end > total)
							top->end = total;
						top->start = top->end - ui.row_count;
					}
				}
			} else if (PAD_justRepeated(BTN_R1) && !PAD_isPressed(BTN_L1) &&
			           !PWR_ignoreSettingInput(BTN_R1, show_setting)) {
				Entry* entry = top->entries[selected];
				int i = entry->alpha + 1;
				if (i < top->alphas->count) {
					selected = top->alphas->items[i];
					if (total > ui.row_count) {
						top->start = selected;
						top->end = top->start + ui.row_count;
						if (top->end > total)
							top->end = total;
						top->start = top->end - ui.row_count;
					}
				}
			}

			// Update selection and mark dirty if changed
			if (selected != top->selected) {
				top->selected = selected;
				dirty = 1;
				// Check if selected ROM has save state for resume
				if (total > 0)
					readyResume(top->entries[top->selected]);
			}

			// Entry opening/navigation actions
			if (total > 0 && can_resume && PAD_justReleased(BTN_RESUME)) {
				should_resume = 1;
				Entry_open(top->entries[top->selected]);
				dirty = 1;
			} else if (total > 0 && PAD_justPressed(BTN_A)) {
				Entry_open(top->entries[top->selected]);
				total = (int)arrlen(top->entries);
				dirty = 1;
				// Re-check resume after ROM/PAK launch returns (directory change block handles dir nav)
				if (total > 0)
					readyResume(top->entries[top->selected]);
			} else if (PAD_justPressed(BTN_B) && (int)arrlen(stack) > 1) {
				closeDirectory();
				total = (int)arrlen(top->entries);
				dirty = 1;
				// Note: readyResume handled by directory change block below
			}
		}

		// Directory change detection - handles startup and navigation between folders
		// When directory changes, all cached data becomes invalid (entries are freed/reallocated)
		if (top != last_directory) {
			// Clear thumbnail cache (surfaces point to old entries)
			if (thumb_cache.size > 0)
				LOG_debug("thumb: clearing cache (%d items)", thumb_cache.size);
			thumb_cache_clear(&thumb_cache);
			last_selected_index = -1;
			last_rendered_entry = NULL; // Prevent dangling pointer comparison
			thumb_exists = 0;
			LauncherThumbnail_fadeReset(&thumb_fade);
			last_directory = top;

			// Check resume state for initially selected entry
			if (total > 0)
				readyResume(top->entries[top->selected]);

			// Clear text cache (entry pointers are now invalid)
			int text_cache_count = 0;
			for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
				if (text_cache[i].surface) {
					SDL_FreeSurface(text_cache[i].surface);
					text_cache[i].surface = NULL;
					text_cache_count++;
				}
				if (text_cache[i].unique_surface) {
					SDL_FreeSurface(text_cache[i].unique_surface);
					text_cache[i].unique_surface = NULL;
				}
				text_cache[i].entry = NULL;
			}
			if (text_cache_count > 0)
				LOG_debug("text cache: CLEAR %d items", text_cache_count);
			text_cache_next_evict = 0;
		}

		///////////////////////////////
		// Thumbnail Loading Flow
		//
		// Step 1: Poll async loader for completed thumbnails, add to cache
		// Step 2: On selection change, check cache or request async load
		// Step 3: If async load completed since last frame, start displaying
		// Step 4: Animate fade-in (handled after this block)
		//
		// Fast scrolling optimization: skip file existence checks while
		// nav buttons are held to keep UI responsive
		///////////////////////////////

		// Step 1: Poll for async thumbnail load completion
		{
			int loaded_index;
			SDL_Surface* loaded = LauncherThumbnail_loaderGet(&loaded_index, NULL);
			if (loaded) {
				thumb_cache_push(&thumb_cache, loaded, "", loaded_index);
			}
		}

		// Step 2: Handle selection changes
		Entry* current_entry = (top && total > 0) ? top->entries[top->selected] : NULL;
		int current_selected = top ? top->selected : -1;
		if (current_entry != last_rendered_entry) {
			// Selection changed - reset thumbnail state
			LauncherThumbnail_cacheClearDisplayed(&thumb_cache);
			thumb_exists = 0;

			// Detect fast scrolling (nav button held, not just pressed)
			int nav_held = (PAD_isPressed(BTN_UP) && !PAD_justPressed(BTN_UP)) ||
			               (PAD_isPressed(BTN_DOWN) && !PAD_justPressed(BTN_DOWN)) ||
			               (PAD_isPressed(BTN_LEFT) && !PAD_justPressed(BTN_LEFT)) ||
			               (PAD_isPressed(BTN_RIGHT) && !PAD_justPressed(BTN_RIGHT));

			// During fast scroll, skip file checks - will handle when user stops
			if (nav_held) {
				// Don't update last_rendered_entry so we retry when scroll stops
			} else if (!current_entry || !current_entry->path || show_version) {
				// No valid entry to show thumbnail for
				last_rendered_entry = current_entry;
				last_selected_index = current_selected;
			} else {
				// Build and check thumbnail path (uses cached .res directory scan)
				char thumb_path[MAX_PATH];
				thumb_exists = ResCache_getThumbPath(current_entry->path, thumb_path);

				if (!thumb_exists) {
					// No thumbnail file for this entry
					last_rendered_entry = current_entry;
					last_selected_index = current_selected;
				} else {
					// Calculate preload hint (next item in scroll direction)
					char hint_path[MAX_PATH];
					int direction = (current_selected > last_selected_index) ? 1 : -1;
					int hint_index = current_selected + direction;
					int has_hint = 0;
					if (hint_index >= 0 && hint_index < total) {
						Entry* hint_entry = top->entries[hint_index];
						has_hint = ResCache_getThumbPath(hint_entry->path, hint_path);
					}

					// Check cache
					int cached_slot = LauncherThumbnail_cacheFind(&thumb_cache, current_selected);
					SDL_Surface* cached_surface =
					    cached_slot >= 0 ? (SDL_Surface*)LauncherThumbnail_cacheGetData(
					                           &thumb_cache, cached_slot)
					                     : NULL;
					if (cached_surface) {
						// Cache HIT - mark as displayed
						LauncherThumbnail_cacheSetDisplayed(&thumb_cache, current_selected);
						if (SDLX_SupportsSurfaceAlphaMod()) {
							LauncherThumbnail_fadeStart(&thumb_fade, now);
						} else {
							LauncherThumbnail_fadeReset(&thumb_fade);
						}
						dirty = 1;

						// Queue preload for next item (if not already cached)
						if (has_hint && LauncherThumbnail_cacheFind(&thumb_cache, hint_index) < 0) {
							LauncherThumbnail_loaderRequest(hint_path, thumb_max_width,
							                                thumb_max_height, hint_index, 1, NULL,
							                                -1);
						}
					} else {
						// Cache MISS - request async load with preload hint
						LOG_debug("thumb: idx=%d MISS -> requesting (hint=%d)", current_selected,
						          has_hint ? hint_index : -1);
						LauncherThumbnail_loaderRequest(thumb_path, thumb_max_width,
						                                thumb_max_height, current_selected, 0,
						                                has_hint ? hint_path : NULL, hint_index);
					}
					last_rendered_entry = current_entry;
					last_selected_index = current_selected;
				}
			}
		}

		// Step 3: Check if async load completed (no selection change, but thumbnail now ready)
		if (thumb_exists && !LauncherThumbnail_cacheIsDisplayedValid(&thumb_cache)) {
			int cached_slot = LauncherThumbnail_cacheFind(&thumb_cache, current_selected);
			SDL_Surface* cached_surface =
			    cached_slot >= 0
			        ? (SDL_Surface*)LauncherThumbnail_cacheGetData(&thumb_cache, cached_slot)
			        : NULL;
			if (cached_surface) {
				LOG_debug("thumb: idx=%d ready", current_selected);
				LauncherThumbnail_cacheSetDisplayed(&thumb_cache, current_selected);
				if (SDLX_SupportsSurfaceAlphaMod()) {
					LauncherThumbnail_fadeStart(&thumb_fade, now);
				} else {
					LauncherThumbnail_fadeReset(&thumb_fade);
				}
				dirty = 1;
			}
		}

		// Check if displayed item was evicted (cache handles this automatically)
		// Note: displayed_index >= 0 distinguishes "was displayed then evicted" from "never displayed"
		// Note: Keep thumb_exists=1 so text layout stays narrow while we re-request
		if (thumb_exists && thumb_cache.displayed_index >= 0 &&
		    !LauncherThumbnail_cacheIsDisplayedValid(&thumb_cache)) {
			// Surface was evicted - reset state so Step 2 re-requests next frame
			last_rendered_entry = NULL;
			dirty = 1;
		}

		// Get current thumbnail surface (fresh lookup each frame - never store the pointer)
		SDL_Surface* thumb_surface =
		    (SDL_Surface*)LauncherThumbnail_cacheGetDisplayedData(&thumb_cache);

		// Check if thumbnail is actually loaded and ready to display
		int showing_thumb = (!show_version && total > 0 && thumb_surface && thumb_surface->w > 0 &&
		                     thumb_surface->h > 0);

		// Animate thumbnail fade-in with smoothstep easing (SDL 2.0 only)
		if (SDLX_SupportsSurfaceAlphaMod() && thumb_surface &&
		    LauncherThumbnail_fadeIsActive(&thumb_fade)) {
			if (LauncherThumbnail_fadeUpdate(&thumb_fade, now)) {
				dirty = 1; // Keep rendering while animating
			}
		}

		// Rendering
		if (dirty) {
			GFX_clear(screen);

			// Display thumbnail if available (right-aligned with padding)
			if (showing_thumb) {
				int padding = DP(ui.edge_padding);
				int ox = ui.screen_width_px - thumb_surface->w - padding;
				int oy = (ui.screen_height_px - thumb_surface->h) / 2;
				SDLX_SetAlphaMod(thumb_surface, thumb_fade.alpha);
				SDL_BlitSurface(thumb_surface, NULL, screen, &(SDL_Rect){ox, oy, 0, 0});
			}

			// Text area width when thumbnail is showing (unselected items)
			int text_area_width = (ui.screen_width_px * THUMB_TEXT_WIDTH_PERCENT) / 100;

			int ow = GFX_blitHardwareGroup(screen, show_setting);

			if (show_version) {
				if (!version) {
					char release[256];
					char version_path[MAX_PATH];
					(void)snprintf(version_path, sizeof(version_path), "%s/version.txt",
					               g_root_system_path);
					getFile(version_path, release, 256);

					char *tmp, *commit;
					commit = strrchr(release, '\n');
					commit[0] = '\0';
					commit = strrchr(release, '\n') + 1;
					tmp = strchr(release, '\n');
					tmp[0] = '\0';

					// TODO: not sure if I want bare PLAT_* calls here
					char* extra_key = "Model";
					char* extra_val = PLAT_getModel();

					SDL_Surface* release_txt =
					    TTF_RenderUTF8_Blended(font.large, "Release", COLOR_DARK_TEXT);
					SDL_Surface* version_txt =
					    TTF_RenderUTF8_Blended(font.large, release, COLOR_WHITE);
					SDL_Surface* commit_txt =
					    TTF_RenderUTF8_Blended(font.large, "Commit", COLOR_DARK_TEXT);
					SDL_Surface* hash_txt = TTF_RenderUTF8_Blended(font.large, commit, COLOR_WHITE);

					SDL_Surface* key_txt =
					    TTF_RenderUTF8_Blended(font.large, extra_key, COLOR_DARK_TEXT);
					SDL_Surface* val_txt =
					    TTF_RenderUTF8_Blended(font.large, extra_val, COLOR_WHITE);

					int l_width = 0;
					int r_width = 0;

					if (release_txt->w > l_width)
						l_width = release_txt->w;
					if (commit_txt->w > l_width)
						l_width = commit_txt->w;
					if (key_txt->w > l_width)
						l_width = commit_txt->w;

					if (version_txt->w > r_width)
						r_width = version_txt->w;
					if (hash_txt->w > r_width)
						r_width = hash_txt->w;
					if (val_txt->w > r_width)
						r_width = val_txt->w;

#define VERSION_LINE_HEIGHT 24
					int x = l_width + DP(8);
					int w = x + r_width;
					int h = DP(VERSION_LINE_HEIGHT * 4);
					version = SDL_CreateRGBSurface(0, w, h, 16, 0, 0, 0, 0);

					SDL_BlitSurface(release_txt, NULL, version, &(SDL_Rect){0, 0, 0, 0});
					SDL_BlitSurface(version_txt, NULL, version, &(SDL_Rect){x, 0, 0, 0});
					SDL_BlitSurface(commit_txt, NULL, version,
					                &(SDL_Rect){0, DP(VERSION_LINE_HEIGHT), 0, 0});
					SDL_BlitSurface(hash_txt, NULL, version,
					                &(SDL_Rect){x, DP(VERSION_LINE_HEIGHT), 0, 0});

					SDL_BlitSurface(key_txt, NULL, version,
					                &(SDL_Rect){0, DP(VERSION_LINE_HEIGHT * 3), 0, 0});
					SDL_BlitSurface(val_txt, NULL, version,
					                &(SDL_Rect){x, DP(VERSION_LINE_HEIGHT * 3), 0, 0});

					SDL_FreeSurface(release_txt);
					SDL_FreeSurface(version_txt);
					SDL_FreeSurface(commit_txt);
					SDL_FreeSurface(hash_txt);
					SDL_FreeSurface(key_txt);
					SDL_FreeSurface(val_txt);
				}
				// Version splash centering - work in DP space
				int version_w_dp = (int)(version->w / gfx_dp_scale + 0.5f);
				int version_h_dp = (int)(version->h / gfx_dp_scale + 0.5f);
				int center_x_dp = (ui.screen_width - version_w_dp) / 2;
				int center_y_dp = (ui.screen_height - version_h_dp) / 2;
				SDL_BlitSurface(version, NULL, screen,
				                &(SDL_Rect){DP(center_x_dp), DP(center_y_dp), 0, 0});

				// buttons (duped and trimmed from below)
				if (show_setting && !GetHDMI())
					GFX_blitHardwareHints(screen, show_setting);
				else
					GFX_blitButtonGroup(
					    (char*[]){BTN_SLEEP == BTN_POWER ? "POWER" : "MENU", "SLEEP", NULL}, 0,
					    screen, 0);

				GFX_blitButtonGroup((char*[]){"B", "BACK", NULL}, 0, screen, 1);
			} else {
				// list
				if (total > 0) {
					int selected_row = top->selected - top->start;
					for (int i = top->start, j = 0; i < top->end; i++, j++) {
						Entry* entry = top->entries[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;
						// Calculate available width in pixels
						// Use fixed widths when thumbnail is showing (prevents text reflow)
						int available_width;
						if (thumb_exists) {
							if (j == selected_row) {
								// Selected item gets more width
								available_width =
								    (ui.screen_width_px * THUMB_SELECTED_WIDTH_PERCENT) / 100 -
								    DP(ui.edge_padding * 2);
							} else {
								// Unselected items constrained to text area
								available_width = text_area_width - DP(ui.edge_padding * 2);
							}
						} else {
							available_width = DP(ui.screen_width) - DP(ui.edge_padding * 2);
							if (i == top->start)
								available_width -= ow;
						}

						trimSortingMeta(&entry_name);

						char display_name[256];
						int text_width = GFX_truncateText(
						    font.large, entry_unique ? entry_unique : entry_name, display_name,
						    available_width, DP(ui.button_padding * 2));
						int max_width = MIN(available_width, text_width);

						int is_selected = (j == selected_row);
						if (is_selected) {
							GFX_blitPill(ASSET_WHITE_PILL, screen,
							             &(SDL_Rect){ui.edge_padding_px,
							                         ui.edge_padding_px + (j * ui.pill_height_px),
							                         max_width, ui.pill_height_px});
						}

						// Text Rendering with Caching
						// - Selected row: render fresh (black text, not cached)
						// - Unselected rows: check cache first, render on miss
						// - Entries with unique names: also cache disambiguation text
						SDL_Surface* text;
						if (is_selected) {
							// Selected row: always render fresh (black text)
							text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_BLACK);
						} else {
							// Search cache for this entry (not by row position!)
							int cache_slot = -1;
							for (int c = 0; c < TEXT_CACHE_SIZE; c++) {
								if (text_cache[c].entry == entry &&
								    text_cache[c].width == available_width &&
								    text_cache[c].surface) {
									cache_slot = c;
									break;
								}
							}

							if (cache_slot >= 0) {
								// Cache hit - use cached surfaces
								text = text_cache[cache_slot].surface;
								if (entry->unique && text_cache[cache_slot].unique_surface) {
									SDL_BlitSurface(
									    text_cache[cache_slot].unique_surface,
									    &(SDL_Rect){0, 0, max_width - DP(ui.button_padding * 2),
									                text_cache[cache_slot].unique_surface->h},
									    screen,
									    &(SDL_Rect){ui.edge_padding_px + DP(ui.button_padding),
									                ui.edge_padding_px + (j * ui.pill_height_px) +
									                    ui.text_offset_px,
									                0, 0});
								}
							} else {
								// Cache miss: render and store
								// For entries with unique names, render unique text first
								SDL_Surface* unique_text = NULL;
								if (entry->unique) {
									trimSortingMeta(&entry_unique);
									char unique_name[256];
									GFX_truncateText(font.large, entry_unique, unique_name,
									                 available_width, DP(ui.button_padding * 2));
									unique_text = TTF_RenderUTF8_Blended(font.large, unique_name,
									                                     COLOR_DARK_TEXT);
									// Blit unique text now
									SDL_BlitSurface(
									    unique_text,
									    &(SDL_Rect){0, 0, max_width - DP(ui.button_padding * 2),
									                unique_text->h},
									    screen,
									    &(SDL_Rect){ui.edge_padding_px + DP(ui.button_padding),
									                ui.edge_padding_px + (j * ui.pill_height_px) +
									                    ui.text_offset_px,
									                0, 0});
									// Re-truncate display_name for main text
									GFX_truncateText(font.large, entry_name, display_name,
									                 available_width, DP(ui.button_padding * 2));
								}

								text =
								    TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);

								// Find empty slot, or use round-robin eviction
								int store_slot = -1;
								for (int c = 0; c < TEXT_CACHE_SIZE; c++) {
									if (!text_cache[c].surface) {
										store_slot = c;
										break;
									}
								}
								if (store_slot < 0) {
									// Cache full, evict using round-robin
									store_slot = text_cache_next_evict;
									text_cache_next_evict =
									    (text_cache_next_evict + 1) % TEXT_CACHE_SIZE;
								}
								if (text_cache[store_slot].surface)
									SDL_FreeSurface(text_cache[store_slot].surface);
								if (text_cache[store_slot].unique_surface)
									SDL_FreeSurface(text_cache[store_slot].unique_surface);
								text_cache[store_slot].surface = text;
								text_cache[store_slot].unique_surface = unique_text;
								text_cache[store_slot].entry = entry;
								text_cache[store_slot].width = available_width;
							}
						}

						SDL_BlitSurface(
						    text, &(SDL_Rect){0, 0, max_width - DP(ui.button_padding * 2), text->h},
						    screen,
						    &(SDL_Rect){ui.edge_padding_px + DP(ui.button_padding),
						                ui.edge_padding_px + (j * ui.pill_height_px) +
						                    ui.text_offset_px,
						                0, 0});

						// Only free if not cached (selected row)
						if (is_selected)
							SDL_FreeSurface(text);
					}
				} else {
					// Use DP-based wrapper for proper scaling
					GFX_blitMessage_DP(font.large, "Empty folder", screen, 0, 0, ui.screen_width,
					                   ui.screen_height);
				}

				// buttons
				if (show_setting && !GetHDMI())
					GFX_blitHardwareHints(screen, show_setting);
				else if (can_resume)
					GFX_blitButtonGroup((char*[]){"X", "RESUME", NULL}, 0, screen, 0);
				else
					GFX_blitButtonGroup(
					    (char*[]){BTN_SLEEP == BTN_POWER ? "POWER" : "MENU",
					              BTN_SLEEP == BTN_POWER || simple_mode ? "SLEEP" : "INFO", NULL},
					    0, screen, 0);

				if (total == 0) {
					if ((int)arrlen(stack) > 1) {
						GFX_blitButtonGroup((char*[]){"B", "BACK", NULL}, 0, screen, 1);
					}
				} else {
					if ((int)arrlen(stack) > 1) {
						GFX_blitButtonGroup((char*[]){"B", "BACK", "A", "OPEN", NULL}, 1, screen,
						                    1);
					} else {
						GFX_blitButtonGroup((char*[]){"A", "OPEN", NULL}, 0, screen, 1);
					}
				}
			}

			GFX_present(NULL);
			dirty = 0;
		} else
			GFX_sync();

		// if (!first_draw) {
		// 	first_draw = SDL_GetTicks();
		// 	LOG_info("- first draw: %lu", first_draw - main_begin);
		// }

		// HDMI hotplug detection
		// When HDMI is connected/disconnected, restart to reinit graphics
		// with correct resolution. Save state so we return to same position.
		static int had_hdmi = -1;
		int has_hdmi = GetHDMI();
		if (had_hdmi == -1)
			had_hdmi = has_hdmi;
		if (has_hdmi != had_hdmi) {
			had_hdmi = has_hdmi;

			Entry* entry = top->entries[top->selected];
			LOG_info("restarting after HDMI change... (%s)", entry->path);
			saveLast(entry->path);
			sleep(4); // Brief pause for HDMI to stabilize
			quit = 1;
		}
	}

	if (version)
		SDL_FreeSurface(version);

	if (screen) {
		GFX_clear(screen);
		GFX_present(NULL);
	}

	thumb_cache_clear(&thumb_cache);

	// Free text cache surfaces
	for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
		if (text_cache[i].surface)
			SDL_FreeSurface(text_cache[i].surface);
		if (text_cache[i].unique_surface)
			SDL_FreeSurface(text_cache[i].unique_surface);
	}

	LauncherThumbnail_loaderQuit();
	Menu_quit();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();

	sync();

	// Close log file (flushes and syncs to disk)
	log_close();

	return EXIT_SUCCESS;
}
