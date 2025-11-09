/**
 * minui_utils.h - Helper utilities for MinUI launcher
 *
 * Pure logic functions extracted from minui.c for testability:
 * - Alphabetical indexing
 * - Path classification
 *
 * Extracted from minui.c for unit testing.
 */

#ifndef __MINUI_UTILS_H__
#define __MINUI_UTILS_H__

/**
 * Gets alphabetical index character for a string.
 *
 * Returns index 1-26 for strings starting with a-z (case-insensitive).
 * Returns 0 for strings starting with non-letters.
 *
 * Used for L1/R1 quick navigation in file browser.
 *
 * Example:
 *   "Apple" -> 1 (A)
 *   "Zelda" -> 26 (Z)
 *   "007 GoldenEye" -> 0 (non-letter)
 *
 * @param str String to get index for
 * @return Index 0-26 (0=non-letter, 1=A, 2=B, ..., 26=Z)
 */
int MinUI_getIndexChar(char* str);

/**
 * Checks if a path is a top-level console directory.
 *
 * A console directory is one whose parent is ROMS_PATH.
 * Example: "/mnt/SDCARD/Roms/GB" -> true (parent is /mnt/SDCARD/Roms)
 *           "/mnt/SDCARD/Roms/GB/Homebrew" -> false (parent is .../Roms/GB)
 *
 * @param path Path to check
 * @param roms_path The ROMS_PATH constant (e.g., "/mnt/SDCARD/Roms")
 * @return 1 if path is a console directory, 0 otherwise
 */
int MinUI_isConsoleDir(char* path, const char* roms_path);

#endif // __MINUI_UTILS_H__
