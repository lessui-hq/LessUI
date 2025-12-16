/**
 * launcher_state.h - Launcher state persistence utilities
 *
 * Provides functions for saving/restoring launcher navigation state
 * and resume path generation.
 *
 * Designed for testability with explicit parameters.
 * Extracted from launcher.c.
 */

#ifndef __LAUNCHER_STATE_H__
#define __LAUNCHER_STATE_H__

#include "stb_ds.h"
#include <stdbool.h>

/**
 * Maximum path length for state paths.
 */
#define LAUNCHER_STATE_MAX_PATH 512

/**
 * Path component for path reconstruction.
 * Used with stb_ds dynamic arrays.
 */
typedef struct {
	char path[LAUNCHER_STATE_MAX_PATH];
} LauncherPathComponent;

/**
 * LauncherPathStack is a stb_ds dynamic array of LauncherPathComponent.
 * Usage: LauncherPathComponent* stack = NULL;
 *        arrpush(stack, component);
 *        arrlen(stack)  // count
 *        arrcap(stack)  // capacity
 *        arrfree(stack)
 */
typedef LauncherPathComponent* LauncherPathStack;

/**
 * Pushes a path onto the stack.
 *
 * @param stack_ptr Pointer to stack array (may be reallocated)
 * @param path Path to push (copied)
 * @return true on success, false on failure
 */
bool LauncherPathStack_push(LauncherPathStack* stack_ptr, const char* path);

/**
 * Pops a path from the stack.
 *
 * @param stack_ptr Pointer to stack array
 * @param out_path Buffer to receive path (at least LAUNCHER_STATE_MAX_PATH bytes)
 * @return true if popped, false if stack was empty
 */
bool LauncherPathStack_pop(LauncherPathStack* stack_ptr, char* out_path);

/**
 * Decomposes a full path into a stack of parent directories.
 *
 * For "/mnt/SDCARD/Roms/GB/game.gb", creates stack with:
 * - "/mnt/SDCARD/Roms/GB/game.gb" (bottom)
 * - "/mnt/SDCARD/Roms/GB"
 * - "/mnt/SDCARD/Roms"
 * - "/mnt/SDCARD" (top, used as root/stop point)
 *
 * @param full_path Path to decompose
 * @param root_path Root path to stop at (not included in stack)
 * @return Stack of path components (caller must arrfree), or NULL on error
 */
LauncherPathStack LauncherState_decomposePath(const char* full_path, const char* root_path);

/**
 * Extracts the filename from a full path.
 *
 * @param full_path Full path including filename
 * @param out_filename Buffer to receive filename (at least LAUNCHER_STATE_MAX_PATH bytes)
 */
void LauncherState_extractFilename(const char* full_path, char* out_filename);

/**
 * Checks if a path looks like a collated ROM folder.
 *
 * Collated folders end with ") " pattern and contain a platform suffix
 * like "(USA)" or "(Japan)". Used to match equivalent entries.
 *
 * @param path Path to check
 * @return true if path appears to be collated
 */
bool LauncherState_isCollatedPath(const char* path);

/**
 * Extracts the collation prefix from a path.
 *
 * For "/Roms/Game Boy (USA)", extracts "/Roms/Game Boy ("
 * This allows matching "Game Boy (USA)" with "Game Boy (Japan)".
 *
 * @param path Path to extract from
 * @param out_prefix Buffer to receive prefix (at least LAUNCHER_STATE_MAX_PATH bytes)
 * @return true if prefix was extracted, false if not a collated path
 */
bool LauncherState_getCollationPrefix(const char* path, char* out_prefix);

/**
 * Generates the resume slot path for a ROM.
 *
 * Resume slot path format: <userdata>/.launcher/<emu>/<romname>.ext.txt
 *
 * @param rom_path Full ROM path
 * @param userdata_path Base userdata path
 * @param emu_name Emulator name
 * @param out_path Buffer to receive slot path (at least LAUNCHER_STATE_MAX_PATH bytes)
 */
void LauncherState_getResumeSlotPath(const char* rom_path, const char* userdata_path,
                                     const char* emu_name, char* out_path);

/**
 * Generates the auto-resume command line.
 *
 * @param emu_path Full path to emulator
 * @param rom_path Full path to ROM
 * @param out_cmd Buffer to receive command (at least LAUNCHER_STATE_MAX_PATH * 2 bytes)
 */
void LauncherState_buildResumeCommand(const char* emu_path, const char* rom_path, char* out_cmd);

/**
 * Checks if a path is under the recents pseudo-path.
 *
 * @param path Path to check
 * @param recents_path Recents pseudo-path constant
 * @return true if path represents the recents list
 */
bool LauncherState_isRecentsPath(const char* path, const char* recents_path);

/**
 * Validates a saved state path.
 *
 * @param path Path to validate
 * @param sd_path SD card base path
 * @return true if path is valid (exists and is under SD card)
 */
bool LauncherState_validatePath(const char* path, const char* sd_path);

/**
 * Converts a relative path to an absolute SD card path.
 *
 * @param relative_path Relative path (starting with /)
 * @param sd_path SD card base path
 * @param out_path Buffer to receive absolute path
 */
void LauncherState_makeAbsolutePath(const char* relative_path, const char* sd_path, char* out_path);

/**
 * Escapes single quotes in a path for shell command construction.
 *
 * Replaces ' with '\'' for safe shell quoting.
 *
 * @param input Path to escape
 * @param out_escaped Buffer to receive escaped path (should be 4x input length)
 * @param out_size Size of output buffer
 */
void LauncherState_escapeQuotes(const char* input, char* out_escaped, int out_size);

#endif // __LAUNCHER_STATE_H__
