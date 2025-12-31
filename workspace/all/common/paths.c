/**
 * paths.c - Runtime path resolution for dynamic storage locations
 *
 * Initializes all runtime paths based on environment variables or defaults.
 */

#include "paths.h"
#include "log.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Runtime path storage
char g_sdcard_path[PATHS_MAX_LEN];
char g_roms_path[PATHS_MAX_LEN];
char g_root_system_path[PATHS_MAX_LEN];
char g_system_path[PATHS_MAX_LEN];
char g_res_path[PATHS_MAX_LEN];
char g_font_path[PATHS_MAX_LEN];
char g_userdata_path[PATHS_MAX_LEN];
char g_shared_userdata_path[PATHS_MAX_LEN];
char g_paks_path[PATHS_MAX_LEN];
char g_recent_path[PATHS_MAX_LEN];
char g_simple_mode_path[PATHS_MAX_LEN];
char g_auto_resume_path[PATHS_MAX_LEN];
char g_faux_recent_path[PATHS_MAX_LEN];
char g_collections_path[PATHS_MAX_LEN];

static int s_initialized = 0;

void Paths_init(void) {
	if (s_initialized) {
		return;
	}

	// Check for LessOS storage environment variable
	const char* env_storage = getenv("LESSOS_STORAGE");
	if (env_storage && env_storage[0] != '\0') {
		strncpy(g_sdcard_path, env_storage, PATHS_MAX_LEN - 1);
		g_sdcard_path[PATHS_MAX_LEN - 1] = '\0';
		LOG_info("Paths_init: Using LESSOS_STORAGE=%s", g_sdcard_path);
	} else {
		// Fall back to compile-time default
		strncpy(g_sdcard_path, SDCARD_PATH, PATHS_MAX_LEN - 1);
		g_sdcard_path[PATHS_MAX_LEN - 1] = '\0';
		LOG_info("Paths_init: Using default SDCARD_PATH=%s", g_sdcard_path);
	}

	// Build all derived paths
	(void)snprintf(g_roms_path, PATHS_MAX_LEN, "%s/Roms", g_sdcard_path);
	(void)snprintf(g_root_system_path, PATHS_MAX_LEN, "%s/.system/", g_sdcard_path);
	(void)snprintf(g_system_path, PATHS_MAX_LEN, "%s/.system/%s", g_sdcard_path, PLATFORM);
	(void)snprintf(g_res_path, PATHS_MAX_LEN, "%s/.system/res", g_sdcard_path);
	(void)snprintf(g_font_path, PATHS_MAX_LEN, "%s/.system/res/InterTight-Bold.ttf", g_sdcard_path);
	(void)snprintf(g_userdata_path, PATHS_MAX_LEN, "%s/.userdata/%s", g_sdcard_path, PLATFORM);
	(void)snprintf(g_shared_userdata_path, PATHS_MAX_LEN, "%s/.userdata/shared", g_sdcard_path);
	(void)snprintf(g_paks_path, PATHS_MAX_LEN, "%s/paks", g_system_path);
	(void)snprintf(g_recent_path, PATHS_MAX_LEN, "%s/.launcher/recent.txt", g_shared_userdata_path);
	(void)snprintf(g_simple_mode_path, PATHS_MAX_LEN, "%s/enable-simple-mode",
	               g_shared_userdata_path);
	(void)snprintf(g_auto_resume_path, PATHS_MAX_LEN, "%s/.launcher/auto_resume.txt",
	               g_shared_userdata_path);
	(void)snprintf(g_faux_recent_path, PATHS_MAX_LEN, "%s/Recently Played", g_sdcard_path);
	(void)snprintf(g_collections_path, PATHS_MAX_LEN, "%s/Collections", g_sdcard_path);

	LOG_debug("Paths_init: g_roms_path=%s", g_roms_path);
	LOG_debug("Paths_init: g_system_path=%s", g_system_path);
	LOG_debug("Paths_init: g_res_path=%s", g_res_path);
	LOG_debug("Paths_init: g_paks_path=%s", g_paks_path);

	s_initialized = 1;
}

int Paths_isInitialized(void) {
	return s_initialized;
}
