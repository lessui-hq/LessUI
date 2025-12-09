/**
 * minarch_config.h - Configuration option utilities for MinArch
 *
 * Provides pure utility functions for config file parsing and
 * option value manipulation. These functions have minimal dependencies
 * and can be tested in isolation.
 *
 * For config path generation, see minarch_paths.h (MinArchConfig_getPath).
 */

#ifndef MINARCH_CONFIG_H
#define MINARCH_CONFIG_H

// MinArchConfig_getPath is in minarch_paths.h
#include "minarch_paths.h"

/**
 * Maps option keys to custom display names.
 *
 * Some cores use cryptic option keys that need friendlier names for display.
 * Returns the mapped name if found, otherwise returns the original name.
 *
 * Current mappings:
 * - "pcsx_rearmed_analog_combo" -> "DualShock Toggle Combo"
 *
 * @param key Option key to look up
 * @param default_name Fallback name if no mapping exists
 * @return Display name (either mapped or default_name)
 *
 * @example
 *   MinArchConfig_getOptionDisplayName("pcsx_rearmed_analog_combo", "Unknown")
 *   // Returns: "DualShock Toggle Combo"
 */
const char* MinArchConfig_getOptionDisplayName(const char* key, const char* default_name);

/**
 * Extracts a value from a configuration string.
 *
 * Searches for lines matching "key = value" pattern and extracts the value.
 * Optionally detects "locked" values marked with a `-` prefix before the key.
 *
 * Config format:
 * - Normal: "key = value\n"
 * - Locked: "-key = value\n" (sets *lock to 1 if lock pointer provided)
 *
 * @param cfg Configuration string to search
 * @param key Key to find (will match "key = " pattern)
 * @param out_value Output buffer for value (256 bytes max)
 * @param lock Optional pointer to receive lock status (1 if locked, unchanged otherwise)
 * @return 1 if key found, 0 if not found
 *
 * @example
 *   char value[256];
 *   int locked = 0;
 *   MinArchConfig_getValue("scaling = native\n-vsync = on\n", "vsync", value, &locked);
 *   // value = "on", locked = 1
 */
int MinArchConfig_getValue(const char* cfg, const char* key, char* out_value, int* lock);

/**
 * Configuration load state enum.
 *
 * Tracks which configuration level is currently loaded:
 * - MINARCH_CONFIG_NONE: Using built-in defaults
 * - MINARCH_CONFIG_CONSOLE: Using console-level config (e.g., /userdata/GB/minarch.cfg)
 * - MINARCH_CONFIG_GAME: Using game-specific config (e.g., /userdata/GB/Tetris.cfg)
 */
typedef enum {
	MINARCH_CONFIG_NONE = 0,
	MINARCH_CONFIG_CONSOLE = 1,
	MINARCH_CONFIG_GAME = 2
} MinArchConfigState;

/**
 * Returns a human-readable description of the current config state.
 *
 * @param state Current configuration load state
 * @return Description string (static, do not free)
 *
 * @example
 *   MinArchConfig_getStateDesc(MINARCH_CONFIG_CONSOLE)
 *   // Returns: "Using console config."
 */
const char* MinArchConfig_getStateDesc(MinArchConfigState state);

#endif /* MINARCH_CONFIG_H */
