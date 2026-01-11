/**
 * player_context.h - Centralized state management for Player
 *
 * This header defines the PlayerContext structure which provides unified
 * access to all runtime state for the libretro frontend. By consolidating
 * global state access into a context object, we enable:
 *
 * 1. Testability - Functions can receive mock contexts
 * 2. Clarity - Dependencies are explicit in function signatures
 * 3. Modularity - Subsystems can be extracted to separate files
 *
 * Migration Strategy:
 * - Context pointers reference existing globals (no memory layout changes)
 * - Functions are migrated incrementally to take context parameters
 * - Wrapper macros maintain backward compatibility during transition
 *
 * Usage:
 *   // Access global context
 *   PlayerContext* ctx = PlayerContext_get();
 *
 *   // Functions take context pointer
 *   void Menu_loop(PlayerContext* ctx);
 */

#ifndef PLAYER_CONTEXT_H
#define PLAYER_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../common/cpu.h"
#include "player_internal.h"

// Forward declaration for SDL_Surface (avoids pulling in SDL headers)
// The actual SDL headers are included by player.c via api.h
struct SDL_Surface;

// Forward declaration for menu state (defined in player_menu.h)
struct PlayerMenuState;

// Forward declaration for MenuList (defined in player_menu_types.h)
struct MenuList;

///////////////////////////////
// Service callback types
///////////////////////////////
// These callbacks allow the menu module to invoke player.c functions
// without creating bidirectional extern dependencies.

/**
 * Memory persistence callbacks - save battery RAM and real-time clock
 */
typedef void (*PlayerSRAMWriteFunc)(void);
typedef void (*PlayerRTCWriteFunc)(void);

/**
 * Save state callbacks - manage emulator save states
 */
typedef void (*PlayerStateGetPathFunc)(char* filename);
typedef void (*PlayerStateReadFunc)(void);
typedef void (*PlayerStateWriteFunc)(void);
typedef void (*PlayerStateAutosaveFunc)(void);

/**
 * Game management callbacks
 */
typedef void (*PlayerGameChangeDiscFunc)(char* path);

/**
 * Video callbacks - scaler selection and frame refresh
 */
typedef void (*PlayerSelectScalerFunc)(int src_w, int src_h, int src_p);
typedef void (*PlayerVideoRefreshFunc)(const void* data, unsigned width, unsigned height,
                                       size_t pitch);

/**
 * CPU/power callbacks
 */
typedef void (*PlayerSetOverclockFunc)(int level);

/**
 * Menu callbacks - options menu display
 */
typedef int (*PlayerMenuOptionsFunc)(struct MenuList* list);

/**
 * Platform callbacks - HDMI monitoring
 */
typedef int (*PlayerGetHDMIFunc)(void);
typedef void (*PlayerHDMIMonFunc)(void);

///////////////////////////////
// Service callbacks container
///////////////////////////////

/**
 * PlayerCallbacks - Function pointers for player.c services
 *
 * These are set by player.c during initialization to allow the menu
 * module to call back into player.c without extern declarations.
 */
typedef struct PlayerCallbacks {
	// Memory persistence
	PlayerSRAMWriteFunc sram_write;
	PlayerRTCWriteFunc rtc_write;

	// Save state management
	PlayerStateGetPathFunc state_get_path;
	PlayerStateReadFunc state_read;
	PlayerStateWriteFunc state_write;
	PlayerStateAutosaveFunc state_autosave;

	// Game management
	PlayerGameChangeDiscFunc game_change_disc;

	// Video
	PlayerSelectScalerFunc select_scaler;
	PlayerVideoRefreshFunc video_refresh;

	// CPU/power
	PlayerSetOverclockFunc set_overclock;

	// Menu
	PlayerMenuOptionsFunc menu_options;
	struct MenuList* options_menu; // The root options menu list

	// Platform/HDMI
	PlayerGetHDMIFunc get_hdmi;
	PlayerHDMIMonFunc hdmi_mon;

	// Frame state
	int* frame_ready_for_flip;

} PlayerCallbacks;

///////////////////////////////
// Context structure
///////////////////////////////

/**
 * PlayerContext - Unified access to frontend state
 *
 * All pointers reference existing globals in player.c.
 * This allows incremental migration without restructuring.
 */
typedef struct PlayerContext {
	//----------------------------------
	// Core emulation
	//----------------------------------
	struct Core* core; // Libretro core interface
	struct Game* game; // Current game/ROM

	//----------------------------------
	// Video/rendering
	//----------------------------------
	struct SDL_Surface** screen; // Main screen surface
	void* renderer; // GFX_Renderer (scaling state)
	void* video_state; // Video geometry state

	// Pixel format (retro_pixel_format enum value)
	int* pixel_format;

	// Display settings
	int* screen_scaling;
	int* screen_sharpness;
	int* screen_effect;

	// Device dimensions
	int* device_width;
	int* device_height;
	int* device_pitch;
	int* fit; // Software vs hardware scaling

	//----------------------------------
	// Runtime flags
	//----------------------------------
	int* quit; // Exit main loop
	int* show_menu; // Display in-game menu
	int* simple_mode; // Simplified interface
	int* show_debug; // Show FPS/CPU overlay
	int* fast_forward; // Fast-forward active
	int* max_ff_speed; // FF speed limit
	int* overclock; // CPU speed mode
	int* state_slot; // Current save slot

	// Input state
	uint32_t* buttons; // Current button state
	int* ignore_menu; // Suppress menu button

	//----------------------------------
	// Configuration
	//----------------------------------
	struct Config* config; // Frontend and core options

	//----------------------------------
	// Auto CPU scaling
	//----------------------------------
	CPUState* auto_cpu_state;
	CPUConfig* auto_cpu_config;

	//----------------------------------
	// Multi-disc support
	//----------------------------------
	void* disk_control; // retro_disk_control_ext_callback

	//----------------------------------
	// Menu state
	//----------------------------------
	struct PlayerMenuState* menu; // Menu runtime state

	//----------------------------------
	// Service callbacks
	//----------------------------------
	PlayerCallbacks* callbacks; // Function pointers to player.c services

} PlayerContext;

///////////////////////////////
// Context lifecycle
///////////////////////////////

/**
 * Get the global context instance.
 * Context is initialized on first call.
 */
PlayerContext* PlayerContext_get(void);

/**
 * Initialize context with pointers to existing globals.
 * Called from player.c during startup.
 */
void PlayerContext_initGlobals(PlayerContext* ctx);

/**
 * Get the global callbacks instance.
 * Returns pointer to callbacks container that player.c populates.
 */
PlayerCallbacks* PlayerContext_getCallbacks(void);

/**
 * Initialize callbacks with function pointers from player.c.
 * Called from player.c during startup after context init.
 */
void PlayerContext_initCallbacks(PlayerContext* ctx, PlayerCallbacks* callbacks);

///////////////////////////////
// Convenience accessors
///////////////////////////////

// These provide cleaner access patterns for common operations.
// Example: ctx_getCore(ctx)->run() instead of ctx->core->run()

static inline struct Core* ctx_getCore(PlayerContext* ctx) {
	return ctx ? ctx->core : NULL;
}

static inline struct Game* ctx_getGame(PlayerContext* ctx) {
	return ctx ? ctx->game : NULL;
}

static inline int ctx_isQuitting(PlayerContext* ctx) {
	return ctx && ctx->quit ? *ctx->quit : 0;
}

static inline int ctx_isMenuShown(PlayerContext* ctx) {
	return ctx && ctx->show_menu ? *ctx->show_menu : 0;
}

static inline void ctx_setQuit(PlayerContext* ctx, int value) {
	if (ctx && ctx->quit) {
		*ctx->quit = value;
	}
}

static inline void ctx_setShowMenu(PlayerContext* ctx, int value) {
	if (ctx && ctx->show_menu) {
		*ctx->show_menu = value;
	}
}

#endif /* PLAYER_CONTEXT_H */
