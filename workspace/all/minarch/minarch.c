/**
 * minarch.c - MinUI Libretro Frontend
 *
 * MinArch is a lightweight, single-purpose libretro frontend that loads and runs
 * retro game emulator cores. It provides essential features for retro gaming:
 *
 * Core Features:
 * - Loads libretro cores (.so files) dynamically at runtime
 * - Manages game loading (ROM files, including .zip extraction)
 * - Save state system with auto-resume on slot 9
 * - In-game menu for settings, save states, and disc changing
 * - Video scaling with multiple modes (native, aspect, fullscreen, cropped)
 * - Audio buffering and synchronization
 * - Input mapping and controller configuration
 * - SRAM/battery save management
 * - RTC (real-time clock) data persistence
 * - Multi-disc game support via .m3u playlists
 *
 * Architecture:
 * - Single-threaded by default, with optional threaded video mode
 * - Uses libretro callback system for core communication
 * - Platform-agnostic through SDL and platform.h abstraction
 * - Integrates with MinUI launcher via file-based IPC
 *
 * Save State System:
 * - Slot 0-8: Manual save states accessible via menu
 * - Slot 9: Auto-save slot - automatically saved on quit, loaded on start
 * - States stored in /mnt/SDCARD/.userdata/<platform>/<core>/
 *
 * Video Pipeline:
 * - Core renders to buffer via video_refresh_callback
 * - Scaler applies aspect ratio correction and sharpness filters
 * - Output blitted to SDL screen surface
 * - Optional vsync for tear-free rendering
 *
 * @note This file is platform-independent - platform-specific behavior
 *       is handled through api.h (GFX_*, SND_*, PAD_*, PWR_* functions)
 */

#include <msettings.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#include "api.h"
#include "defines.h"
#include "frame_pacer.h"
#include "libretro.h"
#include "minarch_archive.h"
#include "minarch_config.h"
#include "minarch_context.h"
#include "minarch_core.h"
#include "minarch_cpu.h"
#include "minarch_env.h"
#include "minarch_game.h"
#include "minarch_input.h"
#include "minarch_internal.h"
#include "minarch_mappings.h"
#include "minarch_memory.h"
#include "minarch_menu.h"
#include "minarch_menu_types.h"
#include "minarch_paths.h"
#include "minarch_rotation.h"
#include "minarch_scaler.h"
#include "minarch_state.h"
#include "minarch_video_convert.h"
#include "minui_file_utils.h"
#include "scaler.h"
#include "utils.h"

///////////////////////////////////////
// Global State
///////////////////////////////////////

// Video
static SDL_Surface* screen; // Main screen surface (managed by platform API)

// Application State
static int quit = 0; // Set to 1 to exit main loop
static int show_menu = 0; // Set to 1 to display in-game menu
static int simple_mode = 0; // Simplified interface mode (fewer options)
static int input_polled_this_frame = 0; // Tracks if core called input_poll_callback
static int toggled_ff_on = 0; // Fast-forward toggle state (for TOGGLE_FF vs HOLD_FF interaction)

// Fatal error handling - detail shown when game fails to load
static char fatal_error_detail[512] = {0};
static int capturing_core_errors = 0; // Flag to enable capture during load

// Segfault recovery during core loading
static sigjmp_buf segfault_jmp;
static volatile sig_atomic_t in_core_load = 0;

// Video geometry state for dynamic updates (type defined in minarch_env.h)
static MinArchVideoState video_state;

// Video Scaling Modes - defined in minarch_mappings.h

///////////////////////////////////////
// Frontend Configuration
///////////////////////////////////////

// Video Settings
static int screen_scaling = MINARCH_SCALE_ASPECT; // Default to aspect-ratio preserving
static int screen_sharpness = SHARPNESS_SOFT; // Bilinear filtering by default
static int screen_effect = EFFECT_NONE; // No scanlines or grid effects

/**
 * Core Pixel Format
 *
 * Tracks the pixel format the current core outputs. Our display hardware uses RGB565
 * (16-bit color), so non-native formats require real-time conversion.
 *
 * Supported formats (from libretro.h):
 *  - RETRO_PIXEL_FORMAT_0RGB1555 (0): Legacy 15-bit, 1 unused bit. Default if core
 *    doesn't call SET_PIXEL_FORMAT. Used by some older arcade cores (mame2003+).
 *    Conversion: Extract 5-bit R/G/B, expand G to 6 bits, pack to RGB565.
 *
 *  - RETRO_PIXEL_FORMAT_XRGB8888 (1): 32-bit with unused alpha byte.
 *    Used by PlayStation (PCSX ReARMed), Neo Geo (FBNeo), modern cores.
 *    Conversion: Extract top 5/6/5 bits per channel, pack to RGB565.
 *
 *  - RETRO_PIXEL_FORMAT_RGB565 (2): Native 16-bit format - no conversion needed.
 *    Most cores use this. Recommended format per libretro spec.
 *
 * Performance: NEON-optimized conversion adds ~0.3-0.5ms per frame.
 * Set automatically by RETRO_ENVIRONMENT_SET_PIXEL_FORMAT callback.
 */
static enum retro_pixel_format pixel_format =
    RETRO_PIXEL_FORMAT_0RGB1555; // Default per libretro spec

// Performance Settings
static int show_debug = 0; // Display FPS/CPU usage overlay
static int max_ff_speed = 3; // Fast-forward speed (0=2x, 3=4x)
static int fast_forward = 0; // Currently fast-forwarding
static int overclock = 3; // CPU speed (0=powersave, 1=normal, 2=performance, 3=auto)

// Auto CPU Scaling State (when overclock == 3)
// Uses frame timing (core.run() execution time) to dynamically adjust CPU speed.
// State and config are managed via minarch_cpu.h structs for testability.
static MinArchCPUState auto_cpu_state;
static MinArchCPUConfig auto_cpu_config;
static uint64_t auto_cpu_last_frame_start = 0; // For measuring core.run() time

// Frame Pacing State
// Decouples emulation from display refresh for non-60Hz displays (e.g., M17 @ 72Hz).
// See frame_pacer.h for algorithm details.
static FramePacer frame_pacer;

// Background thread for applying CPU changes without blocking main loop
static pthread_t auto_cpu_thread;
static pthread_mutex_t auto_cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static int auto_cpu_thread_running = 0;

// Input Settings
static int has_custom_controllers = 0; // Custom controller mappings defined
static int gamepad_type = 0; // Index in minarch_gamepad_labels/minarch_gamepad_values

// Device Dimensions
// These are no longer constants as of the RG CubeXX (rotatable display)
static int DEVICE_WIDTH = 0; // Screen width in pixels
static int DEVICE_HEIGHT = 0; // Screen height in pixels
static int DEVICE_PITCH = 0; // Screen pitch in bytes

GFX_Renderer renderer; // Platform-specific renderer handle


///////////////////////////////////////
// Libretro Core Interface
///////////////////////////////////////

// Core instance (struct defined in minarch_internal.h)
static struct Core core;

///////////////////////////////////////
// Game Management
///////////////////////////////////////

// Game instance (struct defined in minarch_internal.h)
static struct Game game;

/**
 * Sets a fatal error message for display when game fails to load.
 * Title is always "Game failed to load" - only the detail varies.
 */
static void setFatalError(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	(void)vsnprintf(fatal_error_detail, sizeof(fatal_error_detail), fmt, args);
	va_end(args);
}

/**
 * Opens and prepares a game for loading into the core.
 *
 * Handles multiple scenarios:
 * 1. Archive files (.zip, .7z): Extracts first matching ROM to /tmp via 7z binary
 * 2. Multi-disc: Detects and stores .m3u playlist path
 * 3. Memory loading: Reads entire ROM into memory for cores that need it
 *
 * @param path Full path to ROM file or archive
 *
 * @note Sets game.is_open = 1 on success, 0 on failure
 * @note For archives, extracts to /tmp/minarch-XXXXXX/ (deleted on close)
 */
static void Game_open(char* path) {
	LOG_info("Game_open");
	memset(&game, 0, sizeof(game));

	safe_strcpy(game.path, path, sizeof(game.path));
	safe_strcpy(game.name, strrchr(path, '/') + 1, sizeof(game.name));

	// Handle archive files (.zip, .7z)
	if (MinArchArchive_isArchive(game.path)) {
		LOG_info("is archive file");
		char exts[128];
		char* extensions[MINARCH_MAX_EXTENSIONS];
		SAFE_STRCPY(exts, core.extensions);
		MinArchGame_parseExtensions(exts, extensions, MINARCH_MAX_EXTENSIONS);

		// Check if core supports this specific archive format natively.
		// Cores like FBNeo/MAME can handle archives directly (block_extract=true),
		// but only for formats they explicitly list in their extensions.
		const char* archive_ext = NULL;
		if (suffixMatch(".zip", game.path))
			archive_ext = "zip";
		else if (suffixMatch(".7z", game.path))
			archive_ext = "7z";

		bool core_handles_this_archive = archive_ext && strArrayContains(extensions, archive_ext);

		if (!core_handles_this_archive) {
			int result =
			    MinArchArchive_extract(game.path, extensions, game.tmp_path, sizeof(game.tmp_path));
			if (result != MINARCH_ARCHIVE_OK) {
				if (result == MINARCH_ARCHIVE_ERR_NO_MATCH) {
					// Build extension list for user - this is specific and actionable
					char ext_list[256] = {0};
					size_t pos = 0;
					for (int i = 0; extensions[i] && pos < sizeof(ext_list) - 10; i++) {
						if (i > 0)
							pos += snprintf(ext_list + pos, sizeof(ext_list) - pos, ", ");
						pos +=
						    snprintf(ext_list + pos, sizeof(ext_list) - pos, ".%s", extensions[i]);
					}
					setFatalError("No compatible files found in archive\nExpected: %s", ext_list);
				} else {
					setFatalError("Failed to extract archive");
				}
				LOG_error("Failed to extract archive: %s (error %d)", game.path, result);
				return;
			}
		}
	}

	// some cores handle opening files themselves, eg. pcsx_rearmed
	// if the frontend tries to load a 500MB file itself bad things happen
	if (!core.need_fullpath) {
		path = game.tmp_path[0] == '\0' ? game.path : game.tmp_path;

		FILE* file = fopen(path, "r");
		if (file == NULL) {
			setFatalError("Could not open ROM file\n%s", strerror(errno));
			LOG_error("Error opening game: %s\n\t%s", path, strerror(errno));
			return;
		}

		(void)fseek(file, 0, SEEK_END);
		game.size = ftell(file);

		(void)fseek(file, 0, SEEK_SET);
		game.data = malloc(game.size);
		if (game.data == NULL) {
			setFatalError("Not enough memory to load ROM\nFile size: %ld bytes", (long)game.size);
			LOG_error("Couldn't allocate memory for file: %s", path);
			(void)fclose(file); // Game file opened for reading
			return;
		}

		(void)fread(game.data, sizeof(uint8_t), game.size, file);

		(void)fclose(file); // Game file opened for reading
	}

	// m3u-based?
	char m3u_path[MAX_PATH];
	if (MinArchGame_detectM3uPath(game.path, m3u_path, sizeof(m3u_path))) {
		SAFE_STRCPY(game.m3u_path, m3u_path);
		safe_strcpy(game.name, strrchr(m3u_path, '/') + 1, sizeof(game.name));
	}

	game.is_open = 1;
}

/**
 * Closes the current game and frees resources.
 *
 * Cleans up:
 * - ROM data memory (if allocated)
 * - Temporary extracted files
 * - Rumble state
 */
static void Game_close(void) {
	if (game.data)
		free(game.data);
	if (game.tmp_path[0]) {
		// Remove extracted file and temp directory
		(void)remove(game.tmp_path);
		// Extract directory path and remove it
		char dir_path[MAX_PATH];
		SAFE_STRCPY(dir_path, game.tmp_path);
		char* last_slash = strrchr(dir_path, '/');
		if (last_slash) {
			*last_slash = '\0';
			(void)rmdir(dir_path);
		}
	}
	game.is_open = 0;
	VIB_setStrength(0); // Ensure rumble is disabled
}

///////////////////////////////////////
// Multi-Disc Support
///////////////////////////////////////

static struct retro_disk_control_ext_callback disk_control_ext;

/**
 * Changes the active disc for multi-disc games.
 *
 * Used for games like PSX multi-disc titles. Closes current game,
 * opens new disc image, and notifies core via disk_control_ext.
 *
 * @param path Full path to new disc image
 *
 * @note Writes path to CHANGE_DISC_PATH for MinUI launcher integration
 * @note Skips if path is same as current or doesn't exist
 */
void Game_changeDisc(char* path) {
	if (exactMatch(game.path, path) || !exists(path))
		return;

	Game_close();
	Game_open(path);

	struct retro_game_info game_info = {};
	game_info.path = game.path;
	game_info.data = game.data;
	game_info.size = game.size;

	disk_control_ext.replace_image_index(0, &game_info);
	putFile(CHANGE_DISC_PATH, path); // MinUI still needs to know this to update recents.txt
}

///////////////////////////////////////
// SRAM (Battery Save) Management
///////////////////////////////////////
// Handles persistent save RAM for games with battery-backed saves
// (e.g., Pokémon, Zelda, RPGs). Stored as .sav files in /mnt/SDCARD/Saves/<platform>/

static void SRAM_getPath(char* filename) {
	(void)sprintf(filename, "%s/%s.sav", core.saves_dir, game.name);
}

/**
 * Loads battery-backed save RAM from disk into core memory.
 *
 * Called after loading a game. Restores in-game progress for games
 * with battery saves (e.g., save files in RPGs).
 *
 * @note Silently skips if core doesn't support SRAM or file doesn't exist
 */
static void SRAM_read(void) {
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	LOG_debug("sav path (read): %s", filename);

	MinArchMemoryResult result =
	    MinArchMemory_readSRAM(filename, core.get_memory_size, core.get_memory_data);
	if (result != MINARCH_MEM_OK && result != MINARCH_MEM_FILE_NOT_FOUND &&
	    result != MINARCH_MEM_NO_SUPPORT) {
		LOG_error("Error reading SRAM: %s", MinArchMemory_resultString(result));
	}
}

/**
 * Writes battery-backed save RAM from core memory to disk.
 *
 * Called when unloading a game. Persists in-game save data so it
 * can be restored on next launch. Calls sync() to ensure data is
 * flushed to SD card.
 *
 * @note Silently skips if core doesn't support SRAM
 */
void SRAM_write(void) {
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	LOG_debug("sav path (write): %s", filename);

	MinArchMemoryResult result =
	    MinArchMemory_writeSRAM(filename, core.get_memory_size, core.get_memory_data);
	if (result != MINARCH_MEM_OK && result != MINARCH_MEM_NO_SUPPORT) {
		LOG_error("Error writing SRAM: %s", MinArchMemory_resultString(result));
	}

	sync();
}

///////////////////////////////////////
// RTC (Real-Time Clock) Management
///////////////////////////////////////
// Some games track real-world time (e.g., Pokémon day/night cycle).
// This data is stored separately from SRAM as .rtc files.

static void RTC_getPath(char* filename) {
	(void)sprintf(filename, "%s/%s.rtc", core.saves_dir, game.name);
}

/**
 * Loads real-time clock data from disk into core memory.
 *
 * @note Silently skips if core doesn't support RTC or file doesn't exist
 */
static void RTC_read(void) {
	char filename[MAX_PATH];
	RTC_getPath(filename);
	LOG_debug("rtc path (read): %s", filename);

	MinArchMemoryResult result =
	    MinArchMemory_readRTC(filename, core.get_memory_size, core.get_memory_data);
	if (result != MINARCH_MEM_OK && result != MINARCH_MEM_FILE_NOT_FOUND &&
	    result != MINARCH_MEM_NO_SUPPORT) {
		LOG_error("Error reading RTC: %s", MinArchMemory_resultString(result));
	}
}

/**
 * Writes real-time clock data from core memory to disk.
 *
 * @note Silently skips if core doesn't support RTC
 */
void RTC_write(void) {
	char filename[MAX_PATH];
	RTC_getPath(filename);
	LOG_debug("rtc path (write): %s", filename);

	MinArchMemoryResult result =
	    MinArchMemory_writeRTC(filename, core.get_memory_size, core.get_memory_data);
	if (result != MINARCH_MEM_OK && result != MINARCH_MEM_NO_SUPPORT) {
		LOG_error("Error writing RTC: %s", MinArchMemory_resultString(result));
	}

	sync();
}

///////////////////////////////////////
// Save State System
///////////////////////////////////////
// MinArch provides 10 save state slots (0-9):
// - Slots 0-8: Manual save states accessible via in-game menu
// - Slot 9: Special "auto-resume" slot - automatically saved on quit,
//           loaded on startup for seamless game resumption
//
// Save states are complete snapshots of emulator state (RAM, registers, etc.)
// stored in /mnt/SDCARD/.userdata/<arch>/<platform>-<core>/

static int state_slot = 0; // Currently selected slot (0-9)

void State_getPath(char* filename) {
	(void)sprintf(filename, "%s/%s.st%i", core.states_dir, game.name, state_slot);
}

/**
 * Loads a save state from disk into the core.
 *
 * Reads the state file for the current slot and restores emulator state.
 * Temporarily disables fast-forward during load to avoid audio glitches.
 *
 * @note Based on picoarch implementation
 * @note Silently fails if state file doesn't exist or core doesn't support states
 * @note Uses gzopen for compressed state files
 */
void State_read(void) {
	int was_ff = fast_forward;
	fast_forward = 0;

	char filename[MAX_PATH];
	State_getPath(filename);

	MinArchStateCore state_core = {.serialize_size = core.serialize_size,
	                               .serialize = core.serialize,
	                               .unserialize = core.unserialize};

	MinArchStateResult result = MinArchState_read(filename, &state_core);

	if (result != MINARCH_STATE_OK && result != MINARCH_STATE_NO_SUPPORT) {
		// slot 8 is a default state in MiniUI and may not exist, that's okay
		if (result != MINARCH_STATE_FILE_NOT_FOUND || state_slot != 8) {
			LOG_error("Error reading state: %s (%s)", filename, MinArchState_resultString(result));
		}
	}

	fast_forward = was_ff;
}

/**
 * Saves current emulator state to disk.
 *
 * Captures complete emulator state (RAM, CPU registers, etc.) and writes
 * to the current slot. Temporarily disables fast-forward during save.
 *
 * @note Based on picoarch implementation
 * @note Silently fails if core doesn't support states or allocation fails
 */
void State_write(void) {
	int was_ff = fast_forward;
	fast_forward = 0;

	char filename[MAX_PATH];
	State_getPath(filename);

	MinArchStateCore state_core = {.serialize_size = core.serialize_size,
	                               .serialize = core.serialize,
	                               .unserialize = core.unserialize};

	MinArchStateResult result = MinArchState_write(filename, &state_core);

	if (result != MINARCH_STATE_OK && result != MINARCH_STATE_NO_SUPPORT) {
		LOG_error("Error writing state: %s (%s)", filename, MinArchState_resultString(result));
	}

	sync();

	fast_forward = was_ff;
}

/**
 * Automatically saves current state to slot 9 (auto-resume slot).
 *
 * Called when user quits game. Preserves current state_slot selection
 * by temporarily switching to slot 9, saving, then restoring original slot.
 *
 * @note AUTO_RESUME_SLOT is typically 9
 */
void State_autosave(void) {
	int last_state_slot = state_slot;
	state_slot = AUTO_RESUME_SLOT;
	State_write();
	state_slot = last_state_slot;
}

/**
 * Automatically loads state from auto-resume slot on startup.
 *
 * MinUI launcher can request a specific slot via RESUME_SLOT_PATH file.
 * If that file exists, loads from specified slot instead of default.
 * The file is deleted after reading to prevent repeated auto-loads.
 *
 * @note Preserves current state_slot selection
 * @note Silently skips if RESUME_SLOT_PATH doesn't exist
 */
static void State_resume(void) {
	if (!exists(RESUME_SLOT_PATH))
		return;

	int last_state_slot = state_slot;
	state_slot = getInt(RESUME_SLOT_PATH);
	unlink(RESUME_SLOT_PATH);
	State_read();
	state_slot = last_state_slot;
}

///////////////////////////////

// Option and MinArchOptionList types defined in minarch_internal.h

// Label arrays, enums, and button constants now in minarch_mappings.h

// Static button mapping arrays now in minarch_mappings.h/c
// Runtime button mapping state
static MinArchButtonMapping core_button_mapping[RETRO_BUTTON_COUNT + 1] = {0};

// Config state enum aliases (actual enum in minarch_config.h)
#define CONFIG_NONE MINARCH_CONFIG_NONE
#define CONFIG_CONSOLE MINARCH_CONFIG_CONSOLE
#define CONFIG_GAME MINARCH_CONFIG_GAME

static inline char* getScreenScalingDesc(void) {
	if (GFX_supportsOverscan()) {
		return "Native uses integer scaling. Aspect uses core\nreported aspect ratio. Fullscreen "
		       "has non-square\npixels. Cropped is integer scaled then cropped.";
	} else {
		return "Native uses integer scaling.\nAspect uses core reported aspect ratio.\nFullscreen "
		       "has non-square pixels.";
	}
}
static inline int getScreenScalingCount(void) {
	return GFX_supportsOverscan() ? 4 : 3;
}


// Config instance (struct defined in minarch_internal.h)
static struct Config config = {
    .system_cfg = NULL,
    .default_cfg = NULL,
    .user_cfg = NULL,
    .device_tag = NULL,
    .frontend =
        {// (MinArchOptionList)
         .count = FE_OPT_COUNT,
         .changed = 0,
         .options = (MinArchOption[]){[FE_OPT_SCALING] =
                                          {
                                              .key = "minarch_screen_scaling",
                                              .name = "Screen Scaling",
                                              .desc = NULL, // will call getScreenScalingDesc()
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 1,
                                              .value = 1,
                                              .count = 3, // will call getScreenScalingCount()
                                              .lock = 0,
                                              .values = minarch_scaling_labels,
                                              .labels = minarch_scaling_labels,
                                          },
                                      [FE_OPT_EFFECT] =
                                          {
                                              .key = "minarch_screen_effect",
                                              .name = "Screen Effect",
                                              .desc = "Simulates classic CRT and LCD displays.\n"
                                                      "Works best at native scaling.",
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 0,
                                              .value = 0,
                                              .count = 6,
                                              .lock = 0,
                                              .values = minarch_effect_labels,
                                              .labels = minarch_effect_labels,
                                          },
                                      [FE_OPT_SHARPNESS] =
                                          {
                                              .key = "minarch_screen_sharpness",
                                              .name = "Screen Sharpness",
                                              .desc = "Sharp uses nearest neighbor "
                                                      "sampling.\nCrisp integer upscales "
                                                      "before linear sampling.\nSoft uses linear "
                                                      "sampling.",
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 2,
                                              .value = 2,
                                              .count = 3,
                                              .lock = 0,
                                              .values = minarch_sharpness_labels,
                                              .labels = minarch_sharpness_labels,
                                          },
                                      [FE_OPT_OVERCLOCK] =
                                          {
                                              .key = "minarch_cpu_speed",
                                              .name = "CPU Speed",
                                              .desc = "Over- or underclock the CPU to prioritize\n"
                                                      "performance or power savings.\n"
                                                      "Auto adjusts based on emulation demand.",
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 3, // Auto
                                              .value = 3, // Auto
                                              .count = 4,
                                              .lock = 0,
                                              .values = minarch_overclock_labels,
                                              .labels = minarch_overclock_labels,
                                          },
                                      [FE_OPT_DEBUG] =
                                          {
                                              .key = "minarch_debug_hud",
                                              .name = "Debug HUD",
                                              .desc = "Show frames per second, cpu "
                                                      "load,\nresolution, and scaler "
                                                      "information.",
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 0,
                                              .value = 0,
                                              .count = 2,
                                              .lock = 0,
                                              .values = minarch_onoff_labels,
                                              .labels = minarch_onoff_labels,
                                          },
                                      [FE_OPT_MAXFF] =
                                          {
                                              .key = "minarch_max_ff_speed",
                                              .name = "Max FF Speed",
                                              .desc = "Fast forward will not exceed the\nselected "
                                                      "speed (but may "
                                                      "be less\ndepending on game and emulator).",
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 3, // 4x
                                              .value = 3, // 4x
                                              .count = 8,
                                              .lock = 0,
                                              .values = minarch_max_ff_labels,
                                              .labels = minarch_max_ff_labels,
                                          },
                                      [FE_OPT_COUNT] =
                                          {
                                              .key = NULL,
                                              .name = NULL,
                                              .desc = NULL,
                                              .full = NULL,
                                              .var = NULL,
                                              .default_value = 0,
                                              .value = 0,
                                              .count = 0,
                                              .lock = 0,
                                              .values = NULL,
                                              .labels = NULL}},
         .enabled_count = 0,
         .enabled_options = NULL},
    .core =
        {
            // (MinArchOptionList)
            .count = 0,
            .changed = 0,
            .options =
                (MinArchOption[]){
                    {.key = NULL,
                     .name = NULL,
                     .desc = NULL,
                     .full = NULL,
                     .var = NULL,
                     .default_value = 0,
                     .value = 0,
                     .count = 0,
                     .lock = 0,
                     .values = NULL,
                     .labels = NULL},
                },
            .enabled_count = 0,
            .enabled_options = NULL,
        },
    .controls = minarch_default_button_mapping,
    .shortcuts = (MinArchButtonMapping[]){[SHORTCUT_SAVE_STATE] = {.name = "Save State",
                                                                   .retro_id = -1,
                                                                   .local_id = BTN_ID_NONE,
                                                                   .modifier = 0,
                                                                   .default_id = 0,
                                                                   .ignore = 0},
                                          [SHORTCUT_LOAD_STATE] = {.name = "Load State",
                                                                   .retro_id = -1,
                                                                   .local_id = BTN_ID_NONE,
                                                                   .modifier = 0,
                                                                   .default_id = 0,
                                                                   .ignore = 0},
                                          [SHORTCUT_RESET_GAME] = {.name = "Reset Game",
                                                                   .retro_id = -1,
                                                                   .local_id = BTN_ID_NONE,
                                                                   .modifier = 0,
                                                                   .default_id = 0,
                                                                   .ignore = 0},
                                          [SHORTCUT_SAVE_QUIT] = {.name = "Save & Quit",
                                                                  .retro_id = -1,
                                                                  .local_id = BTN_ID_NONE,
                                                                  .modifier = 0,
                                                                  .default_id = 0,
                                                                  .ignore = 0},
                                          [SHORTCUT_CYCLE_SCALE] = {.name = "Cycle Scaling",
                                                                    .retro_id = -1,
                                                                    .local_id = BTN_ID_NONE,
                                                                    .modifier = 0,
                                                                    .default_id = 0,
                                                                    .ignore = 0},
                                          [SHORTCUT_CYCLE_EFFECT] = {.name = "Cycle Effect",
                                                                     .retro_id = -1,
                                                                     .local_id = BTN_ID_NONE,
                                                                     .modifier = 0,
                                                                     .default_id = 0,
                                                                     .ignore = 0},
                                          [SHORTCUT_TOGGLE_FF] = {.name = "Toggle FF",
                                                                  .retro_id = -1,
                                                                  .local_id = BTN_ID_NONE,
                                                                  .modifier = 0,
                                                                  .default_id = 0,
                                                                  .ignore = 0},
                                          [SHORTCUT_HOLD_FF] = {.name = "Hold FF",
                                                                .retro_id = -1,
                                                                .local_id = BTN_ID_NONE,
                                                                .modifier = 0,
                                                                .default_id = 0,
                                                                .ignore = 0},
                                          {.name = NULL,
                                           .retro_id = 0,
                                           .local_id = 0,
                                           .modifier = 0,
                                           .default_id = 0,
                                           .ignore = 0}},
    .loaded = 0,
    .initialized = 0,
};
// Config_getValue moved to minarch_config.c as MinArchConfig_getValue

///////////////////////////////////////
// Auto CPU Scaling
///////////////////////////////////////
// Dynamically adjusts CPU frequency based on frame timing.
// See docs/auto-cpu-scaling.md for design details.
//
// Components:
// - Background thread: Applies CPU changes without blocking main loop
// - updateAutoCPU(): Per-frame monitoring called from main loop

/**
 * Background thread that applies CPU frequency changes.
 *
 * Supports two modes:
 * - Granular: Uses detected frequency array and PLAT_setCPUFrequency()
 * - Fallback: Uses 3 levels (POWERSAVE/NORMAL/PERFORMANCE) via PWR_setCPUSpeed()
 *
 * This keeps expensive system() calls (overclock.elf) off the main emulation loop,
 * preventing frame drops and audio glitches during CPU scaling.
 *
 * Thread safety: Uses auto_cpu_mutex to protect shared state.
 */
static void* auto_cpu_scaling_thread(void* arg) {
	LOG_debug("Auto CPU thread: started (granular=%d, freq_count=%d)\n",
	          auto_cpu_state.use_granular, auto_cpu_state.freq_count);

	while (auto_cpu_thread_running) {
		if (auto_cpu_state.use_granular) {
			// Granular frequency mode
			pthread_mutex_lock(&auto_cpu_mutex);
			int target_idx = auto_cpu_state.target_index;
			int current_idx = auto_cpu_state.current_index;
			pthread_mutex_unlock(&auto_cpu_mutex);

			if (target_idx != current_idx && target_idx >= 0 &&
			    target_idx < auto_cpu_state.freq_count) {
				int freq_khz = auto_cpu_state.frequencies[target_idx];
				unsigned audio_fill_before = SND_getBufferOccupancy();
				LOG_info("Auto CPU: setting %d kHz (index %d/%d, audio=%u%%)\n", freq_khz,
				         target_idx, auto_cpu_state.freq_count - 1, audio_fill_before);

				int result = PLAT_setCPUFrequency(freq_khz);
				if (result == 0) {
					pthread_mutex_lock(&auto_cpu_mutex);
					auto_cpu_state.current_index = target_idx;
					pthread_mutex_unlock(&auto_cpu_mutex);
				} else {
					LOG_warn("Auto CPU: failed to set frequency %d kHz\n", freq_khz);
				}
			}
		} else {
			// Fallback to 3-level mode
			pthread_mutex_lock(&auto_cpu_mutex);
			int target = auto_cpu_state.target_level;
			int current = auto_cpu_state.current_level;
			pthread_mutex_unlock(&auto_cpu_mutex);

			if (target != current) {
				int cpu_speed;
				const char* level_name;
				switch (target) {
				case 0:
					cpu_speed = CPU_SPEED_POWERSAVE;
					level_name = "POWERSAVE";
					break;
				case 1:
					cpu_speed = CPU_SPEED_NORMAL;
					level_name = "NORMAL";
					break;
				case 2:
					cpu_speed = CPU_SPEED_PERFORMANCE;
					level_name = "PERFORMANCE";
					break;
				default:
					cpu_speed = CPU_SPEED_NORMAL;
					level_name = "NORMAL";
					break;
				}

				LOG_info("Auto CPU: applying %s (level %d)\n", level_name, target);
				PWR_setCPUSpeed(cpu_speed);

				pthread_mutex_lock(&auto_cpu_mutex);
				auto_cpu_state.current_level = target;
				pthread_mutex_unlock(&auto_cpu_mutex);
			}
		}

		// Check every 50ms (responsive but not wasteful)
		usleep(50000);
	}

	LOG_debug("Auto CPU thread: stopped\n");
	return NULL;
}

/**
 * Starts the auto CPU scaling background thread.
 *
 * Call when entering Auto mode (overclock == 3).
 * Thread will run until auto_cpu_stopThread() is called.
 */
static void auto_cpu_startThread(void) {
	if (auto_cpu_thread_running)
		return;

	auto_cpu_thread_running = 1;
	if (pthread_create(&auto_cpu_thread, NULL, auto_cpu_scaling_thread, NULL) != 0) {
		LOG_error("Failed to create auto CPU scaling thread\n");
		auto_cpu_thread_running = 0;
	} else {
		LOG_debug("Auto CPU: thread started\n");
	}
}

/**
 * Stops the auto CPU scaling background thread.
 *
 * Call when exiting Auto mode or when game ends.
 * Waits for thread to finish before returning.
 */
static void auto_cpu_stopThread(void) {
	if (!auto_cpu_thread_running)
		return;

	auto_cpu_thread_running = 0;
	pthread_join(auto_cpu_thread, NULL);
	LOG_debug("Auto CPU: thread stopped\n");
}

/**
 * Requests a CPU level change (non-blocking, fallback mode).
 *
 * Sets the target level which the background thread will apply.
 * Returns immediately without blocking the main loop.
 *
 * @param level Target level (0=POWERSAVE, 1=NORMAL, 2=PERFORMANCE)
 */
static void auto_cpu_setTargetLevel(int level) {
	pthread_mutex_lock(&auto_cpu_mutex);
	auto_cpu_state.target_level = level;
	pthread_mutex_unlock(&auto_cpu_mutex);
}

/**
 * Requests a CPU frequency index change (non-blocking, granular mode).
 *
 * @param index Target index into auto_cpu_state.frequencies array
 */
static void auto_cpu_setTargetIndex(int index) {
	if (index < 0)
		index = 0;
	if (index >= auto_cpu_state.freq_count)
		index = auto_cpu_state.freq_count - 1;

	pthread_mutex_lock(&auto_cpu_mutex);
	auto_cpu_state.target_index = index;
	pthread_mutex_unlock(&auto_cpu_mutex);
}

/**
 * Gets the current frequency index (thread-safe).
 */
static int auto_cpu_getCurrentIndex(void) {
	pthread_mutex_lock(&auto_cpu_mutex);
	int idx = auto_cpu_state.current_index;
	pthread_mutex_unlock(&auto_cpu_mutex);
	return idx;
}

/**
 * Gets the current frequency in kHz.
 */
static int auto_cpu_getCurrentFrequency(void) {
	int idx = auto_cpu_getCurrentIndex();
	if (idx >= 0 && idx < auto_cpu_state.freq_count) {
		return auto_cpu_state.frequencies[idx];
	}
	return 0;
}

/**
 * Finds the index of the nearest frequency to the target.
 * Wrapper around module function for convenience.
 */
static int auto_cpu_findNearestIndex(int target_khz) {
	return MinArchCPU_findNearestIndex(auto_cpu_state.frequencies, auto_cpu_state.freq_count,
	                                   target_khz);
}

/**
 * Detects available CPU frequencies and initializes granular scaling.
 *
 * Called once during auto mode initialization. Populates the frequency array
 * and calculates preset indices for POWERSAVE/NORMAL/PERFORMANCE.
 *
 * Strategy for preset mapping (matches manual preset percentages):
 * - POWERSAVE: 55% of max frequency
 * - NORMAL: 80% of max frequency
 * - PERFORMANCE: 100% (max frequency)
 */
static void auto_cpu_detectFrequencies(void) {
	int raw_count =
	    PLAT_getAvailableCPUFrequencies(auto_cpu_state.frequencies, CPU_MAX_FREQUENCIES);

	// Filter out frequencies below minimum threshold
	auto_cpu_state.freq_count = 0;
	for (int i = 0; i < raw_count; i++) {
		if (auto_cpu_state.frequencies[i] >= auto_cpu_config.min_freq_khz) {
			auto_cpu_state.frequencies[auto_cpu_state.freq_count++] = auto_cpu_state.frequencies[i];
		}
	}

	if (auto_cpu_state.freq_count >= 2) {
		auto_cpu_state.use_granular = 1;

		// Calculate preset indices based on frequency percentages of max
		int max_freq = auto_cpu_state.frequencies[auto_cpu_state.freq_count - 1];

		// POWERSAVE: 55% of max
		int ps_target = max_freq * 55 / 100;
		auto_cpu_state.preset_indices[0] = auto_cpu_findNearestIndex(ps_target);

		// NORMAL: 80% of max
		int normal_target = max_freq * 80 / 100;
		auto_cpu_state.preset_indices[1] = auto_cpu_findNearestIndex(normal_target);

		// PERFORMANCE: max frequency
		auto_cpu_state.preset_indices[2] = auto_cpu_state.freq_count - 1;

		LOG_info("Auto CPU: %d frequencies available (%d - %d kHz), filtered from %d\n",
		         auto_cpu_state.freq_count, auto_cpu_state.frequencies[0], max_freq, raw_count);
		LOG_info("Auto CPU: preset indices PS=%d (%d kHz), N=%d (%d kHz), P=%d (%d kHz)\n",
		         auto_cpu_state.preset_indices[0],
		         auto_cpu_state.frequencies[auto_cpu_state.preset_indices[0]],
		         auto_cpu_state.preset_indices[1],
		         auto_cpu_state.frequencies[auto_cpu_state.preset_indices[1]],
		         auto_cpu_state.preset_indices[2],
		         auto_cpu_state.frequencies[auto_cpu_state.preset_indices[2]]);

		// Log all frequencies for debugging
		LOG_debug("Auto CPU: frequency table:\n");
		for (int i = 0; i < auto_cpu_state.freq_count; i++) {
			LOG_debug("  [%d] %d kHz\n", i, auto_cpu_state.frequencies[i]);
		}
	} else {
		auto_cpu_state.use_granular = 0;
		LOG_info("Auto CPU: %d frequencies after filtering (raw: %d), using 3-level fallback\n",
		         auto_cpu_state.freq_count, raw_count);
	}
}

static void resetAutoCPUState(void) {
	auto_cpu_state.frame_count = 0;
	auto_cpu_state.high_util_windows = 0;
	auto_cpu_state.low_util_windows = 0;
	auto_cpu_state.last_underrun = SND_getUnderrunCount();
	auto_cpu_state.startup_frames = 0;
	auto_cpu_state.frame_time_index = 0;
	auto_cpu_state.panic_cooldown = 0;

	// Reset panic tracking (menu changes may allow lower frequencies to work)
	memset(auto_cpu_state.panic_count, 0, sizeof(auto_cpu_state.panic_count));

	// Calculate frame budget from core's declared FPS
	if (core.fps > 0) {
		auto_cpu_state.frame_budget_us = (uint64_t)(1000000.0 / core.fps);
	} else {
		auto_cpu_state.frame_budget_us = 16667; // Default to 60fps
	}

	// Clear frame time buffer
	memset(auto_cpu_state.frame_times, 0, sizeof(auto_cpu_state.frame_times));

	// Detect available frequencies (only once, on first auto mode entry)
	static int frequencies_detected = 0;
	if (!frequencies_detected) {
		auto_cpu_detectFrequencies();
		frequencies_detected = 1;
	}

	// Note: target/current frequency set by setOverclock() after this call

	LOG_info("Auto CPU: enabled, frame budget=%lluus (%.2f fps), granular=%d\n",
	         (unsigned long long)auto_cpu_state.frame_budget_us, core.fps,
	         auto_cpu_state.use_granular);
	LOG_debug(
	    "Auto CPU: util thresholds high=%d%% low=%d%%, windows boost=%d reduce=%d, grace=%d\n",
	    auto_cpu_config.util_high, auto_cpu_config.util_low, auto_cpu_config.boost_windows,
	    auto_cpu_config.reduce_windows, auto_cpu_config.startup_grace);
}

void setOverclock(int i) {
	// Stop auto thread if leaving auto mode
	if (overclock == 3 && i != 3) {
		auto_cpu_stopThread();
	}

	overclock = i;
	switch (i) {
	case 0:
		PWR_setCPUSpeed(CPU_SPEED_POWERSAVE);
		break;
	case 1:
		PWR_setCPUSpeed(CPU_SPEED_NORMAL);
		break;
	case 2:
		PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE);
		break;
	case 3: // Auto
		resetAutoCPUState();
		// Start at max frequency to avoid startup stutter during grace period
		// Background thread will scale down as needed after grace period
		if (auto_cpu_state.use_granular) {
			int start_idx = auto_cpu_state.preset_indices[2]; // PERFORMANCE - start high
			int start_freq = auto_cpu_state.frequencies[start_idx];
			PLAT_setCPUFrequency(start_freq);
			pthread_mutex_lock(&auto_cpu_mutex);
			auto_cpu_state.target_index =
			    start_idx; // Set target so thread doesn't immediately change
			auto_cpu_state.current_index = start_idx; // Mark as applied
			pthread_mutex_unlock(&auto_cpu_mutex);
		} else {
			PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE);
			pthread_mutex_lock(&auto_cpu_mutex);
			auto_cpu_state.target_level = 2; // Set target so thread doesn't immediately change
			auto_cpu_state.current_level = 2; // Mark as applied
			pthread_mutex_unlock(&auto_cpu_mutex);
		}
		auto_cpu_startThread();
		break;
	}
}

// Vsync rate for diagnostics (currently unused, would need measurement to populate)
static float current_vsync_hz = 0;

/**
 * Updates auto CPU scaling based on frame timing (core.run() execution time).
 *
 * Called every frame from the main emulation loop when overclock == 3 (Auto).
 * Uses the 90th percentile of frame execution times to determine CPU utilization,
 * which directly measures emulation performance independent of audio/display timing.
 *
 * Granular Mode Algorithm:
 * - Performance scales linearly with frequency
 * - Boost: Jump to predicted optimal frequency (no step limit)
 * - Reduce: Limited to max_step_down indices to prevent underruns
 * - Panic: Boost by panic_step_up on underrun, with cooldown
 *
 * Fallback Mode Algorithm (3 levels):
 * - Count consecutive high/low util windows
 * - Boost after 2 high-util windows (~1s), reduce after 4 low-util windows (~2s)
 */
static void updateAutoCPU(void) {
	// Skip if not in auto mode or during special states
	if (overclock != 3 || fast_forward || show_menu)
		return;

	// Startup grace period - don't scale during initial warmup
	if (auto_cpu_state.startup_frames < auto_cpu_config.startup_grace) {
		auto_cpu_state.startup_frames++;
		if (auto_cpu_state.startup_frames == auto_cpu_config.startup_grace) {
			LOG_debug("Auto CPU: grace period complete, monitoring active\n");
		}
		return;
	}

	// Get current state (thread-safe read)
	pthread_mutex_lock(&auto_cpu_mutex);
	int current_idx = auto_cpu_state.target_index;
	int current_level = auto_cpu_state.target_level;
	pthread_mutex_unlock(&auto_cpu_mutex);

	// Emergency: check for actual underruns (panic path)
	unsigned underruns = SND_getUnderrunCount();
	int max_idx = auto_cpu_state.freq_count - 1;
	int at_max = auto_cpu_state.use_granular ? (current_idx >= max_idx) : (current_level >= 2);

	if (underruns > auto_cpu_state.last_underrun && !at_max) {
		// Underrun detected - track panic and boost
		unsigned audio_fill = SND_getBufferOccupancy();

		// Track panic at current frequency (for failsafe blocking).
		// If a frequency can't keep up, all lower frequencies are also blocked
		// because lower freq = less CPU throughput = guaranteed worse performance.
		if (auto_cpu_state.use_granular && current_idx >= 0 &&
		    current_idx < MINARCH_CPU_MAX_FREQUENCIES) {
			auto_cpu_state.panic_count[current_idx]++;

			if (auto_cpu_state.panic_count[current_idx] >= MINARCH_CPU_PANIC_THRESHOLD) {
				LOG_warn("Auto CPU: BLOCKING %d kHz and below after %d panics (audio=%u%%)\n",
				         auto_cpu_state.frequencies[current_idx],
				         auto_cpu_state.panic_count[current_idx], audio_fill);
				// Block this frequency and all below - they can't possibly work
				// if this one failed (lower freq = strictly less performance)
				for (int i = 0; i <= current_idx; i++) {
					auto_cpu_state.panic_count[i] = MINARCH_CPU_PANIC_THRESHOLD;
				}
			}
		}

		if (auto_cpu_state.use_granular) {
			int new_idx = current_idx + auto_cpu_config.panic_step_up;
			if (new_idx > max_idx)
				new_idx = max_idx;
			auto_cpu_setTargetIndex(new_idx);
			LOG_warn("Auto CPU: PANIC - underrun, boosting %d→%d kHz (audio=%u%%)\n",
			         auto_cpu_state.frequencies[current_idx], auto_cpu_state.frequencies[new_idx],
			         audio_fill);
		} else {
			int new_level = current_level + auto_cpu_config.panic_step_up;
			if (new_level > 2)
				new_level = 2;
			auto_cpu_setTargetLevel(new_level);
			LOG_warn("Auto CPU: PANIC - underrun, boosting to level %d (audio=%u%%)\n", new_level,
			         audio_fill);
		}
		auto_cpu_state.high_util_windows = 0;
		auto_cpu_state.low_util_windows = 0;
		// Cooldown: wait 8 windows (~4 seconds) before allowing reduction
		auto_cpu_state.panic_cooldown = 8;
		SND_resetUnderrunCount();
		auto_cpu_state.last_underrun = 0;
		return;
	}
	// Update underrun tracking (even if at max level)
	if (underruns > auto_cpu_state.last_underrun) {
		auto_cpu_state.last_underrun = underruns;
	}

	// Count frames in current window
	auto_cpu_state.frame_count++;

	// Check if window is complete
	if (auto_cpu_state.frame_count >= auto_cpu_config.window_frames) {
		// Calculate 90th percentile frame time (ignores outliers like loading screens)
		int samples = (auto_cpu_state.frame_time_index < auto_cpu_config.window_frames)
		                  ? auto_cpu_state.frame_time_index
		                  : auto_cpu_config.window_frames;
		if (samples < 5) {
			// Not enough samples yet - reset and wait
			auto_cpu_state.frame_count = 0;
			return;
		}

		uint64_t p90_time = percentileUint64(auto_cpu_state.frame_times, samples, 0.90f);

		// Calculate utilization as percentage of frame budget
		unsigned util = 0;
		if (auto_cpu_state.frame_budget_us > 0) {
			util = (unsigned)((p90_time * 100) / auto_cpu_state.frame_budget_us);
			if (util > 200)
				util = 200; // Cap at 200% for sanity
		}

		if (auto_cpu_state.use_granular) {
			// Granular mode: use linear performance scaling to find optimal frequency
			// Performance scales linearly with frequency, so:
			// new_util = current_util * (current_freq / new_freq)

			int current_freq = auto_cpu_state.frequencies[current_idx];

			// Decrement panic cooldown each window
			if (auto_cpu_state.panic_cooldown > 0) {
				auto_cpu_state.panic_cooldown--;
			}

			if (util > auto_cpu_config.util_high) {
				// Need more performance - step up
				auto_cpu_state.high_util_windows++;
				auto_cpu_state.low_util_windows = 0;

				if (auto_cpu_state.high_util_windows >= auto_cpu_config.boost_windows &&
				    current_idx < max_idx) {
					// Find next frequency that would bring util to target (sweet spot)
					// Using: new_util = util * (current_freq / new_freq)
					// So: new_freq = current_freq * util / target_util
					// No step limit - linear scaling prediction is accurate, boost aggressively
					int needed_freq = current_freq * (int)util / auto_cpu_config.target_util;
					int new_idx = auto_cpu_findNearestIndex(needed_freq);

					// Ensure we actually go higher
					if (new_idx <= current_idx)
						new_idx = current_idx + 1;
					if (new_idx > max_idx)
						new_idx = max_idx;

					auto_cpu_setTargetIndex(new_idx);
					auto_cpu_state.high_util_windows = 0;
					unsigned audio_fill = SND_getBufferOccupancy();
					LOG_info("Auto CPU: BOOST %d→%d kHz (util=%u%%, target ~%d%%, audio=%u%%)\n",
					         current_freq, auto_cpu_state.frequencies[new_idx], util,
					         auto_cpu_config.target_util, audio_fill);
				}
			} else if (util < auto_cpu_config.util_low) {
				// Can reduce power - step down
				auto_cpu_state.low_util_windows++;
				auto_cpu_state.high_util_windows = 0;

				// Only reduce if: enough consecutive low windows AND panic cooldown expired
				int reduce_ok =
				    (auto_cpu_state.low_util_windows >= auto_cpu_config.reduce_windows) &&
				    (auto_cpu_state.panic_cooldown == 0) && (current_idx > 0);

				if (reduce_ok) {
					// Find frequency that would bring util up to target (sweet spot)
					// new_util = util * (current_freq / new_freq)
					// new_freq = current_freq * util / target_util
					int needed_freq = current_freq * (int)util / auto_cpu_config.target_util;
					int new_idx = auto_cpu_findNearestIndex(needed_freq);

					// Ensure we actually go lower
					if (new_idx >= current_idx)
						new_idx = current_idx - 1;
					if (new_idx < 0)
						new_idx = 0;

					// Limit reduction to max_step_down indices at once
					if (current_idx - new_idx > auto_cpu_config.max_step_down) {
						new_idx = current_idx - auto_cpu_config.max_step_down;
					}

					// Skip blocked frequencies - find first unblocked one above new_idx.
					// Frequencies get blocked when they cause repeated panics.
					while (new_idx >= 0 &&
					       auto_cpu_state.panic_count[new_idx] >= MINARCH_CPU_PANIC_THRESHOLD) {
						new_idx++;
						if (new_idx >= current_idx) {
							// All lower frequencies blocked - stay at current
							break;
						}
					}

					// Don't reduce if no safe frequency found
					if (new_idx >= current_idx) {
						auto_cpu_state.low_util_windows = 0;
					} else {
						int new_freq = auto_cpu_state.frequencies[new_idx];
						int predicted_util = util * current_freq / new_freq;

						auto_cpu_setTargetIndex(new_idx);
						auto_cpu_state.low_util_windows = 0;
						unsigned audio_fill = SND_getBufferOccupancy();
						LOG_info(
						    "Auto CPU: REDUCE %d→%d kHz (util=%u%%, predicted ~%d%%, audio=%u%%)\n",
						    current_freq, new_freq, util, predicted_util, audio_fill);
					}
				}
			} else {
				// In sweet spot - reset counters
				auto_cpu_state.high_util_windows = 0;
				auto_cpu_state.low_util_windows = 0;
			}

			// Sampled debug logging (every 4th window = ~2 seconds)
			static int debug_window_count = 0;
			if (++debug_window_count >= 4) {
				debug_window_count = 0;
				SND_Snapshot snap = SND_getSnapshot();
				LOG_debug("Auto CPU: fill=%u%% int=%.4f adj=%.4f util=%u%% freq=%dkHz idx=%d/%d\n",
				          snap.fill_pct, snap.rate_integral, snap.total_adjust, util, current_freq,
				          current_idx, max_idx);
			}
		} else {
			// Fallback mode: 3-level scaling (original algorithm)
			if (util > auto_cpu_config.util_high) {
				auto_cpu_state.high_util_windows++;
				auto_cpu_state.low_util_windows = 0;
			} else if (util < auto_cpu_config.util_low) {
				auto_cpu_state.low_util_windows++;
				auto_cpu_state.high_util_windows = 0;
			} else {
				auto_cpu_state.high_util_windows = 0;
				auto_cpu_state.low_util_windows = 0;
			}

			// Sampled debug logging
			static int debug_window_count_fallback = 0;
			if (++debug_window_count_fallback >= 4) {
				debug_window_count_fallback = 0;
				SND_Snapshot snap = SND_getSnapshot();
				LOG_debug("Auto CPU: fill=%u%% int=%.4f adj=%.4f util=%u%% level=%d\n",
				          snap.fill_pct, snap.rate_integral, snap.total_adjust, util,
				          current_level);
			}

			// Boost if sustained high utilization
			if (auto_cpu_state.high_util_windows >= auto_cpu_config.boost_windows &&
			    current_level < 2) {
				int new_level = current_level + 1;
				auto_cpu_setTargetLevel(new_level);
				auto_cpu_state.high_util_windows = 0;
				LOG_info("Auto CPU: BOOST level %d (util=%u%%)\n", new_level, util);
			}

			// Reduce if sustained low utilization
			if (auto_cpu_state.low_util_windows >= auto_cpu_config.reduce_windows &&
			    current_level > 0) {
				int new_level = current_level - 1;
				auto_cpu_setTargetLevel(new_level);
				auto_cpu_state.low_util_windows = 0;
				LOG_info("Auto CPU: REDUCE level %d (util=%u%%)\n", new_level, util);
			}
		}

		// Reset window counter (frame times stay in ring buffer)
		auto_cpu_state.frame_count = 0;
	}
}

static void Config_syncFrontend(char* key, int value) {
	int i = -1;
	if (exactMatch(key, config.frontend.options[FE_OPT_SCALING].key)) {
		screen_scaling = value;

		// Integer scaling modes (Native/Cropped) always use sharp pixels
		if (screen_scaling == MINARCH_SCALE_NATIVE || screen_scaling == MINARCH_SCALE_CROPPED)
			GFX_setSharpness(SHARPNESS_SHARP);
		else
			GFX_setSharpness(screen_sharpness);

		renderer.dst_p = 0;
		i = FE_OPT_SCALING;
	} else if (exactMatch(key, config.frontend.options[FE_OPT_EFFECT].key)) {
		screen_effect = value;
		GFX_setEffect(value);
		renderer.dst_p = 0;
		i = FE_OPT_EFFECT;
	} else if (exactMatch(key, config.frontend.options[FE_OPT_SHARPNESS].key)) {
		screen_sharpness = value;

		// Integer scaling modes (Native/Cropped) always use sharp pixels
		if (screen_scaling == MINARCH_SCALE_NATIVE || screen_scaling == MINARCH_SCALE_CROPPED)
			GFX_setSharpness(SHARPNESS_SHARP);
		else
			GFX_setSharpness(screen_sharpness);

		renderer.dst_p = 0;
		i = FE_OPT_SHARPNESS;
	} else if (exactMatch(key, config.frontend.options[FE_OPT_OVERCLOCK].key)) {
		overclock = value;
		i = FE_OPT_OVERCLOCK;
	} else if (exactMatch(key, config.frontend.options[FE_OPT_DEBUG].key)) {
		show_debug = value;
		i = FE_OPT_DEBUG;
	} else if (exactMatch(key, config.frontend.options[FE_OPT_MAXFF].key)) {
		max_ff_speed = value;
		i = FE_OPT_MAXFF;
	}
	if (i == -1)
		return;
	MinArchOption* option = &config.frontend.options[i];
	option->value = value;
}
static void MinArchOptionList_setOptionValue(MinArchOptionList* list, const char* key,
                                             const char* value);
enum {
	CONFIG_WRITE_ALL,
	CONFIG_WRITE_GAME,
};
static void Config_getPath(char* filename, int override) {
	char device_tag[64] = {0};
	if (config.device_tag)
		(void)sprintf(device_tag, "-%s", config.device_tag);
	if (override)
		(void)sprintf(filename, "%s/%s%s.cfg", core.config_dir, game.name, device_tag);
	else
		(void)sprintf(filename, "%s/minarch%s.cfg", core.config_dir, device_tag);
	LOG_debug("Config_getPath %s", filename);
}

///////////////////////////////////////
// Configuration System
///////////////////////////////////////
// Manages frontend and core settings persistence.
// Configuration stored in /mnt/SDCARD/.userdata/<platform>/<core>/minarch.cfg

/**
 * Initializes configuration system from default core config.
 *
 * Parses default controller bindings from the core's initial configuration
 * and stores them in the config structure. This provides fallback values
 * before user customization.
 *
 * @note Only runs once (skipped if already initialized)
 * @note Reads "bind" lines from config.default_cfg
 */
static void Config_init(void) {
	if (!config.default_cfg || config.initialized)
		return;

	LOG_info("Config_init");
	char* tmp = config.default_cfg;
	char* tmp2;
	char* key;

	char button_name[128];
	char button_id[128];
	int i = 0;
	while ((tmp = strstr(tmp, "bind "))) {
		tmp += 5; // tmp now points to the button name (plus the rest of the line)
		key = tmp;
		tmp = strstr(tmp, " = ");
		if (!tmp)
			break;

		int len = tmp - key;
		strncpy(button_name, key, len);
		button_name[len] = '\0';

		tmp += 3;
		strncpy(button_id, tmp, 128);
		tmp2 = strchr(button_id, '\n');
		if (!tmp2)
			tmp2 = strchr(button_id, '\r');
		if (tmp2)
			*tmp2 = '\0';

		int retro_id = -1;
		int local_id = -1;

		tmp2 = strrchr(button_id, ':');
		int remap = 0;
		if (tmp2) {
			const MinArchButtonMapping* found =
			    MinArchInput_findMappingByName(minarch_button_label_mapping, tmp2 + 1);
			if (found)
				retro_id = found->retro_id;
			*tmp2 = '\0';
		}
		const MinArchButtonMapping* found =
		    MinArchInput_findMappingByName(minarch_button_label_mapping, button_id);
		if (found) {
			local_id = found->local_id;
			if (retro_id == -1)
				retro_id = found->retro_id;
		}

		tmp += strlen(button_id); // prepare to continue search

		LOG_debug("\tbind %s (%s) %i:%i", button_name, button_id, local_id, retro_id);

		// TODO: test this without a final line return
		tmp2 = calloc(strlen(button_name) + 1, sizeof(char));
		if (!tmp2) {
			for (int j = 0; j < i; j++) {
				if (core_button_mapping[j].name)
					free(core_button_mapping[j].name);
			}
			return;
		}
		safe_strcpy(tmp2, button_name, strlen(button_name) + 1);
		MinArchButtonMapping* button = &core_button_mapping[i++];
		button->name = tmp2;
		button->retro_id = retro_id;
		button->local_id = local_id;
	};

	config.initialized = 1;
}
static void Config_quit(void) {
	if (!config.initialized)
		return;
	for (int i = 0; core_button_mapping[i].name; i++) {
		free(core_button_mapping[i].name);
	}
}
static void Config_readOptionsString(char* cfg) {
	if (!cfg)
		return;

	LOG_debug("Config_readOptions");
	char key[256];
	char value[256];
	for (int i = 0; config.frontend.options[i].key; i++) {
		MinArchOption* option = &config.frontend.options[i];
		if (!MinArchConfig_getValue(cfg, option->key, value, &option->lock))
			continue;
		MinArchOptionList_setOptionValue(&config.frontend, option->key, value);
		Config_syncFrontend(option->key, option->value);
	}

	if (has_custom_controllers &&
	    MinArchConfig_getValue(cfg, "minarch_gamepad_type", value, NULL)) {
		gamepad_type = strtol(value, NULL, 0);
		int device = strtol(minarch_gamepad_values[gamepad_type], NULL, 0);
		core.set_controller_port_device(0, device);
	}

	for (int i = 0; config.core.options[i].key; i++) {
		MinArchOption* option = &config.core.options[i];
		if (!MinArchConfig_getValue(cfg, option->key, value, &option->lock))
			continue;
		MinArchOptionList_setOptionValue(&config.core, option->key, value);
	}
}
static void Config_readControlsString(char* cfg) {
	if (!cfg)
		return;

	LOG_debug("Config_readControlsString");

	char key[256];
	char value[256];
	char* tmp;
	for (int i = 0; config.controls[i].name; i++) {
		MinArchButtonMapping* mapping = &config.controls[i];
		(void)sprintf(key, "bind %s", mapping->name);
		(void)sprintf(value, "NONE");

		if (!MinArchConfig_getValue(cfg, key, value, NULL))
			continue;
		if ((tmp = strrchr(value, ':')))
			*tmp = '\0'; // this is a binding artifact in default.cfg, ignore

		int id = -1;
		for (int j = 0; minarch_button_labels[j]; j++) {
			if (!strcmp(minarch_button_labels[j], value)) {
				id = j - 1;
				break;
			}
		}
		// LOG_info("\t%s (%i)", value, id);

		int mod = 0;
		if (id >= LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}

		mapping->local_id = id;
		mapping->modifier = mod;
	}

	for (int i = 0; config.shortcuts[i].name; i++) {
		MinArchButtonMapping* mapping = &config.shortcuts[i];
		(void)sprintf(key, "bind %s", mapping->name);
		(void)sprintf(value, "NONE");

		if (!MinArchConfig_getValue(cfg, key, value, NULL))
			continue;

		int id = -1;
		for (int j = 0; minarch_button_labels[j]; j++) {
			if (!strcmp(minarch_button_labels[j], value)) {
				id = j - 1;
				break;
			}
		}

		int mod = 0;
		if (id >= LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}
		// LOG_info("shortcut %s:%s (%i:%i)", key,value, id, mod);

		mapping->local_id = id;
		mapping->modifier = mod;
	}
}
static void Config_load(void) {
	LOG_info("Config_load");

	config.device_tag = getenv("DEVICE");
	LOG_info("config.device_tag %s", config.device_tag);

	// update for crop overscan support
	MinArchOption* scaling_option = &config.frontend.options[FE_OPT_SCALING];
	scaling_option->desc = getScreenScalingDesc();
	scaling_option->count = getScreenScalingCount();
	if (!GFX_supportsOverscan()) {
		minarch_scaling_labels[3] = NULL;
	}

	char* system_path = SYSTEM_PATH "/system.cfg";

	char device_system_path[MAX_PATH] = {0};
	if (config.device_tag)
		(void)sprintf(device_system_path, SYSTEM_PATH "/system-%s.cfg", config.device_tag);

	if (config.device_tag && exists(device_system_path)) {
		LOG_info("Using device_system_path: %s", device_system_path);
		config.system_cfg = allocFile(device_system_path);
	} else if (exists(system_path))
		config.system_cfg = allocFile(system_path);
	else
		config.system_cfg = NULL;

	// LOG_info("config.system_cfg: %s", config.system_cfg);

	char default_path[MAX_PATH];
	getEmuPath((char*)core.tag, default_path);
	char* tmp = strrchr(default_path, '/');
	safe_strcpy(tmp, "/default.cfg", MAX_PATH - (tmp - default_path));

	char device_default_path[MAX_PATH] = {0};
	if (config.device_tag) {
		getEmuPath((char*)core.tag, device_default_path);
		tmp = strrchr(device_default_path, '/');
		char filename[64];
		(void)sprintf(filename, "/default-%s.cfg", config.device_tag);
		safe_strcpy(tmp, filename, MAX_PATH - (tmp - device_default_path));
	}

	if (config.device_tag && exists(device_default_path)) {
		LOG_info("Using device_default_path: %s", device_default_path);
		config.default_cfg = allocFile(device_default_path);
	} else if (exists(default_path))
		config.default_cfg = allocFile(default_path);
	else
		config.default_cfg = NULL;

	// LOG_info("config.default_cfg: %s", config.default_cfg);

	char path[MAX_PATH];
	config.loaded = CONFIG_NONE;
	int override = 0;
	Config_getPath(path, CONFIG_WRITE_GAME);
	if (exists(path))
		override = 1;
	if (!override)
		Config_getPath(path, CONFIG_WRITE_ALL);

	if (exists(path)) {
		config.user_cfg = allocFile(path);
		if (!config.user_cfg)
			return;
		LOG_info("Loaded user config: %s", path);
		config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
	} else {
		config.user_cfg = NULL;
	}
}
static void Config_free(void) {
	if (config.system_cfg)
		free(config.system_cfg);
	if (config.default_cfg)
		free(config.default_cfg);
	if (config.user_cfg)
		free(config.user_cfg);
}
static void Config_readOptions(void) {
	Config_readOptionsString(config.system_cfg);
	Config_readOptionsString(config.default_cfg);
	Config_readOptionsString(config.user_cfg);

	// screen_scaling = MINARCH_SCALE_NATIVE; // TODO: tmp
}
static void Config_readControls(void) {
	Config_readControlsString(config.default_cfg);
	Config_readControlsString(config.user_cfg);
}
static void Config_write(int override) {
	char path[MAX_PATH];
	// sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
	Config_getPath(path, CONFIG_WRITE_GAME);

	if (!override) {
		if (config.loaded == CONFIG_GAME)
			unlink(path);
		Config_getPath(path, CONFIG_WRITE_ALL);
	}
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;

	FILE* file = fopen(path, "wb");
	if (!file)
		return;

	for (int i = 0; config.frontend.options[i].key; i++) {
		MinArchOption* option = &config.frontend.options[i];
		(void)fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}
	for (int i = 0; config.core.options[i].key; i++) {
		MinArchOption* option = &config.core.options[i];
		(void)fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}

	if (has_custom_controllers)
		(void)fprintf(file, "%s = %i\n", "minarch_gamepad_type", gamepad_type);

	for (int i = 0; config.controls[i].name; i++) {
		MinArchButtonMapping* mapping = &config.controls[i];
		int j = mapping->local_id + 1;
		if (mapping->modifier)
			j += LOCAL_BUTTON_COUNT;
		(void)fprintf(file, "bind %s = %s\n", mapping->name, minarch_button_labels[j]);
	}
	for (int i = 0; config.shortcuts[i].name; i++) {
		MinArchButtonMapping* mapping = &config.shortcuts[i];
		int j = mapping->local_id + 1;
		if (mapping->modifier)
			j += LOCAL_BUTTON_COUNT;
		(void)fprintf(file, "bind %s = %s\n", mapping->name, minarch_button_labels[j]);
	}

	(void)fclose(file); // Config file opened for writing
	sync();
}
static void Config_restore(void) {
	char path[MAX_PATH];
	if (config.loaded == CONFIG_GAME) {
		if (config.device_tag)
			(void)sprintf(path, "%s/%s-%s.cfg", core.config_dir, game.name, config.device_tag);
		else
			(void)sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
		(void)unlink(path); // Removing game config file
		LOG_info("Deleted game config: %s", path);
	} else if (config.loaded == CONFIG_CONSOLE) {
		if (config.device_tag)
			(void)sprintf(path, "%s/minarch-%s.cfg", core.config_dir, config.device_tag);
		else
			(void)sprintf(path, "%s/minarch.cfg", core.config_dir);
		(void)unlink(path); // Removing console config file
		LOG_info("Deleted console config: %s", path);
	}
	config.loaded = CONFIG_NONE;

	for (int i = 0; config.frontend.options[i].key; i++) {
		MinArchOption* option = &config.frontend.options[i];
		option->value = option->default_value;
		Config_syncFrontend(option->key, option->value);
	}
	for (int i = 0; config.core.options[i].key; i++) {
		MinArchOption* option = &config.core.options[i];
		option->value = option->default_value;
	}
	config.core.changed = 1; // let the core know

	if (has_custom_controllers) {
		gamepad_type = 0;
		core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	}

	for (int i = 0; config.controls[i].name; i++) {
		MinArchButtonMapping* mapping = &config.controls[i];
		mapping->local_id = mapping->default_id;
		mapping->modifier = 0;
	}
	for (int i = 0; config.shortcuts[i].name; i++) {
		MinArchButtonMapping* mapping = &config.shortcuts[i];
		mapping->local_id = BTN_ID_NONE;
		mapping->modifier = 0;
	}

	Config_load();
	Config_readOptions();
	Config_readControls();
	Config_free();

	renderer.dst_p = 0;
}

///////////////////////////////
static struct Special {
	int palette_updated;
} special;
static void Special_updatedDMGPalette(int frames) {
	// LOG_info("Special_updatedDMGPalette(%i)", frames);
	special.palette_updated = frames; // must wait a few frames
}
static void Special_refreshDMGPalette(void) {
	special.palette_updated -= 1;
	if (special.palette_updated > 0)
		return;

	int rgb = getInt("/tmp/dmg_grid_color");
	GFX_setEffectColor(rgb);
}
static void Special_init(void) {
	if (special.palette_updated > 1)
		special.palette_updated = 1;
	// else if (exactMatch((char*)core.tag, "GBC"))  {
	// 	putInt("/tmp/dmg_grid_color",0xF79E);
	// 	special.palette_updated = 1;
	// }
}
static void Special_render(void) {
	if (special.palette_updated)
		Special_refreshDMGPalette();
}
static void Special_quit(void) {
	system("rm -f /tmp/dmg_grid_color");
}
///////////////////////////////

static int MinArchOption_getValueIndex(MinArchOption* item, const char* value) {
	if (!value)
		return 0;
	for (int i = 0; i < item->count; i++) {
		if (!strcmp(item->values[i], value))
			return i;
	}
	return 0;
}
static void MinArchOption_setValue(MinArchOption* item, const char* value) {
	// TODO: store previous value?
	item->value = MinArchOption_getValueIndex(item, value);
}

// TODO: does this also need to be applied to MinArchOptionList_vars()?
static const char* option_key_name[] = {"pcsx_rearmed_analog_combo", "DualShock Toggle Combo",
                                        NULL};
static const char* getOptionNameFromKey(const char* key, const char* name) {
	char* _key = NULL;
	for (int i = 0; (_key = (char*)option_key_name[i]); i += 2) {
		if (exactMatch((char*)key, _key))
			return option_key_name[i + 1];
	}
	return name;
}

// the following 3 functions always touch config.core, the rest can operate on arbitrary MinArchOptionLists
static void MinArchOptionList_init(const struct retro_core_option_definition* defs) {
	LOG_debug("MinArchOptionList_init");
	int count;
	for (count = 0; defs[count].key; count++)
		;

	// LOG_info("count: %i", count);

	// TODO: add frontend options to this? so the can use the same override method? eg. minarch_*

	config.core.count = count;
	if (count) {
		config.core.options = calloc(count + 1, sizeof(MinArchOption));

		for (int i = 0; i < config.core.count; i++) {
			int len;
			const struct retro_core_option_definition* def = &defs[i];
			MinArchOption* item = &config.core.options[i];

			// Defensive check - should never happen if core reports options correctly
			if (!def->key) {
				LOG_error("Core option %d has NULL key", i);
				continue;
			}

			len = strlen(def->key) + 1;

			item->key = calloc(len, sizeof(char));
			safe_strcpy(item->key, def->key, len);

			len = strlen(def->desc) + 1;
			item->name = calloc(len, sizeof(char));
			safe_strcpy(item->name, getOptionNameFromKey(def->key, def->desc), len);

			if (def->info) {
				len = strlen(def->info) + 1;
				item->desc = calloc(len, sizeof(char));
				strncpy(item->desc, def->info, len);

				item->full = calloc(len, sizeof(char));
				strncpy(item->full, item->desc, len);
				// item->desc[len-1] = '\0';

				// these magic numbers are more about chars per line than pixel width
				// so it's not going to be relative to the screen size, only the scale
				// what does that even mean?
				GFX_wrapText(font.tiny, item->desc, DP(240), 2); // TODO magic number!
				GFX_wrapText(font.medium, item->full, DP(240), 7); // TODO: magic number!
			}

			for (count = 0; def->values[count].value; count++)
				;

			item->count = count;
			item->values = calloc(count + 1, sizeof(char*));
			item->labels = calloc(count + 1, sizeof(char*));

			for (int j = 0; j < count; j++) {
				const char* value = def->values[j].value;
				const char* label = def->values[j].label;

				len = strlen(value) + 1;
				item->values[j] = calloc(len, sizeof(char));
				safe_strcpy(item->values[j], value, len);

				if (label) {
					len = strlen(label) + 1;
					item->labels[j] = calloc(len, sizeof(char));
					safe_strcpy(item->labels[j], label, len);
				} else {
					item->labels[j] = item->values[j];
				}
				// printf("\t%s\n", item->labels[j]);
			}

			item->value = MinArchOption_getValueIndex(item, def->default_value);
			item->default_value = item->value;

			// LOG_info("\tINIT %s (%s) TO %s (%s)", item->name, item->key, item->labels[item->value], item->values[item->value]);
		}
	}
	// fflush(stdout);
}
static void MinArchOptionList_vars(const struct retro_variable* vars) {
	LOG_debug("MinArchOptionList_vars");
	int count;
	for (count = 0; vars[count].key; count++)
		;

	config.core.count = count;
	if (count) {
		config.core.options = calloc(count + 1, sizeof(MinArchOption));

		for (int i = 0; i < config.core.count; i++) {
			int len;
			const struct retro_variable* var = &vars[i];
			MinArchOption* item = &config.core.options[i];

			len = strlen(var->key) + 1;
			item->key = calloc(len, sizeof(char));
			safe_strcpy(item->key, var->key, len);

			len = strlen(var->value) + 1;
			item->var = calloc(len, sizeof(char));
			safe_strcpy(item->var, var->value, len);

			char* tmp = strchr(item->var, ';');
			if (tmp && *(tmp + 1) == ' ') {
				*tmp = '\0';
				item->name = item->var;
				tmp += 2;
			}

			char* opt = tmp;
			for (count = 0; (tmp = strchr(tmp, '|')); tmp++, count++)
				;
			count += 1; // last entry after final '|'

			item->count = count;
			item->values = calloc(count + 1, sizeof(char*));
			item->labels = calloc(count + 1, sizeof(char*));

			tmp = opt;
			int j;
			for (j = 0; (tmp = strchr(tmp, '|')); j++) {
				item->values[j] = opt;
				item->labels[j] = opt;
				*tmp = '\0';
				tmp += 1;
				opt = tmp;
			}
			item->values[j] = opt;
			item->labels[j] = opt;

			// no native default_value support for retro vars
			item->value = 0;
			item->default_value = item->value;
			// printf("SET %s to %s (%i)\n", item->key, default_value, item->value); fflush(stdout);
		}
	}
	// fflush(stdout);
}
static void MinArchOptionList_reset(void) {
	if (!config.core.count)
		return;

	for (int i = 0; i < config.core.count; i++) {
		MinArchOption* item = &config.core.options[i];
		if (item->var) {
			// values/labels are all points to var
			// so no need to free individually
			free(item->var);
		} else {
			if (item->desc)
				free(item->desc);
			if (item->full)
				free(item->full);
			for (int j = 0; j < item->count; j++) {
				char* value = item->values[j];
				char* label = item->labels[j];
				if (label != value)
					free(label);
				free(value);
			}
		}
		free(item->values);
		free(item->labels);
		free(item->key);
		free(item->name);
	}
	if (config.core.enabled_options)
		free(config.core.enabled_options);
	config.core.enabled_count = 0;
	free(config.core.options);
}

static MinArchOption* MinArchOptionList_getOption(MinArchOptionList* list, const char* key) {
	for (int i = 0; i < list->count; i++) {
		MinArchOption* item = &list->options[i];
		if (!strcmp(item->key, key))
			return item;
	}
	return NULL;
}
static char* MinArchOptionList_getOptionValue(MinArchOptionList* list, const char* key) {
	MinArchOption* item = MinArchOptionList_getOption(list, key);
	// if (item) LOG_info("\tGET %s (%s) = %s (%s)", item->name, item->key, item->labels[item->value], item->values[item->value]);

	if (item)
		return item->values[item->value];
	else
		LOG_warn("unknown option %s ", key);
	return NULL;
}
static void MinArchOptionList_setOptionRawValue(MinArchOptionList* list, const char* key,
                                                int value) {
	MinArchOption* item = MinArchOptionList_getOption(list, key);
	if (item) {
		item->value = value;
		list->changed = 1;
		// LOG_info("\tRAW SET %s (%s) TO %s (%s)", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);

		if (exactMatch((char*)core.tag, "GB") && containsString(item->key, "palette"))
			Special_updatedDMGPalette(3); // from options
	} else
		LOG_warn("unknown option %s", key);
}
static void MinArchOptionList_setOptionValue(MinArchOptionList* list, const char* key,
                                             const char* value) {
	MinArchOption* item = MinArchOptionList_getOption(list, key);
	if (item) {
		MinArchOption_setValue(item, value);
		list->changed = 1;
		// LOG_info("\tSET %s (%s) TO %s (%s)", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);

		if (exactMatch((char*)core.tag, "GB") && containsString(item->key, "palette"))
			Special_updatedDMGPalette(2); // from core
	} else
		LOG_warn("unknown option %s", key);
}
// static void MinArchOptionList_setOptionVisibility(MinArchOptionList* list, const char* key, int visible) {
// 	MinArchOption* item = MinArchOptionList_getOption(list, key);
// 	if (item) item->visible = visible;
// 	else printf("unknown option %s \n", key); fflush(stdout);
// }

///////////////////////////////////////
// Input Handling
///////////////////////////////////////

// Forward declarations for sleep handlers (defined later, used by PWR_update)
void Menu_beforeSleep(void);
void Menu_afterSleep(void);

static void Menu_saveState(void);
static void Menu_loadState(void);

/**
 * Enables or disables fast-forward mode.
 *
 * Fast-forward runs the emulator faster than real-time (up to max_ff_speed).
 * Handles interaction with threaded video mode:
 * - Entering FF with threaded video: disables threading temporarily
 * - Exiting FF: restores threading if it was previously enabled
 *
 * @param enable 1 to enable fast-forward, 0 to disable
 * @return The enable value (passthrough)
 *
 */
static int setFastForward(int enable) {
	fast_forward = enable;
	return enable;
}

static uint32_t buttons = 0; // Current button state (RETRO_DEVICE_ID_JOYPAD_* flags)
static int ignore_menu = 0; // Suppress menu button (used for shortcuts)

/**
 * Polls input devices and handles frontend shortcuts.
 *
 * Called by the libretro core before checking input state. Handles:
 * - Power/sleep management
 * - Menu button detection
 * - Fast-forward toggle (MENU + L2/R2)
 * - Save/load state shortcuts
 * - Screenshot capture
 * - Game reset
 *
 * Also translates platform button presses to libretro button flags.
 *
 * @note This is a libretro callback, invoked by core on each frame.
 * Also called by main loop as fallback for misbehaving cores.
 * Guard ensures it only runs once per frame.
 */
static void input_poll_callback(void) {
	if (input_polled_this_frame)
		return; // Already ran this frame

	input_polled_this_frame = 1;
	PAD_poll();

	int show_setting = 0;
	PWR_update(NULL, &show_setting, Menu_beforeSleep, Menu_afterSleep);

	// I _think_ this can stay as is...
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
	}
	if (PAD_isPressed(BTN_MENU) && (PAD_isPressed(BTN_PLUS) || PAD_isPressed(BTN_MINUS))) {
		ignore_menu = 1;
	}

	// this logic only works because TOGGLE_FF is before HOLD_FF in the menu...
	for (int i = 0; i < SHORTCUT_COUNT; i++) {
		MinArchButtonMapping* mapping = &config.shortcuts[i];
		int btn = 1 << mapping->local_id;
		if (btn == BTN_NONE)
			continue; // not bound
		if (!mapping->modifier || PAD_isPressed(BTN_MENU)) {
			if (i == SHORTCUT_TOGGLE_FF) {
				if (PAD_justPressed(btn)) {
					toggled_ff_on = setFastForward(!fast_forward);
					if (mapping->modifier)
						ignore_menu = 1;
					break;
				} else if (PAD_justReleased(btn)) {
					if (mapping->modifier)
						ignore_menu = 1;
					break;
				}
			} else if (i == SHORTCUT_HOLD_FF) {
				// don't allow turn off fast_forward with a release of the hold button
				// if it was initially turned on with the toggle button
				if (PAD_justPressed(btn) || (!toggled_ff_on && PAD_justReleased(btn))) {
					fast_forward = setFastForward(PAD_isPressed(btn));
					if (mapping->modifier)
						ignore_menu = 1; // very unlikely but just in case
				}
			} else if (PAD_justPressed(btn)) {
				switch (i) {
				case SHORTCUT_SAVE_STATE:
					Menu_saveState();
					break;
				case SHORTCUT_LOAD_STATE:
					Menu_loadState();
					break;
				case SHORTCUT_RESET_GAME:
					core.reset();
					break;
				case SHORTCUT_SAVE_QUIT:
					Menu_saveState();
					quit = 1;
					break;
				case SHORTCUT_CYCLE_SCALE:
					screen_scaling += 1;
					int count = config.frontend.options[FE_OPT_SCALING].count;
					if (screen_scaling >= count)
						screen_scaling -= count;
					Config_syncFrontend(config.frontend.options[FE_OPT_SCALING].key,
					                    screen_scaling);
					break;
				case SHORTCUT_CYCLE_EFFECT:
					screen_effect += 1;
					if (screen_effect >= EFFECT_COUNT)
						screen_effect -= EFFECT_COUNT;
					Config_syncFrontend(config.frontend.options[FE_OPT_EFFECT].key, screen_effect);
					break;
				default:
					break;
				}

				if (mapping->modifier)
					ignore_menu = 1;
			}
		}
	}

	if (!ignore_menu && PAD_justReleased(BTN_MENU)) {
		show_menu = 1;
	}

	// Translate platform buttons to libretro button flags for core
	buttons = 0;
	for (int i = 0; config.controls[i].name; i++) {
		MinArchButtonMapping* mapping = &config.controls[i];
		int btn = 1 << mapping->local_id;
		if (btn == BTN_NONE)
			continue; // present buttons can still be unbound
		if (gamepad_type == 0) {
			switch (btn) {
			case BTN_DPAD_UP:
				btn = BTN_UP;
				break;
			case BTN_DPAD_DOWN:
				btn = BTN_DOWN;
				break;
			case BTN_DPAD_LEFT:
				btn = BTN_LEFT;
				break;
			case BTN_DPAD_RIGHT:
				btn = BTN_RIGHT;
				break;
			}
		}
		if (PAD_isPressed(btn) && (!mapping->modifier || PAD_isPressed(BTN_MENU))) {
			buttons |= 1 << mapping->retro_id;
			if (mapping->modifier)
				ignore_menu = 1;
		}
		//  && !PWR_ignoreSettingInput(btn, show_setting)
	}

	// if (buttons) LOG_info("buttons: %i", buttons);
}
static int16_t input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0) {
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
			return buttons;
		return (buttons >> id) & 1;
	} else if (port == 0 && device == RETRO_DEVICE_ANALOG) {
		if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
			if (id == RETRO_DEVICE_ID_ANALOG_X)
				return pad.laxis.x;
			else if (id == RETRO_DEVICE_ID_ANALOG_Y)
				return pad.laxis.y;
		} else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
			if (id == RETRO_DEVICE_ID_ANALOG_X)
				return pad.raxis.x;
			else if (id == RETRO_DEVICE_ID_ANALOG_Y)
				return pad.raxis.y;
		}
	}
	return 0;
}
///////////////////////////////

static void Input_init(const struct retro_input_descriptor* vars) {
	static int input_initialized = 0;
	if (input_initialized)
		return;

	LOG_info("Input_init");

	config.controls =
	    core_button_mapping[0].name ? core_button_mapping : minarch_default_button_mapping;

	LOG_debug("---------------------------------");

	const char* core_button_names[RETRO_BUTTON_COUNT] = {0};
	int present[RETRO_BUTTON_COUNT];
	int core_mapped = 0;
	if (vars) {
		core_mapped = 1;
		// identify buttons available in this core
		for (int i = 0; vars[i].description; i++) {
			const struct retro_input_descriptor* var = &vars[i];
			if (var->port != 0 || var->device != RETRO_DEVICE_JOYPAD || var->index != 0)
				continue;

			// TODO: don't ignore unavailable buttons, just override them to BTN_ID_NONE!
			if (var->id >= RETRO_BUTTON_COUNT) {
				LOG_debug("UNAVAILABLE: %s", var->description);
				continue;
			} else {
				LOG_debug("PRESENT    : %s", var->description);
			}
			present[var->id] = 1;
			core_button_names[var->id] = var->description;
		}
	}

	LOG_debug("---------------------------------");

	for (int i = 0; minarch_default_button_mapping[i].name; i++) {
		MinArchButtonMapping* mapping = &minarch_default_button_mapping[i];
		LOG_debug("DEFAULT %s (%s): <%s>", core_button_names[mapping->retro_id], mapping->name,
		          (mapping->local_id == BTN_ID_NONE
		               ? "NONE"
		               : minarch_device_button_names[mapping->local_id]));
		if (core_button_names[mapping->retro_id])
			mapping->name = (char*)core_button_names[mapping->retro_id];
	}

	LOG_debug("---------------------------------");

	for (int i = 0; config.controls[i].name; i++) {
		MinArchButtonMapping* mapping = &config.controls[i];
		mapping->default_id = mapping->local_id;

		// ignore mappings that aren't available in this core
		if (core_mapped && !present[mapping->retro_id]) {
			mapping->ignore = 1;
			continue;
		}
		LOG_debug("%s: <%s> (%i:%i)", mapping->name,
		          (mapping->local_id == BTN_ID_NONE
		               ? "NONE"
		               : minarch_device_button_names[mapping->local_id]),
		          mapping->local_id, mapping->retro_id);
	}

	LOG_debug("---------------------------------");
	input_initialized = 1;
}

static bool set_rumble_state(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
	// TODO: handle other args? not sure I can
	VIB_setStrength(strength);
	return 1;
}

/**
 * Libretro log callback - maps libretro log levels to LessUI logging.
 *
 * Libretro log levels: DEBUG=0, INFO=1, WARN=2, ERROR=3
 */
static void retro_log_callback(enum retro_log_level level, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char msg_buffer[2048];
	(void)vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
	va_end(args);

	// Map libretro levels to our levels and log
	switch (level) {
	case RETRO_LOG_DEBUG:
		LOG_debug("%s", msg_buffer);
		break;
	case RETRO_LOG_INFO:
		LOG_info("%s", msg_buffer);
		break;
	case RETRO_LOG_WARN:
		LOG_warn("%s", msg_buffer);
		break;
	case RETRO_LOG_ERROR:
	default:
		LOG_error("%s", msg_buffer);
		// Capture error for display if we're loading
		if (capturing_core_errors && msg_buffer[0] != '\0') {
			setFatalError("%s", msg_buffer);
		}
		break;
	}
}

// Helper for audio reinitialization used by SET_SYSTEM_AV_INFO handler
static void env_reinit_audio(double old_rate, double new_rate, double fps) {
	(void)old_rate; // unused, we just need to know it changed
	SND_quit();
	SND_init(new_rate, fps);
}

static bool environment_callback(unsigned cmd, void* data) { // copied from picoarch initially
	// LOG_info("environment_callback: %i", cmd);
	EnvResult result;

	switch (cmd) {
	case RETRO_ENVIRONMENT_SET_ROTATION: { /* 1 */
		result = MinArchEnv_setRotation(&video_state, data);
		return result.success;
	}
	case RETRO_ENVIRONMENT_GET_OVERSCAN: { /* 2 */
		bool* out = (bool*)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: { /* 3 */
		bool* out = (bool*)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE: { /* 6 */
		const struct retro_message* message = (const struct retro_message*)data;
		if (message)
			LOG_info("%s", message->msg);
		break;
	}
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: { /* 8 */
		// puts("RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL");
		// TODO: used by fceumm at least
	}
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: { /* 9 */
		const char** out = (const char**)data;
		if (out) {
			*out = core.bios_dir;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: { /* 10 */
		result = MinArchEnv_setPixelFormat(&pixel_format, data);
		return result.success;
	}
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: { /* 11 */
		// puts("RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
		Input_init((const struct retro_input_descriptor*)data);
		return false;
	} break;
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: { /* 13 */
		MinArchEnv_setDiskControlInterface(&disk_control_ext, data);
		break;
	}

	// TODO: this is called whether using variables or options
	case RETRO_ENVIRONMENT_GET_VARIABLE: { /* 15 */
		// puts("RETRO_ENVIRONMENT_GET_VARIABLE ");
		struct retro_variable* var = (struct retro_variable*)data;
		if (var && var->key) {
			var->value = MinArchOptionList_getOptionValue(&config.core, var->key);
			// printf("\t%s = %s\n", var->key, var->value);
		}
		// fflush(stdout);
		break;
	}
	// TODO: I think this is where the core reports its variables (the precursor to options)
	// TODO: this is called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION sets out to 0
	// TODO: not used by anything yet
	case RETRO_ENVIRONMENT_SET_VARIABLES: { /* 16 */
		// puts("RETRO_ENVIRONMENT_SET_VARIABLES");
		const struct retro_variable* vars = (const struct retro_variable*)data;
		if (vars) {
			MinArchOptionList_reset();
			MinArchOptionList_vars(vars);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: { /* 18 */
		bool flag = *(bool*)data;
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %i", cmd, flag);
		break;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: { /* 17 */
		bool* out = (bool*)data;
		if (out) {
			*out = config.core.changed;
			config.core.changed = 0;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: { /* 21 */
		result = MinArchEnv_setFrameTimeCallback(&video_state, data);
		return result.success;
	}
	case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: { /* 22 */
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK", cmd);
		break;
	}
	case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: { /* 23 */
		struct retro_rumble_interface* iface = (struct retro_rumble_interface*)data;

		// LOG_info("Setup rumble interface.");
		iface->set_rumble_state = set_rumble_state;
		break;
	}
	case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
		unsigned* out = (unsigned*)data;
		if (out)
			*out = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
		break;
	}
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: { /* 27 */
		struct retro_log_callback* log_cb = (struct retro_log_callback*)data;
		if (log_cb)
			log_cb->log = retro_log_callback;
		break;
	}
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: { /* 31 */
		const char** out = (const char**)data;
		if (out)
			*out = core.saves_dir; // save_dir;
		break;
	}
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: { /* 32 */
		result =
		    MinArchEnv_setSystemAVInfo(&video_state, &core.fps, &core.sample_rate,
		                               &core.aspect_ratio, &renderer.dst_p, env_reinit_audio, data);
		return result.success;
	}
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: { /* 35 */
		result = MinArchEnv_setControllerInfo(&has_custom_controllers, data);
		return result.success; // Per libretro convention, returns false
	}
	case RETRO_ENVIRONMENT_SET_GEOMETRY: { /* 37 */
		result = MinArchEnv_setGeometry(&video_state, &renderer.dst_p, data);
		return result.success;
	}
	// RETRO_ENVIRONMENT_SET_MEMORY_MAPS (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_LANGUAGE 39
	case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: { /* (40 | RETRO_ENVIRONMENT_EXPERIMENTAL) */
		// puts("RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER");
		break;
	}

	case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
		// fixes fbneo save state graphics corruption
		MinArchEnv_getAudioVideoEnable(data);
		break;
	}

	// RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS (42 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	case RETRO_ENVIRONMENT_GET_FASTFORWARDING: { /* 49 */
		result = MinArchEnv_getFastforwarding(fast_forward, data);
		return result.success;
	}
	case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: { /* 50 | RETRO_ENVIRONMENT_EXPERIMENTAL */
		result = MinArchEnv_getTargetRefreshRate(core.fps, data);
		return result.success;
	}
	// RETRO_ENVIRONMENT_GET_INPUT_BITMASKS (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: { /* 51 | RETRO_ENVIRONMENT_EXPERIMENTAL */
		bool* out = (bool*)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: { /* 52 */
		// puts("RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION");
		unsigned* out = (unsigned*)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: { /* 53 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS");
		if (data) {
			MinArchOptionList_reset();
			MinArchOptionList_init((const struct retro_core_option_definition*)data);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: { /* 54 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL");
		const struct retro_core_options_intl* options = (const struct retro_core_options_intl*)data;
		if (options && options->us) {
			MinArchOptionList_reset();
			MinArchOptionList_init(options->us);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: { /* 55 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY");
		// const struct retro_core_option_display *display = (const struct retro_core_option_display *)data;
		// 	if (display) MinArchOptionList_setOptionVisibility(&config.core, display->key, display->visible);
		break;
	}
	case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: { /* 57 */
		unsigned* out = (unsigned*)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: { /* 58 */
		MinArchEnv_setDiskControlExtInterface(&disk_control_ext, data);
		break;
	}
	// TODO: RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
	case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: { /* 62 */
		MinArchEnv_setAudioBufferStatusCallback(&core.audio_buffer_status, data);
		return true;
	}
	// TODO: RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY 63 (buffer size adjustment)

	// TODO: RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE 64
	case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: { /* 65 */
		// const struct retro_system_content_info_override* info = (const struct retro_system_content_info_override* )data;
		// if (info) LOG_info("has overrides");
		break;
	}
	// RETRO_ENVIRONMENT_GET_GAME_INFO_EXT 66
	// TODO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK 69
	// used by fceumm
	// TODO: used by gambatte for L/R palette switching (seems like it needs to return true even if data is NULL to indicate support)
	case RETRO_ENVIRONMENT_SET_VARIABLE: {
		// puts("RETRO_ENVIRONMENT_SET_VARIABLE");
		const struct retro_variable* var = (const struct retro_variable*)data;
		if (var && var->key) {
			// printf("\t%s = %s\n", var->key, var->value);
			MinArchOptionList_setOptionValue(&config.core, var->key, var->value);
			break;
		}

		int* out = (int*)data;
		if (out)
			*out = 1;

		break;
	}
	case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: { /* 71 | RETRO_ENVIRONMENT_EXPERIMENTAL */
		MinArchThrottleInfo throttle = {.fast_forward = fast_forward, .max_ff_speed = max_ff_speed};
		result = MinArchEnv_getThrottleState(&throttle, data);
		return result.success;
	}

		// unused
		// case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
		// 	puts("RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK"); fflush(stdout);
		// 	break;
		// }
		// case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
		// 	puts("RETRO_ENVIRONMENT_GET_THROTTLE_STATE"); fflush(stdout);
		// 	break;
		// }
		// case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
		// 	puts("RETRO_ENVIRONMENT_GET_FASTFORWARDING"); fflush(stdout);
		// 	break;
		// };

	default:
		// LOG_debug("Unsupported environment cmd: %u", cmd);
		return false;
	}
	return true;
}

///////////////////////////////

void hdmimon(void) {
	// handle HDMI change
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi == -1)
		had_hdmi = has_hdmi;
	if (has_hdmi != had_hdmi) {
		had_hdmi = has_hdmi;

		LOG_info("restarting after HDMI change...");
		Menu_beforeSleep();
		sleep(4);
		show_menu = 0;
		quit = 1;
	}
}

///////////////////////////////

// TODO: this is a dumb API
SDL_Surface* digits;
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 8
#define DIGIT_TRACKING -2
enum {
	DIGIT_SLASH = 10,
	DIGIT_DOT,
	DIGIT_PERCENT,
	DIGIT_X,
	DIGIT_OP, // (
	DIGIT_CP, // )
	DIGIT_COUNT,
};
#define DIGIT_SPACE DIGIT_COUNT
static void MSG_init(void) {
	digits = SDL_CreateRGBSurface(SDL_SWSURFACE, DP2(DIGIT_WIDTH * DIGIT_COUNT, DIGIT_HEIGHT),
	                              FIXED_DEPTH, 0, 0, 0, 0);
	SDL_FillRect(digits, NULL, RGB_BLACK);

	SDL_Surface* digit;
	char* chars[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8",
	                 "9", "/", ".", "%", "x", "(", ")", NULL};
	char* c;
	int i = 0;
	while ((c = chars[i])) {
		digit = TTF_RenderUTF8_Blended(font.tiny, c, COLOR_WHITE);
		SDL_BlitSurface(digit, NULL, digits,
		                &(SDL_Rect){(i * DP(DIGIT_WIDTH)) + (DP(DIGIT_WIDTH) - digit->w) / 2,
		                            (DP(DIGIT_HEIGHT) - digit->h) / 2});
		SDL_FreeSurface(digit);
		i += 1;
	}
}
static int MSG_blitChar(int n, int x, int y) {
	if (n != DIGIT_SPACE)
		SDL_BlitSurface(digits, &(SDL_Rect){n * DP(DIGIT_WIDTH), 0, DP2(DIGIT_WIDTH, DIGIT_HEIGHT)},
		                screen, &(SDL_Rect){x, y});
	return x + DP(DIGIT_WIDTH + DIGIT_TRACKING);
}
static int MSG_blitInt(int num, int x, int y) {
	int i = num;
	int n;

	if (i > 999) {
		n = i / 1000;
		i -= n * 1000;
		x = MSG_blitChar(n, x, y);
	}
	if (i > 99) {
		n = i / 100;
		i -= n * 100;
		x = MSG_blitChar(n, x, y);
	} else if (num > 99) {
		x = MSG_blitChar(0, x, y);
	}
	if (i > 9) {
		n = i / 10;
		i -= n * 10;
		x = MSG_blitChar(n, x, y);
	} else if (num > 9) {
		x = MSG_blitChar(0, x, y);
	}

	n = i;
	x = MSG_blitChar(n, x, y);

	return x;
}
static int MSG_blitDouble(double num, int x, int y) {
	int i = num;
	int r = (num - i) * 10;
	int n;

	x = MSG_blitInt(i, x, y);

	n = DIGIT_DOT;
	x = MSG_blitChar(n, x, y);

	n = r;
	x = MSG_blitChar(n, x, y);
	return x;
}
static void MSG_quit(void) {
	SDL_FreeSurface(digits);
}

///////////////////////////////

static const char* bitmap_font[] = {
    ['0'] = " 111 "
            "1   1"
            "1   1"
            "1  11"
            "1 1 1"
            "11  1"
            "1   1"
            "1   1"
            " 111 ",
    ['1'] = "   1 "
            " 111 "
            "   1 "
            "   1 "
            "   1 "
            "   1 "
            "   1 "
            "   1 "
            "   1 ",
    ['2'] = " 111 "
            "1   1"
            "    1"
            "   1 "
            "  1  "
            " 1   "
            "1    "
            "1    "
            "11111",
    ['3'] = " 111 "
            "1   1"
            "    1"
            "    1"
            " 111 "
            "    1"
            "    1"
            "1   1"
            " 111 ",
    ['4'] = "1   1"
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            "11111"
            "    1"
            "    1",
    ['5'] = "11111"
            "1    "
            "1    "
            "1111 "
            "    1"
            "    1"
            "    1"
            "1   1"
            " 111 ",
    ['6'] = " 111 "
            "1    "
            "1    "
            "1111 "
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            " 111 ",
    ['7'] = "11111"
            "    1"
            "    1"
            "   1 "
            "  1  "
            "  1  "
            "  1  "
            "  1  "
            "  1  ",
    ['8'] = " 111 "
            "1   1"
            "1   1"
            "1   1"
            " 111 "
            "1   1"
            "1   1"
            "1   1"
            " 111 ",
    ['9'] = " 111 "
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            " 1111"
            "    1"
            "    1"
            " 111 ",
    ['.'] = "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            " 11  "
            " 11  ",
    [','] = "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            "  1  "
            "  1  "
            " 1   ",
    [' '] = "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            "     "
            "     ",
    ['('] = "   1 "
            "  1  "
            " 1   "
            " 1   "
            " 1   "
            " 1   "
            " 1   "
            "  1  "
            "   1 ",
    [')'] = " 1   "
            "  1  "
            "   1 "
            "   1 "
            "   1 "
            "   1 "
            "   1 "
            "  1  "
            " 1   ",
    ['/'] = "   1 "
            "   1 "
            "   1 "
            "  1  "
            "  1  "
            "  1  "
            " 1   "
            " 1   "
            " 1   ",
    ['x'] = "     "
            "     "
            "1   1"
            "1   1"
            " 1 1 "
            "  1  "
            " 1 1 "
            "1   1"
            "1   1",
    ['%'] = " 1   "
            "1 1  "
            "1 1 1"
            " 1 1 "
            "  1  "
            " 1 1 "
            "1 1 1"
            "  1 1"
            "   1 ",
    ['-'] = "     "
            "     "
            "     "
            "     "
            " 111 "
            "     "
            "     "
            "     "
            "     ",
    ['L'] = "1    "
            "1    "
            "1    "
            "1    "
            "1    "
            "1    "
            "1    "
            "1    "
            "11111",
    ['b'] = "1    "
            "1    "
            "1    "
            "1111 "
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            "1111 ",
    ['u'] = "     "
            "     "
            "     "
            "1   1"
            "1   1"
            "1   1"
            "1   1"
            "1  11"
            " 11 1",
    ['r'] = "     "
            "     "
            "     "
            "1 11 "
            "11  1"
            "1    "
            "1    "
            "1    "
            "1    ",
    [':'] = "     "
            "     "
            " 11  "
            " 11  "
            "     "
            "     "
            " 11  "
            " 11  "
            "     ",
    ['!'] = "  1  "
            "  1  "
            "  1  "
            "  1  "
            "  1  "
            "  1  "
            "     "
            "  1  "
            "  1  ",
    ['F'] = "11111"
            "1    "
            "1    "
            "1    "
            "1111 "
            "1    "
            "1    "
            "1    "
            "1    ",
    ['P'] = "1111 "
            "1   1"
            "1   1"
            "1   1"
            "1111 "
            "1    "
            "1    "
            "1    "
            "1    ",
    ['S'] = " 111 "
            "1   1"
            "1    "
            "1    "
            " 111 "
            "    1"
            "    1"
            "1   1"
            " 111 ",
    ['A'] = "  1  "
            " 1 1 "
            "1   1"
            "1   1"
            "11111"
            "1   1"
            "1   1"
            "1   1"
            "1   1",
    ['C'] = " 111 "
            "1   1"
            "1    "
            "1    "
            "1    "
            "1    "
            "1    "
            "1   1"
            " 111 ",
};
static void blitBitmapText(char* text, int ox, int oy, uint16_t* data, int stride, int width,
                           int height) {
#define CHAR_WIDTH 5
#define CHAR_HEIGHT 9
#define LETTERSPACING 1

	int len = strlen(text);
	int w = ((CHAR_WIDTH + LETTERSPACING) * len) - 1;
	int h = CHAR_HEIGHT;

	if (ox < 0)
		ox = width - w + ox;
	if (oy < 0)
		oy = height - h + oy;

	// Bounds check - need 1px margin for outline
	if (ox < 1 || oy < 1 || ox + w + 1 > width || oy + h + 1 > height)
		return;

	data += oy * stride + ox;
	uint16_t* row = data - stride;
	memset(row - 1, 0, (size_t)(w + 2) * 2);
	for (int y = 0; y < CHAR_HEIGHT; y++) {
		row = data + (ptrdiff_t)y * stride;
		memset(row - 1, 0, (size_t)(w + 2) * 2);
		for (int i = 0; i < len; i++) {
			const char* c = bitmap_font[(unsigned char)text[i]];
			for (int x = 0; x < CHAR_WIDTH; x++) {
				int j = y * CHAR_WIDTH + x;
				if (c[j] == '1')
					*row = 0xffff;
				row++;
			}
			row += LETTERSPACING;
		}
	}
	row = data + (ptrdiff_t)CHAR_HEIGHT * stride;
	memset(row - 1, 0, (size_t)(w + 2) * 2);
}

///////////////////////////////////////
// Video Processing
///////////////////////////////////////

// Performance counters for debug overlay and monitoring
static int cpu_ticks = 0;
static int fps_ticks = 0;
static int use_ticks = 0;
static double fps_double = 0;
static double cpu_double = 0;
static double use_double = 0; // System CPU usage percentage
static uint32_t sec_start = 0;

#ifdef USES_SWSCALER
static int fit = 1; // Use software scaler (fit to screen)
#else
static int fit = 0; // Use hardware scaler
#endif

// Helper macro: true if pixel format requires conversion to RGB565
#define NEEDS_CONVERSION ((pixel_format) != RETRO_PIXEL_FORMAT_RGB565)

// Pixel format conversion - implementation in minarch_video_convert.c
static void pixel_convert(const void* data, unsigned width, unsigned height, size_t pitch) {
	MinArchVideoConvert_convert(data, width, height, pitch, (MinArchPixelFormat)pixel_format);
}

// Wrapper function that passes video_state.rotation to the new module
static void* apply_rotation(void* src, uint32_t src_w, uint32_t src_h, uint32_t src_p) {
	return MinArchRotation_apply(video_state.rotation, src, src_w, src_h, src_p);
}

/**
 * Selects and configures the appropriate video scaler.
 *
 * Determines how to scale the core's output resolution to the device screen.
 * Handles multiple scaling modes (native, aspect, fullscreen, cropped) and
 * calculates source/destination rectangles for optimal display.
 *
 * Scaling modes:
 * - MINARCH_SCALE_NATIVE: 1:1 pixel mapping (may be cropped if game > screen)
 * - MINARCH_SCALE_ASPECT: Maintain aspect ratio with letterboxing
 * - SCALE_FULLSCREEN: Stretch to fill screen (may distort)
 * - MINARCH_SCALE_CROPPED: Crop to fill screen while maintaining aspect
 *
 * @param src_w Source width from core
 * @param src_h Source height from core
 * @param src_p Source pitch (bytes per scanline)
 *
 * @note Updates global 'renderer' structure with calculated values
 * @note Clears screen when scaler changes
 */
void selectScaler(int src_w, int src_h, int src_p) {
	if (NEEDS_CONVERSION)
		MinArchVideoConvert_allocBuffer(src_w, src_h);

	// Build input parameters for scaler calculation
	MinArchScalerInput input = {.src_w = src_w,
	                            .src_h = src_h,
	                            .src_p = src_p,
	                            .aspect_ratio = core.aspect_ratio,
	                            .rotation = (MinArchRotation)video_state.rotation,
	                            .mode = (MinArchScalerMode)screen_scaling,
	                            .device_w = DEVICE_WIDTH,
	                            .device_h = DEVICE_HEIGHT,
	                            .device_p = DEVICE_PITCH,
	                            .bpp = FIXED_BPP,
	                            .fit = fit,
	                            .buffer_w = VIDEO_BUFFER_WIDTH,
	                            .buffer_h = VIDEO_BUFFER_HEIGHT,
	                            .hdmi_width = HDMI_WIDTH};

	// Calculate scaling parameters
	MinArchScalerResult result;
	MinArchScaler_calculate(&input, &result);

	// Copy results to renderer
	renderer.src_x = result.src_x;
	renderer.src_y = result.src_y;
	renderer.src_w = result.src_w;
	renderer.src_h = result.src_h;
	renderer.src_p = result.src_p;
	renderer.dst_x = result.dst_x;
	renderer.dst_y = result.dst_y;
	renderer.dst_w = result.dst_w;
	renderer.dst_h = result.dst_h;
	renderer.dst_p = result.dst_p;
	renderer.scale = result.scale;
	renderer.aspect = result.aspect;
	renderer.true_w = result.true_w;
	renderer.true_h = result.true_h;

	LOG_debug("Scaler: %s %dx%d->%dx%d, scale=%d, aspect=%.2f", result.scaler_name, src_w, src_h,
	          result.dst_w, result.dst_h, result.scale, result.aspect);

	renderer.blit = GFX_getScaler(&renderer);

	// Adjust screen size for fit mode
	int final_w = result.dst_w;
	int final_h = result.dst_h;
	if (fit) {
		final_w = DEVICE_WIDTH;
		final_h = DEVICE_HEIGHT;
	}

	screen = GFX_resize(final_w, final_h, result.dst_p);
}
// Flag to indicate a frame was rendered and is ready for flip
// Set by video_refresh_callback, cleared after flip in main loop
int frame_ready_for_flip = 0;
static uint32_t last_blit_time = 0; // For FF frame skip (blit cost savings)

static void video_refresh_callback_main(const void* data, unsigned width, unsigned height,
                                        size_t pitch) {
	// return;

	Special_render();

	// static int tmp_frameskip = 0;
	// if ((tmp_frameskip++)%2) return;

	// During fast-forward, skip blitting if less than 10ms since last blit
	// This saves CPU/GPU cost from the blit operation itself
	// (The actual frame pacing is handled by limitFF() in main loop)
	if (fast_forward && SDL_GetTicks() - last_blit_time < 10)
		return;

	// FFVII menus
	// 16: 30/200
	// 15: 30/180
	// 14: 45/180
	// 12: 30/150
	// 10: 30/120 (optimize text off has no effect)
	//  8: 60/210 (with optimize text off)
	// you can squeeze more out of every console by turning prevent tearing off
	// eg. PS@10 60/240

	if (!data) {
		// Core skipped rendering, but still flip to maintain vsync cadence.
		// Without this, we skip vsync wait → 4ms frame → next frame waits 2 vblanks → 30ms
		// This causes 20% audio buffer oscillation even with perfect rate control.
		frame_ready_for_flip = 1;
		return;
	}

	fps_ticks += 1;

	// Calculate pitches for different stages
	// pitch = bytes per line as provided by core (varies by pixel format)
	// rgb565_pitch = bytes per line in RGB565 format (what renderer expects)
	size_t rgb565_pitch;

	if (NEEDS_CONVERSION) {
		// Core uses non-native format, we'll convert to RGB565 (2 bytes/pixel)
		rgb565_pitch = (size_t)width * FIXED_BPP;
	} else {
		// Core provided RGB565 directly, use as-is
		rgb565_pitch = pitch;
	}

	// if source has changed size (or forced by dst_p==0)
	// eg. true src + cropped src + fixed dst + cropped dst
	// Note: renderer.true_w/true_h hold ROTATED dimensions, so we need to compare carefully
	int expected_w = renderer.true_w;
	int expected_h = renderer.true_h;

	// Un-swap dimensions if rotation is active to compare against core output
	if (video_state.rotation == ROTATION_90 || video_state.rotation == ROTATION_270) {
		int temp = expected_w;
		expected_w = expected_h;
		expected_h = temp;
	}

	if (renderer.dst_p == 0 || (int)width != expected_w || (int)height != expected_h) {
		if ((int)width != expected_w || (int)height != expected_h) {
			LOG_debug("Video dimensions changed: %dx%d -> %ux%u", expected_w, expected_h, width,
			          height);
		}
		selectScaler(width, height, rgb565_pitch);
		GFX_clearAll();
	}

	// Perform pixel format conversion if needed (after buffer is allocated)
	void* frame_data;
	size_t frame_pitch;

	if (NEEDS_CONVERSION) {
		pixel_convert(data, width, height, pitch);
		frame_data = MinArchVideoConvert_getBuffer();
		frame_pitch = rgb565_pitch;
	} else {
		frame_data = (void*)data;
		frame_pitch = rgb565_pitch;
	}

	// Apply software rotation if needed
	void* rotated_data = apply_rotation(frame_data, width, height, frame_pitch);

	// Update pitch in renderer if rotation was applied
	// The rotation buffer always uses tightly-packed pitch regardless of rotation angle
	if (rotated_data != frame_data) {
		renderer.src_p = MinArchRotation_getBuffer()->pitch;
	}

	renderer.src = rotated_data;

	// debug - render after pixel conversion so we write to RGB565 buffer
	if (show_debug) {
		int x = 2 + renderer.src_x;
		int y = 2 + renderer.src_y;
		char debug_text[128];
		int scale = renderer.scale;
		if (scale == -1)
			scale = 1; // nearest neighbor flag

		// Debug text rendering needs correct buffer dimensions and pitch.
		// blitBitmapText expects pitch in pixels (uint16_t), not bytes.
		//
		// After 90°/270° rotation, the buffer dimensions are swapped (width becomes height
		// and vice versa) because the image has been rotated. We detect this by checking if
		// rotated_data != frame_data (indicating rotation was actually applied).
		//
		// blitBitmapText needs the post-rotation dimensions to correctly bounds-check text
		// rendering, and the rotation buffer's pitch instead of the original pitch.
		int pitch_in_pixels;
		int debug_width = width;
		int debug_height = height;

		if (rotated_data != frame_data) {
			// Use rotation buffer pitch when rotation was applied
			pitch_in_pixels = MinArchRotation_getBuffer()->pitch / sizeof(uint16_t);
			if (video_state.rotation == ROTATION_90 || video_state.rotation == ROTATION_270) {
				// Swap dimensions for 90°/270° rotations
				debug_width = height;
				debug_height = width;
			}
		} else {
			// Use original pitch when rotation was skipped
			pitch_in_pixels = rgb565_pitch / sizeof(uint16_t);
		}

		// Get buffer fill (sampled every 15 frames for readability)
		static unsigned fill_display = 0;
		static int sample_count = 0;
		if (++sample_count >= 15) {
			sample_count = 0;
			fill_display = SND_getBufferOccupancy();
		}

		// Top-left: FPS and system CPU %
#ifdef SYNC_MODE_AUDIOCLOCK
		(void)sprintf(debug_text, "%.0f FPS %i%% AC", fps_double, (int)use_double);
#else
		(void)sprintf(debug_text, "%.0f FPS %i%%", fps_double, (int)use_double);
#endif
		blitBitmapText(debug_text, x, y, (uint16_t*)renderer.src, pitch_in_pixels, debug_width,
		               debug_height);

		// Top-right: Source resolution and scale factor
		(void)sprintf(debug_text, "%ix%i %ix", renderer.src_w, renderer.src_h, scale);
		blitBitmapText(debug_text, -x, y, (uint16_t*)renderer.src, pitch_in_pixels, debug_width,
		               debug_height);

		// Bottom-left: CPU info + buffer fill (always), plus utilization when auto
		if (overclock == 3) {
			// Auto CPU mode: show frequency/level, utilization, and buffer fill
			pthread_mutex_lock(&auto_cpu_mutex);
			int current_idx = auto_cpu_state.current_index;
			int level = auto_cpu_state.current_level;
			pthread_mutex_unlock(&auto_cpu_mutex);

			// Calculate current utilization from most recent frame times
			unsigned util = 0;
			int samples = (auto_cpu_state.frame_time_index < auto_cpu_config.window_frames)
			                  ? auto_cpu_state.frame_time_index
			                  : auto_cpu_config.window_frames;
			if (samples >= 5 && auto_cpu_state.frame_budget_us > 0) {
				uint64_t p90 = percentileUint64(auto_cpu_state.frame_times, samples, 0.90f);
				util = (unsigned)((p90 * 100) / auto_cpu_state.frame_budget_us);
				if (util > 200)
					util = 200;
			}

			if (auto_cpu_state.use_granular && current_idx >= 0 &&
			    current_idx < auto_cpu_state.freq_count) {
				// Granular mode: show frequency in MHz (e.g., "1200" for 1200 MHz)
				int freq_mhz = auto_cpu_state.frequencies[current_idx] / 1000;
				(void)sprintf(debug_text, "%i u:%u%% b:%u%%", freq_mhz, util, fill_display);
			} else {
				// Fallback mode: show level
				(void)sprintf(debug_text, "L%i u:%u%% b:%u%%", level, util, fill_display);
			}
		} else {
			// Manual mode: show level and buffer fill (overclock 0/1/2 maps to L0/L1/L2)
			(void)sprintf(debug_text, "L%i b:%u%%", overclock, fill_display);
		}
		blitBitmapText(debug_text, x, -y, (uint16_t*)renderer.src, pitch_in_pixels, debug_width,
		               debug_height);

		// Bottom-right: Output resolution
		(void)sprintf(debug_text, "%ix%i", renderer.dst_w, renderer.dst_h);
		blitBitmapText(debug_text, -x, -y, (uint16_t*)renderer.src, pitch_in_pixels, debug_width,
		               debug_height);
	}
	renderer.dst = screen->pixels;
	// LOG_info("video_refresh_callback: %ix%i@%i %ix%i@%i",width,height,pitch,screen->w,screen->h,screen->pitch);

	GFX_blitRenderer(&renderer);
	last_blit_time = SDL_GetTicks();
	frame_ready_for_flip = 1; // Signal main loop to flip
	// NOTE: GFX_flip moved to main loop - see "Decouple vsync from core.run()" change
}

/**
 * Video refresh callback from libretro core.
 *
 * Receives rendered frame from core and passes it to the main rendering function.
 *
 * @param data Pointer to pixel data (format depends on pixel_format setting)
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param pitch Bytes per scanline
 *
 * @note This is a libretro callback, invoked by core after rendering a frame
 */
void video_refresh_callback(const void* data, unsigned width, unsigned height, size_t pitch) {
	if (!data)
		return;

	video_refresh_callback_main(data, width, height, pitch);
}

///////////////////////////////////////
// Audio Callbacks
///////////////////////////////////////

/**
 * Single audio sample callback from libretro core.
 *
 * Receives individual stereo samples. Most cores use the batch callback instead.
 *
 * @param left Left channel sample (-32768 to 32767)
 * @param right Right channel sample (-32768 to 32767)
 *
 * @note Audio disabled during fast-forward for performance
 */
static void audio_sample_callback(int16_t left, int16_t right) {
	if (!fast_forward)
		SND_batchSamples(&(const SND_Frame){left, right}, 1);
}

/**
 * Batch audio samples callback from libretro core.
 *
 * Receives multiple stereo samples at once for efficiency. This is the
 * primary audio submission method used by most cores.
 *
 * @param data Pointer to interleaved stereo samples (L,R,L,R,...)
 * @param frames Number of stereo frames (not individual samples)
 * @return Number of frames consumed (always returns frames)
 *
 * @note Audio disabled during fast-forward for performance
 * @note Data format: int16_t[frames * 2] interleaved stereo
 */
static size_t audio_sample_batch_callback(const int16_t* data, size_t frames) {
	if (!fast_forward)
		return SND_batchSamples((const SND_Frame*)data, frames);
	else
		return frames;
	// return frames;
};

///////////////////////////////////////
// Core Management
///////////////////////////////////////

/**
 * Extracts core name from filename.
 *
 * Core files are named like "core_libretro.so" - this extracts "core".
 *
 * @param in_name Input filename (e.g., "fceumm_libretro.so")
 * @param out_name Output buffer for core name (e.g., "fceumm")
 */
void Core_getName(char* in_name, char* out_name) {
	safe_strcpy(out_name, basename(in_name), 128); // Core names are at most 128 bytes
	char* tmp = strrchr(out_name, '_');
	tmp[0] = '\0';
}

/**
 * Selects appropriate BIOS directory path with smart fallback.
 *
 * Implements intelligent BIOS path selection:
 * - If Bios/<tag>/ exists and contains files → use tag-specific directory
 * - If Bios/<tag>/ is empty or only has .keep/.gitkeep → fall back to Bios/ root
 * - If Bios/<tag>/ doesn't exist → fall back to Bios/ root
 *
 * This allows both organized users (per-platform directories) and messy
 * users (all BIOS files in root) to work seamlessly.
 *
 * @param tag Platform tag (e.g., "GB", "PS", "N64")
 * @param bios_dir Output buffer for selected BIOS directory path
 */
void MinArch_selectBiosPath(const char* tag, char* bios_dir) {
	char tag_bios_dir[MAX_PATH];
	MinArchPaths_getTagBios(SDCARD_PATH "/Bios", tag, tag_bios_dir);

	int has_files = MinUI_hasNonHiddenFiles(tag_bios_dir);
	MinArchPaths_chooseBios(SDCARD_PATH "/Bios", tag, bios_dir, has_files);

	if (has_files) {
		LOG_info("Using tag-specific BIOS directory: %s", bios_dir);
	} else {
		LOG_info("Tag directory empty, falling back to root BIOS directory: %s", bios_dir);
	}
}

/**
 * Loads a libretro core from disk and resolves API functions.
 *
 * Opens the .so file using dlopen() and resolves all required libretro
 * API function pointers using dlsym(). Also sets up directory paths for
 * saves, states, config, and BIOS files based on platform and core name.
 *
 * BIOS path uses smart fallback: if Bios/<tag>/ contains files, use it;
 * otherwise fall back to Bios/ root (for users with messy BIOS folders).
 *
 * @param core_path Full path to core .so file
 * @param tag_name Platform tag (e.g., "GB", "NES")
 *
 * @note Exits on failure (core is required to continue)
 */
void Core_open(const char* core_path, const char* tag_name) {
	LOG_info("Core_open");
	core.handle = dlopen(core_path, RTLD_LAZY);

	if (!core.handle) {
		const char* error = dlerror();
		LOG_error("%s", error);
		setFatalError("Failed to load core\n%s", error);
		return;
	}

	core.init = dlsym(core.handle, "retro_init");
	core.deinit = dlsym(core.handle, "retro_deinit");
	core.get_system_info = dlsym(core.handle, "retro_get_system_info");
	core.get_system_av_info = dlsym(core.handle, "retro_get_system_av_info");
	core.set_controller_port_device = dlsym(core.handle, "retro_set_controller_port_device");
	core.reset = dlsym(core.handle, "retro_reset");
	core.run = dlsym(core.handle, "retro_run");
	core.serialize_size = dlsym(core.handle, "retro_serialize_size");
	core.serialize = dlsym(core.handle, "retro_serialize");
	core.unserialize = dlsym(core.handle, "retro_unserialize");
	core.load_game = dlsym(core.handle, "retro_load_game");
	core.load_game_special = dlsym(core.handle, "retro_load_game_special");
	core.unload_game = dlsym(core.handle, "retro_unload_game");
	core.get_region = dlsym(core.handle, "retro_get_region");
	core.get_memory_data = dlsym(core.handle, "retro_get_memory_data");
	core.get_memory_size = dlsym(core.handle, "retro_get_memory_size");

	void (*set_environment_callback)(retro_environment_t);
	void (*set_video_refresh_callback)(retro_video_refresh_t);
	void (*set_audio_sample_callback)(retro_audio_sample_t);
	void (*set_audio_sample_batch_callback)(retro_audio_sample_batch_t);
	void (*set_input_poll_callback)(retro_input_poll_t);
	void (*set_input_state_callback)(retro_input_state_t);

	set_environment_callback = dlsym(core.handle, "retro_set_environment");
	set_video_refresh_callback = dlsym(core.handle, "retro_set_video_refresh");
	set_audio_sample_callback = dlsym(core.handle, "retro_set_audio_sample");
	set_audio_sample_batch_callback = dlsym(core.handle, "retro_set_audio_sample_batch");
	set_input_poll_callback = dlsym(core.handle, "retro_set_input_poll");
	set_input_state_callback = dlsym(core.handle, "retro_set_input_state");

	struct retro_system_info info = {};
	core.get_system_info(&info);

	Core_getName((char*)core_path, (char*)core.name);
	(void)sprintf((char*)core.version, "%s (%s)", info.library_name, info.library_version);
	safe_strcpy((char*)core.tag, tag_name, sizeof(core.tag));
	safe_strcpy((char*)core.extensions, info.valid_extensions, sizeof(core.extensions));

	core.need_fullpath = info.need_fullpath;

	LOG_info("core: %s version: %s tag: %s (valid_extensions: %s need_fullpath: %i)", core.name,
	         core.version, core.tag, info.valid_extensions, info.need_fullpath);

	(void)sprintf((char*)core.config_dir, USERDATA_PATH "/%s-%s", core.tag, core.name);
	(void)sprintf((char*)core.states_dir, SHARED_USERDATA_PATH "/%s-%s", core.tag, core.name);
	(void)sprintf((char*)core.saves_dir, SDCARD_PATH "/Saves/%s", core.tag);
	MinArch_selectBiosPath(core.tag, (char*)core.bios_dir);

	char cmd[512];
	(void)sprintf(cmd, "mkdir -p \"%s\"; mkdir -p \"%s\"", core.config_dir, core.states_dir);
	system(cmd);

	set_environment_callback(environment_callback);
	set_video_refresh_callback(video_refresh_callback);
	set_audio_sample_callback(audio_sample_callback);
	set_audio_sample_batch_callback(audio_sample_batch_callback);
	set_input_poll_callback(input_poll_callback);
	set_input_state_callback(input_state_callback);
}
void Core_init(void) {
	LOG_info("Core_init");
	core.init();
	core.initialized = 1;
}

/**
 * Signal handler for catching segfaults during core loading.
 * Allows graceful error display instead of silent crash.
 */
static void core_load_segfault_handler(int sig) {
	(void)sig;
	if (in_core_load) {
		siglongjmp(segfault_jmp, 1);
	}
	// Not in core_load - terminate immediately using async-signal-safe function
	_exit(128 + SIGSEGV);
}

bool Core_load(void) {
	LOG_info("Core_load");
	struct retro_game_info game_info;
	MinArchCore_buildGameInfo(&game, &game_info);
	LOG_info("game path: %s (%i)", game_info.path, game_info.size);

	// Set up segfault handler to catch core crashes during load_game().
	// Some cores crash when given invalid ROM data or unsupported formats.
	// Without this, the user would see a silent crash; with it, we can
	// display a helpful error message before exiting gracefully.
	LOG_debug("Setting up SIGSEGV handler for core load");
	struct sigaction sa, old_sa;
	sa.sa_handler = core_load_segfault_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGSEGV, &sa, &old_sa);

	capturing_core_errors = 1;
	in_core_load = 1;

	LOG_debug("Calling core.load_game");
	bool success;
	if (sigsetjmp(segfault_jmp, 1) == 0) {
		// Normal path
		success = core.load_game(&game_info);
		LOG_debug("core.load_game returned %s", success ? "true" : "false");
	} else {
		// Caught segfault - core crashed
		LOG_error("Core crashed during load_game (SIGSEGV caught)");
		success = false;
		if (fatal_error_detail[0] == '\0') {
			setFatalError("Core crashed during initialization");
		}
	}

	in_core_load = 0;
	capturing_core_errors = 0;
	sigaction(SIGSEGV, &old_sa, NULL); // Restore old handler
	LOG_debug("Restored old SIGSEGV handler");

	if (!success) {
		LOG_debug("Core_load failed");
		if (fatal_error_detail[0] == '\0') {
			setFatalError("Core could not be initialized");
		}
		return false;
	}

	SRAM_read();
	RTC_read();

	// NOTE: must be called after core.load_game!
	struct retro_system_av_info av_info = {};
	core.get_system_av_info(&av_info);
	core.set_controller_port_device(
	    0, RETRO_DEVICE_JOYPAD); // set a default, may update after loading configs

	// Process AV info
	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av_info, &info);
	core.fps = info.fps;
	core.sample_rate = info.sample_rate;
	core.aspect_ratio = info.aspect_ratio;

	LOG_info("aspect_ratio: %f (%ix%i) fps: %f", info.aspect_ratio, av_info.geometry.base_width,
	         av_info.geometry.base_height, core.fps);
	return true;
}
void Core_reset(void) {
	core.reset();
}
void Core_unload(void) {
	SND_quit();
}
void Core_quit(void) {
	if (core.initialized) {
		SRAM_write();
		RTC_write();
		core.unload_game();
		core.deinit();
		core.initialized = 0;
	}
}
void Core_close(void) {
	// Stop auto CPU scaling thread if running
	auto_cpu_stopThread();

	// Free pixel format conversion buffer
	MinArchVideoConvert_freeBuffer();

	// Free rotation buffer
	MinArchRotation_freeBuffer();

	// Reset pixel format to default for next core
	pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

	if (core.handle)
		dlclose(core.handle);
}

///////////////////////////////////////
// Menu init/quit wrappers (implementation in minarch_menu.c)
///////////////////////////////////////

void Menu_init(void) {
	MinArchMenu_init(MinArchContext_get());
}

void Menu_quit(void) {
	MinArchMenu_quit(MinArchContext_get());
}

///////////////////////////////////////
// Sleep handlers - delegate to minarch_menu.c (which uses callbacks)
///////////////////////////////////////

void Menu_beforeSleep(void) {
	MinArchMenu_beforeSleep(MinArchContext_get());
}

void Menu_afterSleep(void) {
	MinArchMenu_afterSleep(MinArchContext_get());
}

// Menu types defined in minarch_menu_types.h

int Menu_message(const char* message, char** pairs) {
	GFX_setMode(MODE_MAIN);
	int dirty = 1;
	while (1) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B))
			break;

		PWR_update(&dirty, NULL, Menu_beforeSleep, Menu_afterSleep);

		if (dirty) {
			GFX_clear(screen);
			GFX_blitMessage(font.medium, message, screen,
			                &(SDL_Rect){0, DP(ui.edge_padding), DP(ui.screen_width),
			                            DP(ui.screen_height - ui.pill_height - ui.edge_padding)});
			GFX_blitButtonGroup(pairs, 0, screen, 1);
			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();

		hdmimon();
	}
	GFX_setMode(MODE_MENU);
	return MENU_CALLBACK_NOP; // TODO: this should probably be an arg
}

int Menu_options(MenuList* list);

static int MenuList_freeItems(MenuList* list, int i) {
	// TODO: what calls this? do menu's register for needing it? then call it on quit for each?
	if (list->items)
		free(list->items);
	return MENU_CALLBACK_NOP;
}

///////////////////////////////
// Generic option menu helper (reduces duplication)
///////////////////////////////

/**
 * Build and display an options menu from an MinArchOptionList.
 *
 * Handles the common pattern of:
 * 1. Lazy initialization of enabled_options (filtering locked options)
 * 2. Creating/updating MenuItems from options
 * 3. Displaying Menu_options() or showing "no options" message
 *
 * This consolidates the identical code in OptionFrontend_openMenu
 * and OptionEmulator_openMenu.
 *
 * @param source MinArchOptionList to build menu from (config.frontend or config.core)
 * @param menu MenuList to populate/update
 * @param no_options_msg Message to show if no options available (NULL to skip check)
 * @return MENU_CALLBACK_NOP
 */
static int OptionsMenu_buildAndShow(MinArchOptionList* source, MenuList* menu,
                                    const char* no_options_msg) {
	// Build enabled_options list if not already built
	if (!source->enabled_count) {
		int enabled_count = 0;
		for (int idx = 0; idx < source->count; idx++) {
			if (!source->options[idx].lock)
				enabled_count += 1;
		}
		source->enabled_count = enabled_count;
		source->enabled_options = calloc(enabled_count + 1, sizeof(MinArchOption*));
		int j = 0;
		for (int idx = 0; idx < source->count; idx++) {
			MinArchOption* item = &source->options[idx];
			if (item->lock)
				continue;
			source->enabled_options[j] = item;
			j += 1;
		}
	}

	// Create MenuItems on first call
	if (menu->items == NULL) {
		menu->items = calloc(source->enabled_count + 1, sizeof(MenuItem));
		if (!menu->items)
			return MENU_CALLBACK_NOP;

		for (int j = 0; j < source->enabled_count; j++) {
			MinArchOption* option = source->enabled_options[j];
			MenuItem* item = &menu->items[j];
			item->key = option->key;
			item->name = option->name;
			item->desc = option->desc;
			item->value = option->value;
			item->values = option->labels;
		}
	} else {
		// Update values on subsequent calls
		for (int j = 0; j < source->enabled_count; j++) {
			MinArchOption* option = source->enabled_options[j];
			MenuItem* item = &menu->items[j];
			item->value = option->value;
		}
	}

	// Show menu or "no options" message
	if (no_options_msg && menu->items[0].name == NULL) {
		Menu_message(no_options_msg, (char*[]){"B", "BACK", NULL});
	} else if (menu->items[0].name) {
		Menu_options(menu);
	} else if (no_options_msg) {
		Menu_message(no_options_msg, (char*[]){"B", "BACK", NULL});
	}

	return MENU_CALLBACK_NOP;
}

static int OptionFrontend_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Config_syncFrontend(item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static MenuList OptionFrontend_menu = {
    .type = MENU_VAR,
    .max_width = 0,
    .desc = NULL,
    .items = NULL,
    .on_confirm = NULL,
    .on_change = OptionFrontend_optionChanged,
};
static int OptionFrontend_openMenu(MenuList* list, int i) {
	(void)list;
	(void)i;
	return OptionsMenu_buildAndShow(&config.frontend, &OptionFrontend_menu, NULL);
}

static int OptionEmulator_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	// NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) - option IS used in LOG_info below
	MinArchOption* option = MinArchOptionList_getOption(&config.core, item->key);
	if (option) {
		LOG_info("%s (%s) changed from `%s` (%s) to `%s` (%s)", item->name, item->key,
		         item->values[option->value], option->values[option->value],
		         item->values[item->value], option->values[item->value]);
	}
	MinArchOptionList_setOptionRawValue(&config.core, item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static int OptionEmulator_optionDetail(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	MinArchOption* option = MinArchOptionList_getOption(&config.core, item->key);
	if (option && option->full)
		return Menu_message(option->full, (char*[]){"B", "BACK", NULL});
	else
		return MENU_CALLBACK_NOP;
}
static MenuList OptionEmulator_menu = {
    .type = MENU_FIXED,
    .max_width = 0,
    .desc = NULL,
    .items = NULL,
    .on_confirm = OptionEmulator_optionDetail, // TODO: this needs pagination to be truly useful
    .on_change = OptionEmulator_optionChanged,
};
static int OptionEmulator_openMenu(MenuList* list, int i) {
	(void)list;
	(void)i;
	return OptionsMenu_buildAndShow(&config.core, &OptionEmulator_menu,
	                                "This core has no options.");
}

int OptionControls_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values != minarch_button_labels) {
		// LOG_info("changed gamepad_type");
		return MENU_CALLBACK_NOP;
	}

	MinArchButtonMapping* button = &config.controls[item->id];

	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();

		// NOTE: off by one because of the initial NONE value
		for (int id = 0; id <= LOCAL_BUTTON_COUNT; id++) {
			if (id > 0 && PAD_justPressed(1 << (id - 1))) {
				item->value = id;
				button->local_id = id - 1;
				if (PAD_isPressed(BTN_MENU)) {
					item->value += LOCAL_BUTTON_COUNT;
					button->modifier = 1;
				} else {
					button->modifier = 0;
				}
				bound = 1;
				break;
			}
		}
		GFX_sync();
		hdmimon();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionControls_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values != minarch_button_labels)
		return MENU_CALLBACK_NOP;

	MinArchButtonMapping* button = &config.controls[item->id];
	button->local_id = -1;
	button->modifier = 0;
	return MENU_CALLBACK_NOP;
}
static int OptionControls_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values != minarch_gamepad_labels)
		return MENU_CALLBACK_NOP;

	if (has_custom_controllers) {
		gamepad_type = item->value;
		int device = strtol(minarch_gamepad_values[item->value], NULL, 0);
		core.set_controller_port_device(0, device);
	}
	return MENU_CALLBACK_NOP;
}
static MenuList OptionControls_menu = {
    .type = MENU_INPUT,
    .max_width = 0,
    .desc =
        "Press A to set and X to clear."
        "\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
    ,
    .items = NULL,
    .on_confirm = OptionControls_bind,
    .on_change = OptionControls_unbind};
static int OptionControls_openMenu(MenuList* list, int i) {
	LOG_info("OptionControls_openMenu");

	if (OptionControls_menu.items == NULL) {
		// TODO: where do I free this?
		OptionControls_menu.items =
		    calloc(RETRO_BUTTON_COUNT + 1 + has_custom_controllers, sizeof(MenuItem));
		int k = 0;

		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->name = "Controller";
			item->desc = "Select the type of controller.";
			item->value = gamepad_type;
			item->values = minarch_gamepad_labels;
			item->on_change = OptionControls_optionChanged;
		}

		for (int j = 0; config.controls[j].name; j++) {
			MinArchButtonMapping* button = &config.controls[j];
			if (button->ignore)
				continue;

			LOG_info("\t%s (%i:%i)", button->name, button->local_id, button->retro_id);

			MenuItem* item = &OptionControls_menu.items[k++];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local_id + 1;
			if (button->modifier)
				item->value += LOCAL_BUTTON_COUNT;
			item->values = minarch_button_labels;
		}
	} else {
		// update values
		int k = 0;

		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = gamepad_type;
		}

		for (int j = 0; config.controls[j].name; j++) {
			MinArchButtonMapping* button = &config.controls[j];
			if (button->ignore)
				continue;

			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = button->local_id + 1;
			if (button->modifier)
				item->value += LOCAL_BUTTON_COUNT;
		}
	}
	Menu_options(&OptionControls_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionShortcuts_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	MinArchButtonMapping* button = &config.shortcuts[item->id];
	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();

		// NOTE: off by one because of the initial NONE value
		for (int id = 0; id <= LOCAL_BUTTON_COUNT; id++) {
			if (id > 0 && PAD_justPressed(1 << (id - 1))) {
				item->value = id;
				button->local_id = id - 1;
				if (PAD_isPressed(BTN_MENU)) {
					item->value += LOCAL_BUTTON_COUNT;
					button->modifier = 1;
				} else {
					button->modifier = 0;
				}
				bound = 1;
				break;
			}
		}
		GFX_sync();
		hdmimon();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionShortcuts_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	MinArchButtonMapping* button = &config.shortcuts[item->id];
	button->local_id = -1;
	button->modifier = 0;
	return MENU_CALLBACK_NOP;
}
static MenuList OptionShortcuts_menu = {
    .type = MENU_INPUT,
    .max_width = 0,
    .desc =
        "Press A to set and X to clear."
        "\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
    ,
    .items = NULL,
    .on_confirm = OptionShortcuts_bind,
    .on_change = OptionShortcuts_unbind};
static char* getSaveDesc(void) {
	return (char*)MinArchConfig_getStateDesc(config.loaded);
}
static int OptionShortcuts_openMenu(MenuList* list, int i) {
	if (OptionShortcuts_menu.items == NULL) {
		// TODO: where do I free this? I guess I don't :sweat_smile:
		OptionShortcuts_menu.items = calloc(SHORTCUT_COUNT + 1, sizeof(MenuItem));
		for (int j = 0; config.shortcuts[j].name; j++) {
			MinArchButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local_id + 1;
			if (button->modifier)
				item->value += LOCAL_BUTTON_COUNT;
			item->values = minarch_button_labels;
		}
	} else {
		// update values
		for (int j = 0; config.shortcuts[j].name; j++) {
			MinArchButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->value = button->local_id + 1;
			if (button->modifier)
				item->value += LOCAL_BUTTON_COUNT;
		}
	}
	Menu_options(&OptionShortcuts_menu);
	return MENU_CALLBACK_NOP;
}

static void OptionSaveChanges_updateDesc(void);
static int OptionSaveChanges_onConfirm(MenuList* list, int i) {
	char* message;
	switch (i) {
	case 0: {
		Config_write(CONFIG_WRITE_ALL);
		message = "Saved for console.";
		break;
	}
	case 1: {
		Config_write(CONFIG_WRITE_GAME);
		message = "Saved for game.";
		break;
	}
	default: {
		Config_restore();
		if (config.loaded)
			message = "Restored console defaults.";
		else
			message = "Restored defaults.";
		break;
	}
	}
	Menu_message(message, (char*[]){"A", "OKAY", NULL});
	OptionSaveChanges_updateDesc();
	return MENU_CALLBACK_EXIT;
}
static MenuList OptionSaveChanges_menu = {.type = MENU_LIST,
                                          .max_width = 0,
                                          .desc = NULL,
                                          .items =
                                              (MenuItem[]){
                                                  {.name = "Save for console",
                                                   .desc = NULL,
                                                   .values = NULL,
                                                   .key = NULL,
                                                   .id = 0,
                                                   .value = 0,
                                                   .submenu = NULL,
                                                   .on_confirm = NULL,
                                                   .on_change = NULL},
                                                  {.name = "Save for game",
                                                   .desc = NULL,
                                                   .values = NULL,
                                                   .key = NULL,
                                                   .id = 0,
                                                   .value = 0,
                                                   .submenu = NULL,
                                                   .on_confirm = NULL,
                                                   .on_change = NULL},
                                                  {.name = "Restore defaults",
                                                   .desc = NULL,
                                                   .values = NULL,
                                                   .key = NULL,
                                                   .id = 0,
                                                   .value = 0,
                                                   .submenu = NULL,
                                                   .on_confirm = NULL,
                                                   .on_change = NULL},
                                                  {.name = NULL,
                                                   .desc = NULL,
                                                   .values = NULL,
                                                   .key = NULL,
                                                   .id = 0,
                                                   .value = 0,
                                                   .submenu = NULL,
                                                   .on_confirm = NULL,
                                                   .on_change = NULL},
                                              },
                                          .on_confirm = OptionSaveChanges_onConfirm,
                                          .on_change = NULL};
static int OptionSaveChanges_openMenu(MenuList* list, int i) {
	OptionSaveChanges_updateDesc();
	OptionSaveChanges_menu.desc = getSaveDesc();
	Menu_options(&OptionSaveChanges_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionQuicksave_onConfirm(MenuList* list, int i) {
	Menu_beforeSleep();
	PWR_powerOff();
	return MENU_CALLBACK_NOP; // Never reached (device powers off)
}

MenuList options_menu = {.type = MENU_LIST,
                         .max_width = 0,
                         .desc = NULL,
                         .items =
                             (MenuItem[]){
                                 {.name = "Frontend",
                                  .desc = "MinUI (" BUILD_DATE " " BUILD_HASH ")",
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = OptionFrontend_openMenu,
                                  .on_change = NULL},
                                 {.name = "Emulator",
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = OptionEmulator_openMenu,
                                  .on_change = NULL},
                                 {.name = "Controls",
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = OptionControls_openMenu,
                                  .on_change = NULL},
                                 {.name = "Shortcuts",
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = OptionShortcuts_openMenu,
                                  .on_change = NULL},
                                 {.name = "Save Changes",
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = OptionSaveChanges_openMenu,
                                  .on_change = NULL},
                                 {.name = NULL,
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = NULL,
                                  .on_change = NULL},
                                 {.name = NULL,
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = NULL,
                                  .on_change = NULL},
                                 {.name = NULL,
                                  .desc = NULL,
                                  .values = NULL,
                                  .key = NULL,
                                  .id = 0,
                                  .value = 0,
                                  .submenu = NULL,
                                  .on_confirm = NULL,
                                  .on_change = NULL},
                             },
                         .on_confirm = NULL,
                         .on_change = NULL};

static void OptionSaveChanges_updateDesc(void) {
	options_menu.items[4].desc = getSaveDesc();
}

#define OPTION_PADDING 8

/**
 * Calculates optimal label and value widths using proportional truncation.
 *
 * Distributes available space fairly based on natural text sizes. If both fit,
 * uses natural sizes. Otherwise, distributes proportionally while enforcing
 * minimum allocations (25% for label, 20% for value).
 *
 * @param label_text Label string to measure
 * @param value_text Value string to measure (NULL if no value)
 * @param total_width Total available width in pixels
 * @param label_w_out Output: allocated width for label text
 * @param value_w_out Output: allocated width for value text
 */
static void calculateProportionalWidths(const char* label_text, const char* value_text,
                                        int total_width, int* label_w_out, int* value_w_out) {
	int natural_label_w, natural_value_w = 0;
	TTF_SizeUTF8(font.medium, label_text, &natural_label_w, NULL);
	if (value_text)
		TTF_SizeUTF8(font.small, value_text, &natural_value_w, NULL);

	int total_natural = natural_label_w + natural_value_w;

	if (total_natural <= total_width) {
		// Both fit! Use natural sizes
		*label_w_out = natural_label_w;
		*value_w_out = natural_value_w;
	} else {
		// Distribute proportionally based on natural ratio
		if (total_natural > 0) {
			*label_w_out = (total_width * natural_label_w) / total_natural;
			*value_w_out = (total_width * natural_value_w) / total_natural;
		} else {
			// Fallback: split 50/50 (should never happen)
			*label_w_out = total_width / 2;
			*value_w_out = total_width / 2;
		}

		// Enforce minimums (25% label, 20% value)
		int min_label = total_width / 4;
		int min_value = total_width / 5;

		if (*label_w_out < min_label) {
			*label_w_out = min_label;
			*value_w_out = total_width - *label_w_out;
		}
		if (*value_w_out < min_value) {
			*value_w_out = min_value;
			*label_w_out = total_width - *value_w_out;
		}
	}
}

int Menu_options(MenuList* list) {
	MenuItem* items = list->items;
	int type = list->type;

	int show_options = 1;
	int show_settings = 0;

	// dependent on option list offset top and bottom, eg. the gray triangles
	int max_visible_options = (ui.screen_height - (ui.edge_padding + ui.pill_height) * 2) /
	                          ui.option_size; // 7 for 480, 10 for 720

	int count;
	for (count = 0; items[count].name; count++)
		;

	// Initialize navigation state using extracted function
	MinArchMenuNavState nav;
	MinArchMenuNav_init(&nav, count, max_visible_options);

	OptionSaveChanges_updateDesc();

	int defer_menu = false;
	while (show_options) {
		if (nav.await_input) {
			defer_menu = true;
			if (list->on_confirm)
				list->on_confirm(list, nav.selected);

			MinArchMenuNav_advanceItem(&nav);
			nav.dirty = 1;
			nav.await_input = 0;
		}

		GFX_startFrame();
		PAD_poll();

		// Navigation input (up/down)
		if (PAD_justRepeated(BTN_UP)) {
			if (MinArchMenuNav_navigate(&nav, -1))
				nav.dirty = 1;
		} else if (PAD_justRepeated(BTN_DOWN)) {
			if (MinArchMenuNav_navigate(&nav, +1))
				nav.dirty = 1;
		} else {
			// Value cycling (left/right)
			MenuItem* item = &items[nav.selected];
			if (item->values && item->values != minarch_button_labels) { // not an input binding
				if (PAD_justRepeated(BTN_LEFT)) {
					if (MinArchMenuNav_cycleValue(item, -1)) {
						if (item->on_change)
							item->on_change(list, nav.selected);
						else if (list->on_change)
							list->on_change(list, nav.selected);
						nav.dirty = 1;
					}
				} else if (PAD_justRepeated(BTN_RIGHT)) {
					if (MinArchMenuNav_cycleValue(item, +1)) {
						if (item->on_change)
							item->on_change(list, nav.selected);
						else if (list->on_change)
							list->on_change(list, nav.selected);
						nav.dirty = 1;
					}
				}
			}
		}

		// Action buttons (A/B/X)
		MenuItem* item = &items[nav.selected];
		MinArchMenuAction action = MinArchMenuNav_getAction(
		    list, item, type, PAD_justPressed(BTN_A), PAD_justPressed(BTN_B),
		    PAD_justPressed(BTN_X), minarch_button_labels);

		switch (action) {
		case MENU_ACTION_EXIT:
			show_options = 0;
			break;

		case MENU_ACTION_CONFIRM: {
			int result = MENU_CALLBACK_NOP;
			if (item->on_confirm)
				result = item->on_confirm(list, nav.selected);
			else if (list->on_confirm)
				result = list->on_confirm(list, nav.selected);

			if (result == MENU_CALLBACK_EXIT)
				show_options = 0;
			else {
				if (result == MENU_CALLBACK_NEXT_ITEM)
					MinArchMenuNav_advanceItem(&nav);
				nav.dirty = 1;
			}
			break;
		}

		case MENU_ACTION_SUBMENU: {
			int result = Menu_options(item->submenu);
			if (result == MENU_CALLBACK_EXIT)
				show_options = 0;
			else
				nav.dirty = 1;
			break;
		}

		case MENU_ACTION_AWAIT_INPUT:
			nav.await_input = 1;
			break;

		case MENU_ACTION_CLEAR_INPUT:
			item->value = 0;
			if (item->on_change)
				item->on_change(list, nav.selected);
			else if (list->on_change)
				list->on_change(list, nav.selected);
			MinArchMenuNav_advanceItem(&nav);
			nav.dirty = 1;
			break;

		case MENU_ACTION_NONE:
		default:
			break;
		}

		if (!defer_menu)
			PWR_update(&nav.dirty, &show_settings, Menu_beforeSleep, Menu_afterSleep);

		if (defer_menu && PAD_justReleased(BTN_MENU))
			defer_menu = false;

		if (nav.dirty) {
			GFX_clear(screen);
			GFX_blitHardwareGroup(screen, show_settings);

			char* desc = NULL;
			SDL_Surface* text;

			if (type == MENU_LIST) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest item
					for (int i = 0; i < nav.count; i++) {
						MenuItem* loop_item = &items[i];
						int w = 0;
						TTF_SizeUTF8(font.medium, loop_item->name, &w, NULL);
						w += DP(OPTION_PADDING * 2);
						if (w > mw)
							mw = w;
					}
					// cache the result
					list->max_width = mw = MIN(mw, DP(ui.screen_width - ui.edge_padding * 2));
				}

				int ox = DP_CENTER_PX(ui.screen_width, mw);
				int oy = ui.edge_padding_px + ui.pill_height_px;
				int selected_row = nav.selected - nav.start;
				for (int i = nav.start, j = 0; i < nav.end; i++, j++) {
					MenuItem* loop_item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					// int ox = (screen->w - w) / 2; // if we're centering these (but I don't think we should after seeing it)
					if (j == selected_row) {
						// move out of conditional if centering
						int w = 0;
						TTF_SizeUTF8(font.medium, loop_item->name, &w, NULL);
						w += DP(OPTION_PADDING * 2);

						GFX_blitPill(
						    ASSET_OPTION_WHITE, screen,
						    &(SDL_Rect){ox, oy + (j * ui.option_size_px), w, ui.option_size_px});
						text_color = COLOR_BLACK;

						if (loop_item->desc)
							desc = loop_item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.medium, loop_item->name, text_color);
					SDL_BlitSurface(
					    text, NULL, screen,
					    &(SDL_Rect){ox + DP(OPTION_PADDING),
					                oy + (j * ui.option_size_px) + ui.option_offset_px});
					SDL_FreeSurface(text);
				}
			} else if (type == MENU_FIXED) {
				// NOTE: no need to calculate max width
				int mw = DP(ui.screen_width - ui.edge_padding * 2);
				int ox, oy;
				ox = oy = ui.edge_padding_px;
				oy += ui.pill_height_px;

				int selected_row = nav.selected - nav.start;
				for (int i = nav.start, j = 0; i < nav.end; i++, j++) {
					MenuItem* loop_item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j == selected_row) {
						// gray pill
						GFX_blitPill(
						    ASSET_OPTION, screen,
						    &(SDL_Rect){ox, oy + (j * ui.option_size_px), mw, ui.option_size_px});
					}

					// Calculate optimal widths using proportional truncation
					int label_text_w, value_text_w;
					int total_available = mw - DP(OPTION_PADDING * 3);
					const char* value_str =
					    (loop_item->value >= 0) ? loop_item->values[loop_item->value] : NULL;
					calculateProportionalWidths(loop_item->name, value_str, total_available,
					                            &label_text_w, &value_text_w);

					int label_w = label_text_w + DP(OPTION_PADDING * 2);

					// Render value text
					if (loop_item->value >= 0) {
						char truncated[256];
						GFX_truncateText(font.small, loop_item->values[loop_item->value], truncated,
						                 value_text_w, 0);
						text = TTF_RenderUTF8_Blended(font.small, truncated, COLOR_WHITE);
						SDL_BlitSurface(
						    text, NULL, screen,
						    &(SDL_Rect){ox + mw - text->w - DP(OPTION_PADDING),
						                oy + (j * ui.option_size_px) + ui.option_value_offset_px});
						SDL_FreeSurface(text);
					}

					if (j == selected_row) {
						// white pill
						GFX_blitPill(ASSET_OPTION_WHITE, screen,
						             &(SDL_Rect){ox, oy + (j * ui.option_size_px), label_w,
						                         ui.option_size_px});
						text_color = COLOR_BLACK;

						if (loop_item->desc)
							desc = loop_item->desc;
					}
					// Render label text
					char label_truncated[256];
					GFX_truncateText(font.medium, loop_item->name, label_truncated, label_text_w,
					                 0);
					text = TTF_RenderUTF8_Blended(font.medium, label_truncated, text_color);
					SDL_BlitSurface(
					    text, NULL, screen,
					    &(SDL_Rect){ox + DP(OPTION_PADDING),
					                oy + (j * ui.option_size_px) + ui.option_offset_px});
					SDL_FreeSurface(text);
				}
			} else if (type == MENU_VAR || type == MENU_INPUT) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest row
					int mrw = 0;
					for (int i = 0; i < nav.count; i++) {
						MenuItem* loop_item = &items[i];
						int w = 0;
						int lw = 0;
						int rw = 0;
						TTF_SizeUTF8(font.medium, loop_item->name, &lw, NULL);

						// every value list in an input table is the same
						// so only calculate rw for the first item...
						if (!mrw || type != MENU_INPUT) {
							for (int k = 0; loop_item->values[k]; k++) {
								TTF_SizeUTF8(font.small, loop_item->values[k], &rw, NULL);
								if (lw + rw > w)
									w = lw + rw;
								if (rw > mrw)
									mrw = rw;
							}
						} else {
							w = lw + mrw;
						}
						w += DP(OPTION_PADDING * 4);
						if (w > mw)
							mw = w;
					}
					// cache the result
					list->max_width = mw = MIN(mw, DP(ui.screen_width - ui.edge_padding * 2));
				}

				int ox = DP_CENTER_PX(ui.screen_width, mw);
				int oy = ui.edge_padding_px + ui.pill_height_px;
				int selected_row = nav.selected - nav.start;
				for (int i = nav.start, j = 0; i < nav.end; i++, j++) {
					MenuItem* loop_item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					// Calculate optimal widths using proportional truncation
					int label_text_w, value_text_w;
					int total_available = mw - DP(OPTION_PADDING * 3);
					const char* value_str =
					    (loop_item->value >= 0) ? loop_item->values[loop_item->value] : NULL;
					calculateProportionalWidths(loop_item->name, value_str, total_available,
					                            &label_text_w, &value_text_w);

					int label_w = label_text_w + DP(OPTION_PADDING * 2);

					if (j == selected_row) {
						// gray pill
						GFX_blitPill(
						    ASSET_OPTION, screen,
						    &(SDL_Rect){ox, oy + (j * ui.option_size_px), mw, ui.option_size_px});

						// white pill
						GFX_blitPill(ASSET_OPTION_WHITE, screen,
						             &(SDL_Rect){ox, oy + (j * ui.option_size_px), label_w,
						                         ui.option_size_px});
						text_color = COLOR_BLACK;

						if (loop_item->desc)
							desc = loop_item->desc;
					}
					// Render label text
					char label_truncated[256];
					GFX_truncateText(font.medium, loop_item->name, label_truncated, label_text_w,
					                 0);
					text = TTF_RenderUTF8_Blended(font.medium, label_truncated, text_color);
					SDL_BlitSurface(
					    text, NULL, screen,
					    &(SDL_Rect){ox + DP(OPTION_PADDING),
					                oy + (j * ui.option_size_px) + ui.option_offset_px});
					SDL_FreeSurface(text);

					if (nav.await_input && j == selected_row) {
						// buh
					} else if (loop_item->value >= 0) {
						// Render value text
						char truncated[256];
						GFX_truncateText(font.small, loop_item->values[loop_item->value], truncated,
						                 value_text_w, 0);
						text = TTF_RenderUTF8_Blended(font.small, truncated, COLOR_WHITE);
						SDL_BlitSurface(
						    text, NULL, screen,
						    &(SDL_Rect){ox + mw - text->w - DP(OPTION_PADDING),
						                oy + (j * ui.option_size_px) + ui.option_value_offset_px});
						SDL_FreeSurface(text);
					}
				}
			}

			if (nav.count > max_visible_options) {
#define SCROLL_WIDTH 24
#define SCROLL_HEIGHT 4
#define SCROLL_MARGIN 4 // Tight spacing anchored to option list
				int ox = (DP(ui.screen_width) - DP(SCROLL_WIDTH)) / 2;
				int options_top = ui.edge_padding_px + ui.pill_height_px;
				int options_bottom = options_top + (max_visible_options * ui.option_size_px);

				if (nav.start > 0)
					GFX_blitAsset(ASSET_SCROLL_UP, NULL, screen,
					              &(SDL_Rect){ox, options_top - DP(SCROLL_HEIGHT + SCROLL_MARGIN)});
				if (nav.end < nav.count)
					GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen,
					              &(SDL_Rect){ox, options_bottom + DP(SCROLL_MARGIN)});
			}

			if (!desc && list->desc)
				desc = list->desc;

			if (desc) {
				int w, h;
				GFX_sizeText(font.tiny, desc, DP(12), &w, &h);
				GFX_blitText(font.tiny, desc, DP(12), COLOR_WHITE, screen,
				             &(SDL_Rect){DP_CENTER_PX(ui.screen_width, w),
				                         DP(ui.screen_height) - DP(ui.edge_padding) - h, w, h});
			}

			GFX_flip(screen);
			nav.dirty = 0;
		} else
			GFX_sync();
		hdmimon();
	}

	// GFX_clearAll();
	// GFX_flip(screen);

	return 0;
}

///////////////////////////////
// Menu state wrappers (implementation in minarch_menu.c)
///////////////////////////////

static void Menu_initState(void) {
	MinArchMenu_initState(MinArchContext_get());
}

static void Menu_updateState(void) {
	MinArchMenu_updateState(MinArchContext_get());
}

static void Menu_saveState(void) {
	MinArchMenu_saveState(MinArchContext_get());
}

static void Menu_loadState(void) {
	MinArchMenu_loadState(MinArchContext_get());
	FramePacer_reset(&frame_pacer); // Reset accumulator after state load
}

static void Menu_scale(SDL_Surface* src, SDL_Surface* dst) {
	MinArchMenu_scale(MinArchContext_get(), src, dst);
}

///////////////////////////////////////
// In-Game Menu
///////////////////////////////////////

/**
 * Main menu loop - displays and handles in-game menu interaction.
 *
 * The in-game menu provides access to:
 * - Save/load state management (10 slots)
 * - Frontend options (scaling, sharpness, vsync, etc.)
 * - Core emulator options (from libretro core)
 * - Controller configuration
 * - Disc changing (multi-disc games)
 * - Reset and quit
 *
 * Flow:
 * 1. Captures current frame as background
 * 2. Saves SRAM/RTC (in case of crash)
 * 3. Reduces CPU speed and enables sleep (power savings)
 * 4. Displays menu over game screenshot
 * 5. Handles navigation and option changes
 * 6. Restores game state and resumes
 *
 * @note Blocks until user exits menu
 * @note Changes are saved to config file on menu exit
 */
static void Menu_loop(void) {
	MinArchMenu_loop(MinArchContext_get());
}

///////////////////////////////////////
// Performance Tracking
///////////////////////////////////////
// Runtime monitoring for debug HUD and audio/video synchronization.
//
// Functions:
// - getUsage(): System CPU% from /proc/self/stat
// - trackFPS(): FPS and CPU counters for debug HUD (once per second)
// - limitFF(): Fast-forward speed limiter
//
// These feed data to:
// - Debug HUD (top-left corner): FPS, CPU%

/**
 * Gets CPU usage percentage from /proc/self/stat.
 *
 * Reads process CPU ticks and converts to percentage. Used for debug overlay.
 *
 * @return CPU usage in percent (0-100+)
 *
 * @note Based on picoarch implementation
 * @note Returns 0 on error
 */
static unsigned getUsage(void) {
	long unsigned ticks = 0;
	long ticksps = 0;
	FILE* file = NULL;

	file = fopen("/proc/self/stat", "r");
	if (!file)
		goto finish;

	if (!fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &ticks))
		goto finish;

	ticksps = sysconf(_SC_CLK_TCK);

	if (ticksps)
		ticks = ticks * 100 / ticksps;

finish:
	if (file)
		(void)fclose(file); // File opened for reading

	return ticks;
}

/**
 * Tracks frames per second and CPU usage for debug overlay.
 *
 * Updates FPS and CPU usage counters once per second. Values are
 * displayed in debug overlay when show_debug is enabled.
 */
static void trackFPS(void) {
	cpu_ticks += 1;
	static int last_use_ticks = 0;
	static unsigned last_underrun_count = 0;
	uint32_t now = SDL_GetTicks();
	if (now - sec_start >= 1000) {
		double last_time = (double)(now - sec_start) / 1000;
		fps_double = fps_ticks / last_time;
		cpu_double = cpu_ticks / last_time;
		use_ticks = getUsage();
		if (use_ticks && last_use_ticks) {
			use_double = (use_ticks - last_use_ticks) / last_time;
		}
		last_use_ticks = use_ticks;
		sec_start = now;
		cpu_ticks = 0;
		fps_ticks = 0;

		// Check for audio underruns (sampled once per second)
		// Only warn if not in auto CPU mode (auto mode handles this via PANIC path)
		if (overclock != 3) {
			unsigned underruns = SND_getUnderrunCount();
			if (underruns > last_underrun_count) {
				LOG_warn("Audio: %u underrun(s) in last second\n", underruns - last_underrun_count);
				last_underrun_count = underruns;
			}
		}
	}
}

/**
 * Limits fast-forward speed to configured maximum.
 *
 * When fast-forwarding, this function ensures we don't exceed max_ff_speed
 * by inserting small delays between frames. Without this, FF would run as
 * fast as possible, consuming 100% CPU and draining battery.
 *
 * @note Only active when fast_forward is enabled and max_ff_speed > 0
 * @note Recalculates frame time when max_ff_speed changes
 */
static void limitFF(void) {
	static uint64_t ff_frame_time = 0;
	static uint64_t last_time = 0;
	static int last_max_speed = -1;
	if (last_max_speed != max_ff_speed) {
		last_max_speed = max_ff_speed;
		ff_frame_time = 1000000 / (core.fps * (max_ff_speed + 1));
	}

	uint64_t now = getMicroseconds();
	if (fast_forward && max_ff_speed) {
		if (last_time == 0)
			last_time = now;
		int elapsed = now - last_time;
		if (elapsed > 0 && elapsed < 0x80000) {
			if ((uint64_t)elapsed < ff_frame_time) {
				int delay = (ff_frame_time - elapsed) / 1000;
				if (delay > 0 && delay < 17) { // don't allow a delay any greater than a frame
					SDL_Delay(delay);
				}
			}
			last_time += ff_frame_time;
			return;
		}
	}
	last_time = now;
}

///////////////////////////////////////
// Main Entry Point
///////////////////////////////////////

/**
 * MinArch main entry point.
 *
 * Initializes all subsystems, loads core and game, runs main loop, and cleans up.
 *
 * Initialization sequence:
 * 1. Platform initialization (video, audio, input, power)
 * 2. Load libretro core (.so file)
 * 3. Load game ROM (with ZIP extraction if needed)
 * 4. Restore configuration and save states
 * 5. Initialize core with game
 * 6. Resume from auto-save slot
 * 7. Enter main loop
 *
 * Main loop runs core.run() each frame with vsync. Dynamic rate control
 * adjusts audio resampling to maintain sync without blocking.
 *
 * Shutdown sequence:
 * 1. Auto-save current state to slot 9
 * 2. Write SRAM/RTC data
 * 3. Unload game and core
 * 4. Cleanup all subsystems
 *
 * @param argc Argument count (expects 3: program, core path, rom path)
 * @param argv Arguments: [0]=program, [1]=core .so path, [2]=ROM path
 * @return EXIT_SUCCESS on normal exit
 *
 * @note Exits early if game fails to load
 */

/**
 * Display a fatal error message and wait for user acknowledgment.
 *
 * Shows "Game failed to load" title (large, white) and detail text (small, gray).
 * Blocks until user presses A or B.
 *
 * Reads from global fatal_error_detail.
 */
static void showFatalError(void) {
	static const char* title_text = "Game failed to start.";

	if (!screen || !font.large || !font.small) {
		LOG_error("showFatalError: UI not initialized");
		return;
	}

	// Copy and wrap detail text to fit screen
	char detail[512];
	strncpy(detail, fatal_error_detail, sizeof(detail) - 1);
	detail[sizeof(detail) - 1] = '\0';
	int text_width = ui.screen_width_px - ui.edge_padding_px * 2;
	GFX_wrapText(font.small, detail, text_width, 0);

	char* pairs[] = {"B", "BACK", NULL};
	int dirty = 1;

	while (1) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B))
			break;

		// Use NULL callbacks - no game state to save during error display
		PWR_update(&dirty, NULL, NULL, NULL);

		if (dirty) {
			GFX_clear(screen);

			// Calculate dimensions for vertical centering
			int title_h = TTF_FontHeight(font.large);
			int detail_line_h = TTF_FontLineSkip(font.small);

			// Count detail lines
			int detail_lines = 1;
			for (const char* p = detail; *p; p++) {
				if (*p == '\n')
					detail_lines++;
			}
			int detail_h = detail_lines * detail_line_h;

			int spacing = DP(4);
			int total_h = title_h + spacing + detail_h;
			int content_area_h = DP(ui.screen_height - ui.pill_height - ui.edge_padding * 2);
			int y = DP(ui.edge_padding) + (content_area_h - total_h) / 2;

			// Title (large, white, centered)
			SDL_Surface* title = TTF_RenderUTF8_Blended(font.large, title_text, COLOR_WHITE);
			if (title) {
				SDL_Rect title_rect = {(screen->w - title->w) / 2, y, title->w, title->h};
				SDL_BlitSurface(title, NULL, screen, &title_rect);
				SDL_FreeSurface(title);
			}

			y += title_h + spacing;

			// Detail text (small, gray, multi-line, left-aligned with standard padding)
			GFX_blitText(font.small, detail, detail_line_h, COLOR_GRAY, screen,
			             &(SDL_Rect){ui.edge_padding_px, y, text_width, detail_h});

			GFX_blitButtonGroup(pairs, 0, screen, 1);
			GFX_flip(screen);
			dirty = 0;
		} else {
			GFX_sync();
		}
	}
}

// Main loop implementation selected at compile-time based on sync mode
#ifdef SYNC_MODE_AUDIOCLOCK
#include "minarch_loop_audioclock.inc"
#else
#include "minarch_loop_vsync.inc"
#endif

int main(int argc, char* argv[]) {
	// Initialize logging early (reads LOG_FILE and LOG_SYNC from environment)
	log_open(NULL);

	LOG_info("MinArch");

	// Initialize context with pointers to globals
	// This enables functions to be migrated to context-based signatures
	MinArchContext* ctx = MinArchContext_get();
	ctx->core = &core;
	ctx->game = &game;
	ctx->screen = &screen;
	ctx->renderer = &renderer;
	ctx->video_state = &video_state;
	ctx->pixel_format = (int*)&pixel_format;
	ctx->screen_scaling = &screen_scaling;
	ctx->screen_sharpness = &screen_sharpness;
	ctx->screen_effect = &screen_effect;
	ctx->device_width = &DEVICE_WIDTH;
	ctx->device_height = &DEVICE_HEIGHT;
	ctx->device_pitch = &DEVICE_PITCH;
	ctx->fit = &fit;
	ctx->quit = &quit;
	ctx->show_menu = &show_menu;
	ctx->simple_mode = &simple_mode;
	ctx->show_debug = &show_debug;
	ctx->fast_forward = &fast_forward;
	ctx->max_ff_speed = &max_ff_speed;
	ctx->overclock = &overclock;
	ctx->state_slot = &state_slot;
	ctx->buttons = &buttons;
	ctx->ignore_menu = &ignore_menu;
	ctx->config = &config;
	ctx->auto_cpu_state = &auto_cpu_state;
	ctx->auto_cpu_config = &auto_cpu_config;
	ctx->disk_control = &disk_control_ext;
	ctx->menu = MinArchMenu_getState();
	MinArchContext_initGlobals(ctx);

	// Initialize callbacks for menu module to call back into minarch.c
	// This eliminates extern dependencies in minarch_menu.c
	MinArchCallbacks callbacks = {
	    .sram_write = SRAM_write,
	    .rtc_write = RTC_write,
	    .state_get_path = State_getPath,
	    .state_read = State_read,
	    .state_write = State_write,
	    .state_autosave = State_autosave,
	    .game_change_disc = Game_changeDisc,
	    .select_scaler = selectScaler,
	    .video_refresh = video_refresh_callback,
	    .set_overclock = setOverclock,
	    .menu_options = Menu_options,
	    .options_menu = &options_menu,
	    .get_hdmi = GetHDMI,
	    .hdmi_mon = hdmimon,
	    .frame_ready_for_flip = &frame_ready_for_flip,
	};
	MinArchContext_initCallbacks(ctx, &callbacks);

	// Initialize auto CPU scaling config with defaults
	MinArchCPU_initConfig(&auto_cpu_config);
	MinArchCPU_initState(&auto_cpu_state);

	setOverclock(overclock); // default to normal
	// force a stack overflow to ensure asan is linked and actually working
	// char tmp[2];
	// tmp[2] = 'a';

	char core_path[MAX_PATH];
	char rom_path[MAX_PATH];
	char tag_name[MAX_PATH];

	SAFE_STRCPY(core_path, argv[1]);
	SAFE_STRCPY(rom_path, argv[2]);
	getEmuName(rom_path, tag_name);

	LOG_info("rom_path: %s", rom_path);

	LOG_debug("GFX_init");
	screen = GFX_init(MODE_MENU);
	if (screen == NULL) {
		LOG_error("Failed to initialize video");
		log_close();
		return EXIT_FAILURE;
	}

	LOG_debug("PAD_init");
	PAD_init();
	DEVICE_WIDTH = screen->w;
	DEVICE_HEIGHT = screen->h;
	DEVICE_PITCH = screen->pitch;

	LOG_debug("VIB_init");
	VIB_init();

	LOG_debug("PWR_init");
	PWR_init();
	if (!HAS_POWER_BUTTON)
		PWR_disableSleep();

	LOG_debug("InitSettings");
	InitSettings(); // Initialize early so GetMute() works in error display

	LOG_debug("MSG_init");
	MSG_init();

	Core_open(core_path, tag_name);
	if (!core.handle) {
		LOG_debug("Core_open failed, core.handle=NULL");
		if (fatal_error_detail[0] != '\0') {
			LOG_info("Showing fatal error: %s", fatal_error_detail);
			showFatalError();
		}
		goto finish;
	}

	Game_open(rom_path); // nes tries to load gamegenie setting before this returns ffs
	if (!game.is_open) {
		LOG_debug("Game_open failed, game.is_open=0");
		if (fatal_error_detail[0] != '\0') {
			LOG_info("Showing fatal error: %s", fatal_error_detail);
			GFX_clearBlit(); // Ensure UI rendering mode
			showFatalError();
		}
		goto finish;
	}

	simple_mode = exists(SIMPLE_MODE_PATH);

	// restore options
	LOG_debug("Config_load");
	Config_load(); // before init?

	LOG_debug("Config_init");
	Config_init();

	LOG_debug("Config_readOptions (early)");
	Config_readOptions(); // cores with boot logo option (eg. gb) need to load options early
	setOverclock(overclock);

	Core_init();

	// TODO: find a better place to do this
	// mixing static and loaded data is messy
	// why not move to Core_init()?
	// ah, because it's defined before options_menu...
	options_menu.items[1].desc = (char*)core.version;

	if (!Core_load()) {
		LOG_info("Showing fatal error: %s", fatal_error_detail);
		GFX_clearBlit(); // Ensure UI rendering mode (core may have set game mode)
		showFatalError();
		goto finish;
	}

	LOG_debug("Input_init");
	Input_init(NULL);

	LOG_debug("Config_readOptions (late)");
	Config_readOptions(); // but others load and report options later (eg. nes)

	LOG_debug("Config_readControls");
	Config_readControls(); // restore controls (after the core has reported its defaults)
	Config_free();

	LOG_debug("SND_init (sample_rate=%.0f, fps=%.2f)", core.sample_rate, core.fps);
	SND_init(core.sample_rate, core.fps);

	LOG_debug("Menu_init");
	Menu_init();

	LOG_debug("State_resume");
	State_resume();

	LOG_debug("Menu_initState");
	Menu_initState(); // make ready for state shortcuts

	// Run the main loop (implementation selected at compile-time)
	run_main_loop();

	Menu_quit();

finish:

	QuitSettings();

	Game_close();
	Core_unload();

	Core_quit();
	Core_close();

	Config_quit();

	Special_quit();

	MSG_quit();
	PWR_quit();
	VIB_quit();
	SND_quit();
	PAD_quit();
	GFX_quit();

	MinArchVideoConvert_freeBuffer();

	// Close log file (flushes and syncs to disk)
	log_close();

	return EXIT_SUCCESS;
}
