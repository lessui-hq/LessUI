/**
 * launcher_emu_cache.c - Emulator availability cache for Launcher
 *
 * Replaces per-call exists() checks with a single startup scan and O(1) lookups.
 * For 50 console folders, this eliminates ~100 syscalls per root menu load.
 */

#include "launcher_emu_cache.h"
#include "../vendor/stb/stb_ds.h"
#include "utils.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Map entry type for string -> string
typedef struct {
	char* key;
	char* value;
} EmuCacheEntry;

// Global cache - initialized once at startup
static EmuCacheEntry* emu_cache = NULL;

/**
 * Scans a directory for .pak subdirectories and adds them to the cache.
 *
 * @param dir_path Directory to scan (e.g., "/mnt/SDCARD/.system/miyoomini/paks/Emus")
 * @return Number of paks found
 */
static int scanPakDirectory(const char* dir_path) {
	int count = 0;

	DIR* dh = opendir(dir_path);
	if (!dh)
		return 0;

	struct dirent* dp;
	while ((dp = readdir(dh)) != NULL) {
		// Skip hidden entries
		if (dp->d_name[0] == '.')
			continue;

		// Check for .pak suffix
		size_t len = strlen(dp->d_name);
		if (len < 5)
			continue;

		if (strcmp(dp->d_name + len - 4, ".pak") != 0)
			continue;

		// Verify launch.sh exists inside the pak
		char launch_path[512];
		(void)snprintf(launch_path, sizeof(launch_path), "%s/%s/launch.sh", dir_path, dp->d_name);
		if (!exists(launch_path))
			continue;

		// Extract emu name (strip .pak suffix)
		char emu_name[256];
		strncpy(emu_name, dp->d_name, len - 4);
		emu_name[len - 4] = '\0';

		// Add to cache (NULL value - we only need key presence)
		shput(emu_cache, emu_name, NULL);
		count++;
	}

	closedir(dh);
	return count;
}

int EmuCache_init(const char* paks_path, const char* sdcard_path, const char* platform) {
	// Free existing cache if reinitializing
	EmuCache_free();

	// Initialize with key duplication (stb_ds copies keys)
	sh_new_strdup(emu_cache);

	int total = 0;
	char scan_path[512];

	// Scan shared location: {paks_path}/Emus/
	(void)snprintf(scan_path, sizeof(scan_path), "%s/Emus", paks_path);
	total += scanPakDirectory(scan_path);

	// Scan platform-specific location: {sdcard_path}/Emus/{platform}/
	(void)snprintf(scan_path, sizeof(scan_path), "%s/Emus/%s", sdcard_path, platform);
	total += scanPakDirectory(scan_path);

	return total;
}

int EmuCache_hasEmu(const char* emu_name) {
	if (!emu_cache || !emu_name)
		return 0;

	return shgeti(emu_cache, emu_name) >= 0;
}

void EmuCache_free(void) {
	if (emu_cache) {
		// Keys are freed by shfree when using sh_new_strdup
		// Values are NULL so no need to free them
		shfree(emu_cache);
		emu_cache = NULL;
	}
}

int EmuCache_count(void) {
	if (!emu_cache)
		return 0;
	return (int)shlen(emu_cache);
}
