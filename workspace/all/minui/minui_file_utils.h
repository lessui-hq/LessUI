/**
 * minui_file_utils.h - File and directory checking utilities for MinUI
 *
 * Provides functions to check for specific files and directories related to ROM management:
 * - Emulator availability (.pak files)
 * - CUE sheets for disc-based games
 * - M3U playlists for multi-disc games
 * - Directory content checking (non-hidden files)
 *
 * Extracted from minui.c for testability with file mocking.
 */

#ifndef __MINUI_FILE_UTILS_H__
#define __MINUI_FILE_UTILS_H__

/**
 * Checks if an emulator exists for a given system.
 *
 * Searches for emulator .pak directories in two locations:
 * 1. {paks_path}/Emus/{emu_name}.pak/launch.sh
 * 2. {sdcard_path}/Emus/{platform}/{emu_name}.pak/launch.sh
 *
 * Example:
 *   hasEmu("gpsp", "/mnt/SDCARD/Paks", "/mnt/SDCARD", "miyoomini")
 *   Looks for:
 *     /mnt/SDCARD/Paks/Emus/gpsp.pak/launch.sh
 *     /mnt/SDCARD/Emus/miyoomini/gpsp.pak/launch.sh
 *
 * @param emu_name Emulator name (e.g., "gpsp", "gambatte")
 * @param paks_path PAKS_PATH constant
 * @param sdcard_path SDCARD_PATH constant
 * @param platform PLATFORM constant
 * @return 1 if emulator exists, 0 otherwise
 */
int MinUI_hasEmu(char* emu_name, const char* paks_path, const char* sdcard_path,
                 const char* platform);

/**
 * Checks if a directory contains a .cue file for disc-based games.
 *
 * The .cue file must be named after the directory itself.
 * Example: /Roms/PS1/Final Fantasy VII/Final Fantasy VII.cue
 *
 * @param dir_path Full path to directory
 * @param cue_path Output buffer for .cue file path (min 256 bytes)
 * @return 1 if .cue file exists, 0 otherwise
 *
 * @note cue_path is always written, even if file doesn't exist
 */
int MinUI_hasCue(char* dir_path, char* cue_path);

/**
 * Checks if a ROM has an associated .m3u playlist for multi-disc games.
 *
 * The .m3u file must be in the parent directory and named after that directory.
 * Example: For /Roms/PS1/Game/disc1.bin, looks for /Roms/PS1/Game.m3u
 *
 * @param rom_path Full path to ROM file
 * @param m3u_path Output buffer for .m3u file path (min 256 bytes)
 * @return 1 if .m3u file exists, 0 otherwise
 *
 * @note m3u_path is always written, even if file doesn't exist
 */
int MinUI_hasM3u(char* rom_path, char* m3u_path);

/**
 * Checks if a directory contains any non-hidden files.
 *
 * Hidden files/directories start with '.' or are named ".." or ".".
 *
 * @param dir_path Full path to directory
 * @return 1 if directory exists and contains non-hidden files, 0 otherwise
 *
 * Example:
 *   MinUI_hasNonHiddenFiles("/mnt/SDCARD/.minui/Collections") → 1 if has .txt files
 *   MinUI_hasNonHiddenFiles("/empty/dir") → 0
 */
int MinUI_hasNonHiddenFiles(const char* dir_path);

#endif // __MINUI_FILE_UTILS_H__
