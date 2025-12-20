/**
 * launcher_utils.h - Helper utilities for Launcher launcher
 *
 * Pure logic functions extracted from launcher.c for testability:
 * - Alphabetical indexing
 * - Path classification
 *
 * Extracted from launcher.c for unit testing.
 */

#ifndef __LAUNCHER_UTILS_H__
#define __LAUNCHER_UTILS_H__

/**
 * Gets alphabetical index character for a string.
 *
 * Returns index 1-26 for strings starting with a-z (case-insensitive).
 * Returns 0 for strings starting with non-letters.
 *
 * Used for L1/R1 quick navigation in file browser.
 * When building alphabetical index, pass entry->sort_key (which has
 * leading articles stripped) rather than entry->name to match sort order.
 *
 * Example:
 *   "Apple" -> 1 (A)
 *   "Zelda" -> 26 (Z)
 *   "007 GoldenEye" -> 0 (non-letter)
 *
 * @param str String to get index for (typically sort_key for Entry indexing)
 * @return Index 0-26 (0=non-letter, 1=A, 2=B, ..., 26=Z)
 */
int Launcher_getIndexChar(char* str);

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
int Launcher_isConsoleDir(char* path, const char* roms_path);

#endif // __LAUNCHER_UTILS_H__
