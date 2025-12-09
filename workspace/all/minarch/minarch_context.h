/**
 * minarch_context.h - Centralized state management for MinArch
 *
 * This header defines the MinArchContext structure which provides unified
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
 *   MinArchContext* ctx = MinArchContext_get();
 *
 *   // Functions take context pointer
 *   void Menu_loop(MinArchContext* ctx);
 */

#ifndef MINARCH_CONTEXT_H
#define MINARCH_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minarch_cpu.h"
#include "minarch_internal.h"

// Forward declaration for SDL_Surface (avoids pulling in SDL headers)
// The actual SDL headers are included by minarch.c via api.h
struct SDL_Surface;

// Forward declaration for menu state (defined in minarch_menu.h)
struct MinArchMenuState;

// Forward declaration for MenuList (defined in minarch_menu_types.h)
struct MenuList;

///////////////////////////////
// Service callback types
///////////////////////////////
// These callbacks allow the menu module to invoke minarch.c functions
// without creating bidirectional extern dependencies.

/**
 * Memory persistence callbacks - save battery RAM and real-time clock
 */
typedef void (*MinArchSRAMWriteFunc)(void);
typedef void (*MinArchRTCWriteFunc)(void);

/**
 * Save state callbacks - manage emulator save states
 */
typedef void (*MinArchStateGetPathFunc)(char* filename);
typedef void (*MinArchStateReadFunc)(void);
typedef void (*MinArchStateWriteFunc)(void);
typedef void (*MinArchStateAutosaveFunc)(void);

/**
 * Game management callbacks
 */
typedef void (*MinArchGameChangeDiscFunc)(char* path);

/**
 * Video callbacks - scaler selection and frame refresh
 */
typedef void (*MinArchSelectScalerFunc)(int src_w, int src_h, int src_p);
typedef void (*MinArchVideoRefreshFunc)(const void* data, unsigned width, unsigned height,
                                        size_t pitch);

/**
 * CPU/power callbacks
 */
typedef void (*MinArchSetOverclockFunc)(int level);

/**
 * Menu callbacks - options menu display
 */
typedef int (*MinArchMenuOptionsFunc)(struct MenuList* list);

/**
 * Platform callbacks - HDMI monitoring
 */
typedef int (*MinArchGetHDMIFunc)(void);
typedef void (*MinArchHDMIMonFunc)(void);

///////////////////////////////
// Service callbacks container
///////////////////////////////

/**
 * MinArchCallbacks - Function pointers for minarch.c services
 *
 * These are set by minarch.c during initialization to allow the menu
 * module to call back into minarch.c without extern declarations.
 */
typedef struct MinArchCallbacks {
	// Memory persistence
	MinArchSRAMWriteFunc sram_write;
	MinArchRTCWriteFunc rtc_write;

	// Save state management
	MinArchStateGetPathFunc state_get_path;
	MinArchStateReadFunc state_read;
	MinArchStateWriteFunc state_write;
	MinArchStateAutosaveFunc state_autosave;

	// Game management
	MinArchGameChangeDiscFunc game_change_disc;

	// Video
	MinArchSelectScalerFunc select_scaler;
	MinArchVideoRefreshFunc video_refresh;

	// CPU/power
	MinArchSetOverclockFunc set_overclock;

	// Menu
	MinArchMenuOptionsFunc menu_options;
	struct MenuList* options_menu; // The root options menu list

	// Platform/HDMI
	MinArchGetHDMIFunc get_hdmi;
	MinArchHDMIMonFunc hdmi_mon;

	// Frame state
	int* frame_ready_for_flip;

} MinArchCallbacks;

///////////////////////////////
// Context structure
///////////////////////////////

/**
 * MinArchContext - Unified access to frontend state
 *
 * All pointers reference existing globals in minarch.c.
 * This allows incremental migration without restructuring.
 */
typedef struct MinArchContext {
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
	MinArchCPUState* auto_cpu_state;
	MinArchCPUConfig* auto_cpu_config;

	//----------------------------------
	// Multi-disc support
	//----------------------------------
	void* disk_control; // retro_disk_control_ext_callback

	//----------------------------------
	// Menu state
	//----------------------------------
	struct MinArchMenuState* menu; // Menu runtime state

	//----------------------------------
	// Service callbacks
	//----------------------------------
	MinArchCallbacks* callbacks; // Function pointers to minarch.c services

} MinArchContext;

///////////////////////////////
// Context lifecycle
///////////////////////////////

/**
 * Get the global context instance.
 * Context is initialized on first call.
 */
MinArchContext* MinArchContext_get(void);

/**
 * Initialize context with pointers to existing globals.
 * Called from minarch.c during startup.
 */
void MinArchContext_initGlobals(MinArchContext* ctx);

/**
 * Get the global callbacks instance.
 * Returns pointer to callbacks container that minarch.c populates.
 */
MinArchCallbacks* MinArchContext_getCallbacks(void);

/**
 * Initialize callbacks with function pointers from minarch.c.
 * Called from minarch.c during startup after context init.
 */
void MinArchContext_initCallbacks(MinArchContext* ctx, MinArchCallbacks* callbacks);

///////////////////////////////
// Convenience accessors
///////////////////////////////

// These provide cleaner access patterns for common operations.
// Example: ctx_getCore(ctx)->run() instead of ctx->core->run()

static inline struct Core* ctx_getCore(MinArchContext* ctx) {
	return ctx ? ctx->core : NULL;
}

static inline struct Game* ctx_getGame(MinArchContext* ctx) {
	return ctx ? ctx->game : NULL;
}

static inline int ctx_isQuitting(MinArchContext* ctx) {
	return ctx && ctx->quit ? *ctx->quit : 0;
}

static inline int ctx_isMenuShown(MinArchContext* ctx) {
	return ctx && ctx->show_menu ? *ctx->show_menu : 0;
}

static inline void ctx_setQuit(MinArchContext* ctx, int value) {
	if (ctx && ctx->quit) {
		*ctx->quit = value;
	}
}

static inline void ctx_setShowMenu(MinArchContext* ctx, int value) {
	if (ctx && ctx->show_menu) {
		*ctx->show_menu = value;
	}
}

#endif /* MINARCH_CONTEXT_H */
