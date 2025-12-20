/**
 * launcher_navigation.h - Navigation logic for Launcher
 *
 * Provides testable navigation functions using the context pattern.
 * Handles opening ROMs, directories, and applications.
 *
 * Design:
 * - Pure functions compute navigation decisions
 * - Context-aware functions execute navigation using callbacks
 * - All external state access is through the context
 */

#ifndef LAUNCHER_NAVIGATION_H
#define LAUNCHER_NAVIGATION_H

#include "launcher_context.h"
#include "launcher_entry.h"
#include <stdbool.h>

///////////////////////////////
// Navigation action types
///////////////////////////////

/**
 * Types of navigation actions that can be performed.
 */
typedef enum {
	LAUNCHER_NAV_NONE, // No action needed
	LAUNCHER_NAV_OPEN_ROM, // Launch ROM with emulator
	LAUNCHER_NAV_OPEN_PAK, // Launch .pak application
	LAUNCHER_NAV_OPEN_DIR, // Open directory for browsing
	LAUNCHER_NAV_CLOSE_DIR, // Close current directory
	LAUNCHER_NAV_QUIT // Exit launcher
} LauncherNavActionType;

/**
 * Navigation action result.
 * Contains all information needed to execute a navigation action.
 */
typedef struct {
	LauncherNavActionType action;
	char path[512]; // Primary path (ROM, pak, or directory)
	char last_path[512]; // Path for state restoration
	int auto_launch; // For directories: auto-launch contents
	int resume_slot; // For ROMs: save state slot (-1 = none)
} LauncherNavAction;

///////////////////////////////
// Pure navigation logic
///////////////////////////////

/**
 * Determines the navigation action for an entry.
 *
 * This is a pure function - it only examines the entry and returns
 * what action should be taken. Does not modify any state.
 *
 * @param entry Entry to open
 * @param current_path Current directory path (for collection handling)
 * @param collections_path Collections path constant
 * @param out_action Output action structure
 */
void LauncherNav_determineAction(const Entry* entry, const char* current_path,
                                 const char* collections_path, LauncherNavAction* out_action);

/**
 * Determines if a directory should auto-launch its contents.
 *
 * Auto-launch applies when:
 * - Directory contains a .cue file (disc image)
 * - Directory contains a .m3u file (multi-disc playlist)
 *
 * @param dir_path Directory path
 * @param out_launch_path Path to launch (cue or m3u), if auto-launch
 * @param launch_path_size Size of out_launch_path buffer
 * @return true if directory should auto-launch, false for normal browsing
 */
bool LauncherNav_shouldAutoLaunch(const char* dir_path, char* out_launch_path,
                                  int launch_path_size);

/**
 * Builds the ROM launch command.
 *
 * Handles:
 * - Finding the emulator for the ROM
 * - Multi-disc games (m3u playlists)
 * - Resume state (loading saved disc position)
 *
 * @param rom_path Full ROM path
 * @param emu_name Emulator name
 * @param emu_path Full emulator path
 * @param should_resume Whether to resume from save state
 * @param slot_path Save state slot path (if resuming)
 * @param m3u_path M3U path if multi-disc (or NULL)
 * @param userdata_path Userdata path for disc state
 * @param out_cmd Output command buffer
 * @param cmd_size Size of output buffer
 * @param out_sd_path Actual ROM path to launch (may differ for multi-disc)
 * @param sd_path_size Size of out_sd_path buffer
 */
void LauncherNav_buildRomCommand(const char* rom_path, const char* emu_name, const char* emu_path,
                                 bool should_resume, const char* slot_path, const char* m3u_path,
                                 const char* userdata_path, char* out_cmd, int cmd_size,
                                 char* out_sd_path, int sd_path_size);

/**
 * Builds the pak launch command.
 *
 * @param pak_path Full .pak directory path
 * @param out_cmd Output command buffer
 * @param cmd_size Size of output buffer
 */
void LauncherNav_buildPakCommand(const char* pak_path, char* out_cmd, int cmd_size);

///////////////////////////////
// Context-aware navigation
///////////////////////////////

/**
 * Opens an entry using the context.
 *
 * This is the main entry point for navigation. It:
 * 1. Determines what action to take
 * 2. Executes the action using context callbacks
 * 3. Updates state (recents, last path, etc.)
 *
 * @param ctx Launcher context
 * @param entry Entry to open
 */
void LauncherNav_openEntry(LauncherContext* ctx, Entry* entry);

/**
 * Opens a ROM using the context.
 *
 * Handles multi-disc games, resume states, recents tracking.
 *
 * @param ctx Launcher context
 * @param path ROM path
 * @param last Path for state restoration (may differ from path)
 */
void LauncherNav_openRom(LauncherContext* ctx, const char* path, const char* last);

/**
 * Opens a pak application using the context.
 *
 * @param ctx Launcher context
 * @param path Pak directory path
 */
void LauncherNav_openPak(LauncherContext* ctx, const char* path);

/**
 * Opens a directory using the context.
 *
 * @param ctx Launcher context
 * @param path Directory path
 * @param auto_launch Whether to auto-launch contents
 */
void LauncherNav_openDirectory(LauncherContext* ctx, const char* path, int auto_launch);

/**
 * Closes the current directory using the context.
 *
 * @param ctx Launcher context
 */
void LauncherNav_closeDirectory(LauncherContext* ctx);

#endif /* LAUNCHER_NAVIGATION_H */
