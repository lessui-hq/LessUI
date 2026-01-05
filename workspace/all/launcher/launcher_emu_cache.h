/**
 * launcher_emu_cache.h - Emulator availability cache for Launcher
 *
 * Caches available emulator paks at startup for O(1) lookups during
 * root menu generation. Eliminates ~100 syscalls per root menu load.
 *
 * Usage:
 *   EmuCache_init(paks_path, sdcard_path, platform);
 *   if (EmuCache_hasEmu("gpsp")) { ... }
 *   EmuCache_free();
 */

#ifndef LAUNCHER_EMU_CACHE_H
#define LAUNCHER_EMU_CACHE_H

/**
 * Initializes the emulator cache by scanning pak directories.
 *
 * Scans two locations for .pak directories:
 * 1. {paks_path}/Emus/{name}.pak (shared emus)
 * 2. {sdcard_path}/Emus/{platform}/{name}.pak (platform-specific emus)
 *
 * After initialization, EmuCache_hasEmu() provides O(1) lookups.
 *
 * @param paks_path Path to shared paks (e.g., PAKS_PATH)
 * @param sdcard_path Path to SD card root (e.g., SDCARD_PATH)
 * @param platform Platform identifier (e.g., "miyoomini")
 * @return Number of emulators found (0 if directories don't exist or are empty)
 */
int EmuCache_init(const char* paks_path, const char* sdcard_path, const char* platform);

/**
 * Checks if an emulator is available.
 *
 * Must call EmuCache_init() first.
 *
 * @param emu_name Emulator name (e.g., "gpsp", "gambatte")
 * @return 1 if emulator pak exists, 0 otherwise
 */
int EmuCache_hasEmu(const char* emu_name);

/**
 * Frees the emulator cache.
 *
 * Safe to call multiple times or before init.
 */
void EmuCache_free(void);

/**
 * Returns the number of cached emulators.
 *
 * @return Count of emulators in cache, or 0 if not initialized
 */
int EmuCache_count(void);

#endif // LAUNCHER_EMU_CACHE_H
