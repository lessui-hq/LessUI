/**
 * paths_stub.c - Test stub for runtime paths
 *
 * Provides initialized path globals for unit testing.
 * Uses values from tests/support/platform.h (SDCARD_PATH="/tmp/test")
 */

#include "paths.h"

#include <string.h>

// Initialize paths with test values
char g_sdcard_path[PATHS_MAX_LEN] = "/tmp/test";
char g_roms_path[PATHS_MAX_LEN] = "/tmp/test/Roms";
char g_root_system_path[PATHS_MAX_LEN] = "/tmp/test/.system";
char g_system_path[PATHS_MAX_LEN] = "/tmp/test/.system/test";
char g_res_path[PATHS_MAX_LEN] = "/tmp/test/.system/res";
char g_font_path[PATHS_MAX_LEN] = "/tmp/test/.system/res/font.ttf";
char g_userdata_path[PATHS_MAX_LEN] = "/tmp/test/.userdata/test";
char g_shared_userdata_path[PATHS_MAX_LEN] = "/tmp/test/.userdata/shared";
char g_paks_path[PATHS_MAX_LEN] = "/tmp/test/.system/test/paks";
char g_recent_path[PATHS_MAX_LEN] = "/tmp/test/.userdata/shared/.launcher/recent.txt";
char g_simple_mode_path[PATHS_MAX_LEN] = "/tmp/test/.userdata/test/.simple_mode";
char g_auto_resume_path[PATHS_MAX_LEN] = "/tmp/test/.userdata/test/.auto_resume";
char g_faux_recent_path[PATHS_MAX_LEN] = "/tmp/test/Recently Played";
char g_collections_path[PATHS_MAX_LEN] = "/tmp/test/Collections";

static int s_initialized = 1; // Pre-initialized for tests

void Paths_init(void) {
	// No-op for tests - already initialized with test values
}

int Paths_isInitialized(void) {
	return s_initialized;
}
