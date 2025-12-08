/**
 * minui_launcher.h - ROM and PAK launcher utilities
 *
 * Provides functions to construct shell commands for launching
 * ROMs with emulators and PAKs (application packages).
 *
 * Designed for testability - command construction is separated from
 * file I/O and global state manipulation.
 *
 * Extracted from minui.c.
 */

#ifndef __MINUI_LAUNCHER_H__
#define __MINUI_LAUNCHER_H__

#include <stddef.h>

/**
 * Maximum command buffer size for launcher commands.
 */
#define MINUI_MAX_CMD_SIZE 1024

/**
 * Escapes single quotes in a string for shell command safety.
 *
 * Replaces ' with '\'' which safely handles quotes in bash strings.
 * Example: "it's" becomes "it'\''s"
 *
 * @param str String to escape (modified in place)
 * @return The modified string (same pointer as input)
 *
 * @note String must have enough space for escaped result.
 *       In worst case, each quote becomes 4 characters.
 */
char* MinUI_escapeSingleQuotes(char* str);

/**
 * Replaces all occurrences of a substring in a string.
 *
 * Modifies the string in place. Handles overlapping replacements
 * by recursing after each replacement.
 *
 * @param line String to modify (modified in place)
 * @param search Substring to find
 * @param replace Replacement string
 * @return Number of replacements made
 *
 * @note String must have enough space for replaced result.
 */
int MinUI_replaceString(char* line, const char* search, const char* replace);

/**
 * Constructs command to launch a PAK (application package).
 *
 * Format: '<pak_path>/launch.sh'
 *
 * @param cmd Output buffer for command (min MINUI_MAX_CMD_SIZE bytes)
 * @param cmd_size Size of command buffer
 * @param pak_path Path to .pak directory (will be escaped)
 * @return Length of generated command, 0 on error
 *
 * @note pak_path is modified in place (quotes escaped)
 */
int MinUI_buildPakCommand(char* cmd, size_t cmd_size, char* pak_path);

/**
 * Constructs command to launch a ROM with its emulator.
 *
 * Format: '<emu_path>' '<rom_path>'
 *
 * @param cmd Output buffer for command (min MINUI_MAX_CMD_SIZE bytes)
 * @param cmd_size Size of command buffer
 * @param emu_path Path to emulator launch script (will be escaped)
 * @param rom_path Path to ROM file (will be escaped)
 * @return Length of generated command, 0 on error
 *
 * @note Both paths are modified in place (quotes escaped)
 */
int MinUI_buildRomCommand(char* cmd, size_t cmd_size, char* emu_path, char* rom_path);

/**
 * Writes a command string to the command queue file.
 *
 * The command queue file (/tmp/next) is watched by the system's
 * init script and executed after the current program exits.
 *
 * @param filepath Path to command queue file (e.g., "/tmp/next")
 * @param cmd Command to write
 * @return 0 on success, -1 on error
 */
int MinUI_queueCommand(const char* filepath, const char* cmd);

/**
 * Checks if a path is under the ROMs directory.
 *
 * Used to determine if a launch should be added to recent list.
 *
 * @param path Path to check
 * @param roms_path Base ROMs path (e.g., "/mnt/SDCARD/Roms")
 * @return 1 if path is under roms_path, 0 otherwise
 */
int MinUI_isRomsPath(const char* path, const char* roms_path);

#endif // __MINUI_LAUNCHER_H__
