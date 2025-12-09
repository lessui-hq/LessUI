/**
 * minarch_internal.h - Internal type definitions shared between minarch modules
 *
 * This header contains struct definitions for internal MinArch types that
 * need to be accessed by multiple modules (minarch.c, minarch_menu.c, etc).
 *
 * These types are implementation details and should not be exposed to
 * external code - use minarch_context.h for public interfaces.
 */

#ifndef MINARCH_INTERNAL_H
#define MINARCH_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "defines.h"
#include "libretro.h"

///////////////////////////////
// Core structure
///////////////////////////////

/**
 * Core structure - Manages the loaded libretro core (.so) and its interface.
 */
struct Core {
	// State
	int initialized; // Core has been initialized
	int need_fullpath; // Core requires file path (not ROM data)

	// Metadata (populated from core)
	const char tag[8]; // Platform tag, e.g., "GBC", "NES"
	const char name[128]; // Core name, e.g., "gambatte", "fceumm"
	const char version[128]; // Core version string
	const char extensions[128]; // Supported file extensions, e.g., "gb|gbc|dmg"

	// Directory paths
	const char config_dir[MAX_PATH]; // Core config directory
	const char states_dir[MAX_PATH]; // Save states directory
	const char saves_dir[MAX_PATH]; // SRAM saves directory
	const char bios_dir[MAX_PATH]; // BIOS files directory

	// Audio/Video parameters
	double fps; // Target frames per second
	double sample_rate; // Audio sample rate in Hz
	double aspect_ratio; // Display aspect ratio

	// Dynamic library
	void* handle; // dlopen() handle to loaded .so file

	// Libretro API function pointers
	void (*init)(void);
	void (*deinit)(void);
	void (*get_system_info)(struct retro_system_info* info);
	void (*get_system_av_info)(struct retro_system_av_info* info);
	void (*set_controller_port_device)(unsigned port, unsigned device);
	void (*reset)(void);
	void (*run)(void);

	// Save state functions
	size_t (*serialize_size)(void);
	bool (*serialize)(void* data, size_t size);
	bool (*unserialize)(const void* data, size_t size);

	// Game management
	bool (*load_game)(const struct retro_game_info* game);
	bool (*load_game_special)(unsigned game_type, const struct retro_game_info* info,
	                          size_t num_info);
	void (*unload_game)(void);

	// Memory access
	unsigned (*get_region)(void);
	void* (*get_memory_data)(unsigned id);
	size_t (*get_memory_size)(unsigned id);

	// Callbacks from core
	retro_audio_buffer_status_callback_t audio_buffer_status;
};

///////////////////////////////
// Game structure
///////////////////////////////

/**
 * Game structure - Represents the currently loaded game/ROM file.
 */
struct Game {
	char path[MAX_PATH]; // Original ROM path
	char name[MAX_PATH]; // Base filename (for save files)
	char m3u_path[MAX_PATH]; // Path to .m3u playlist (multi-disc)
	char tmp_path[MAX_PATH]; // Temp file (extracted from ZIP)
	void* data; // ROM data in memory
	size_t size; // ROM size in bytes
	int is_open; // Successfully loaded flag
};

///////////////////////////////
// Option structures
///////////////////////////////

// Forward declaration for button mappings (defined in minarch_input.h)
typedef struct MinArchButtonMapping MinArchButtonMapping;

// Option types defined in minarch_options.h
#include "minarch_options.h"

///////////////////////////////
// Config structure
///////////////////////////////

/**
 * Config structure - Configuration state for frontend and core options
 */
struct Config {
	char* system_cfg; // system.cfg based on system limitations
	char* default_cfg; // pak.cfg based on platform limitations
	char* user_cfg; // minarch.cfg or game.cfg
	char* device_tag;
	MinArchOptionList frontend; // Frontend settings
	MinArchOptionList core; // Core-specific options
	MinArchButtonMapping* controls; // Button mappings
	MinArchButtonMapping* shortcuts; // Shortcut mappings
	int loaded;
	int initialized;
};

#endif /* MINARCH_INTERNAL_H */
