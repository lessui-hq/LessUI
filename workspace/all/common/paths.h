/**
 * paths.h - Runtime path resolution for dynamic storage locations
 *
 * This module provides runtime-resolved paths that can adapt to different
 * storage locations (e.g., LessOS internal vs external storage).
 *
 * Call Paths_init() early in main() before using any path variables.
 * The paths will use the LESSOS_STORAGE environment variable if set,
 * otherwise fall back to the compile-time SDCARD_PATH default.
 */

#ifndef PATHS_H
#define PATHS_H

#include "defines.h"

// Maximum path length for runtime paths
#define PATHS_MAX_LEN MAX_PATH

/**
 * Runtime path variables.
 * These mirror the compile-time macros in defines.h but are resolved at runtime.
 */
extern char g_sdcard_path[PATHS_MAX_LEN];
extern char g_roms_path[PATHS_MAX_LEN];
extern char g_root_system_path[PATHS_MAX_LEN];
extern char g_system_path[PATHS_MAX_LEN];
extern char g_res_path[PATHS_MAX_LEN];
extern char g_font_path[PATHS_MAX_LEN];
extern char g_userdata_path[PATHS_MAX_LEN];
extern char g_shared_userdata_path[PATHS_MAX_LEN];
extern char g_paks_path[PATHS_MAX_LEN];
extern char g_recent_path[PATHS_MAX_LEN];
extern char g_simple_mode_path[PATHS_MAX_LEN];
extern char g_auto_resume_path[PATHS_MAX_LEN];
extern char g_faux_recent_path[PATHS_MAX_LEN];
extern char g_collections_path[PATHS_MAX_LEN];

/**
 * Initialize runtime paths.
 *
 * Reads the LESSOS_STORAGE environment variable if set, otherwise uses
 * the compile-time SDCARD_PATH default. All derived paths are then built.
 *
 * Must be called early in main() before any paths are used.
 */
void Paths_init(void);

/**
 * Check if paths have been initialized.
 *
 * @return 1 if Paths_init() has been called, 0 otherwise
 */
int Paths_isInitialized(void);

#endif // PATHS_H
