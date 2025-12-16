/**
 * launcher_context.h - Centralized state management for Launcher
 *
 * This header defines the LauncherContext structure which provides unified
 * access to all runtime state for the launcher. By consolidating
 * global state access into a context object, we enable:
 *
 * 1. Testability - Functions can receive mock contexts
 * 2. Clarity - Dependencies are explicit in function signatures
 * 3. Modularity - Subsystems can be extracted to separate files
 *
 * Migration Strategy (same as Player):
 * - Context pointers reference existing globals (no memory layout changes)
 * - Functions are migrated incrementally to take context parameters
 * - Wrapper functions maintain backward compatibility during transition
 *
 * Usage:
 *   // Access global context
 *   LauncherContext* ctx = LauncherContext_get();
 *
 *   // Functions take context pointer
 *   void Entry_open_ctx(LauncherContext* ctx, Entry* entry);
 */

#ifndef LAUNCHER_CONTEXT_H
#define LAUNCHER_CONTEXT_H

#include "launcher_entry.h"
#include <stdbool.h>
#include <stddef.h>

// Note: We include these headers to get Directory and Recent types
// This makes the context fully typed instead of using void*
#include "launcher_directory.h"
#include "recent_file.h"

///////////////////////////////
// Service callback types
///////////////////////////////
// These callbacks allow navigation functions to invoke launcher.c services
// without creating bidirectional extern dependencies.

/**
 * Recent games callbacks
 */
typedef void (*LauncherAddRecentFunc)(char* path, char* alias);
typedef void (*LauncherSaveRecentsFunc)(void);

/**
 * Command execution callbacks
 */
typedef void (*LauncherQueueNextFunc)(char* cmd);

/**
 * State persistence callbacks
 */
typedef void (*LauncherSaveLastFunc)(char* path);
typedef void (*LauncherLoadLastFunc)(void);

/**
 * Directory navigation callbacks
 */
typedef void (*LauncherOpenDirectoryFunc)(char* path, int auto_launch);
typedef Directory* (*LauncherDirectoryNewFunc)(char* path, int selected);

/**
 * File operation callbacks (allows mocking in tests)
 * Note: Signatures match actual functions in utils.h
 */
typedef int (*LauncherExistsFunc)(char* path);
typedef void (*LauncherPutFileFunc)(const char* path, const char* content);
typedef void (*LauncherGetFileFunc)(const char* path, char* buffer, size_t size);
typedef void (*LauncherPutIntFunc)(const char* path, int value);

///////////////////////////////
// Service callbacks container
///////////////////////////////

/**
 * LauncherCallbacks - Function pointers for launcher.c services
 *
 * These are set by launcher.c during initialization to allow extracted
 * modules to call back into launcher.c without extern declarations.
 */
typedef struct LauncherCallbacks {
	// Recent games
	LauncherAddRecentFunc add_recent;
	LauncherSaveRecentsFunc save_recents;

	// Command execution
	LauncherQueueNextFunc queue_next;

	// State persistence
	LauncherSaveLastFunc save_last;
	LauncherLoadLastFunc load_last;

	// Directory navigation
	LauncherOpenDirectoryFunc open_directory;
	LauncherDirectoryNewFunc directory_new;

	// File operations (for testability)
	LauncherExistsFunc exists;
	LauncherPutFileFunc put_file;
	LauncherGetFileFunc get_file;
	LauncherPutIntFunc put_int;

} LauncherCallbacks;

///////////////////////////////
// UI state structure
///////////////////////////////

/**
 * LauncherState - UI layout and display state
 */
typedef struct LauncherUIState {
	int row_count; // Number of visible rows
} LauncherUIState;

///////////////////////////////
// Restore state structure
///////////////////////////////

/**
 * LauncherRestoreState - Navigation state restoration
 */
typedef struct LauncherRestoreState {
	int depth;
	int relative;
	int selected;
	int start;
	int end;
} LauncherRestoreState;

///////////////////////////////
// Context structure
///////////////////////////////

/**
 * LauncherContext - Unified access to launcher state
 *
 * All pointers reference existing globals in launcher.c.
 * This allows incremental migration without restructuring.
 */
typedef struct LauncherContext {
	//----------------------------------
	// Navigation state
	//----------------------------------
	Directory** top; // Current directory being viewed
	Directory*** stack; // Pointer to Directory** stack (stb_ds dynamic array)
	Recent*** recents; // Pointer to Recent** recents (stb_ds dynamic array)

	//----------------------------------
	// Runtime flags
	//----------------------------------
	int* quit; // Exit main loop flag
	int* can_resume; // ROM has save state
	int* should_resume; // User requested resume
	int* simple_mode; // Simplified interface mode

	//----------------------------------
	// Resume state
	//----------------------------------
	char* slot_path; // Path to current save state slot file
	int slot_path_size; // Size of slot_path buffer

	//----------------------------------
	// State restoration
	//----------------------------------
	LauncherRestoreState* restore;

	//----------------------------------
	// UI state (pointer to global ui struct from api.h)
	//----------------------------------
	void* ui; // Pointer to global ui struct

	//----------------------------------
	// Alias for recent entries
	//----------------------------------
	char** recent_alias;

	//----------------------------------
	// Service callbacks
	//----------------------------------
	LauncherCallbacks* callbacks;

} LauncherContext;

///////////////////////////////
// Context lifecycle
///////////////////////////////

/**
 * Get the global context instance.
 * Context is initialized on first call.
 */
LauncherContext* LauncherContext_get(void);

/**
 * Initialize context with pointers to existing globals.
 * Called from launcher.c during startup.
 */
void LauncherContext_initGlobals(LauncherContext* ctx);

/**
 * Get the global callbacks instance.
 * Returns pointer to callbacks container that launcher.c populates.
 */
LauncherCallbacks* LauncherContext_getCallbacks(void);

/**
 * Initialize callbacks with function pointers from launcher.c.
 * Called from launcher.c during startup after context init.
 */
void LauncherContext_initCallbacks(LauncherContext* ctx, LauncherCallbacks* callbacks);

///////////////////////////////
// Convenience accessors
///////////////////////////////

// Returns the current directory (properly typed)
static inline Directory* ctx_getTop(LauncherContext* ctx) {
	return (ctx && ctx->top) ? *ctx->top : NULL;
}

static inline Directory** ctx_getStack(LauncherContext* ctx) {
	return (ctx && ctx->stack) ? *ctx->stack : NULL;
}

static inline Recent** ctx_getRecents(LauncherContext* ctx) {
	return (ctx && ctx->recents) ? *ctx->recents : NULL;
}

static inline int ctx_isQuitting(LauncherContext* ctx) {
	return (ctx && ctx->quit) ? *ctx->quit : 0;
}

static inline void ctx_setQuit(LauncherContext* ctx, int value) {
	if (ctx && ctx->quit) {
		*ctx->quit = value;
	}
}

static inline int ctx_canResume(LauncherContext* ctx) {
	return (ctx && ctx->can_resume) ? *ctx->can_resume : 0;
}

static inline void ctx_setCanResume(LauncherContext* ctx, int value) {
	if (ctx && ctx->can_resume) {
		*ctx->can_resume = value;
	}
}

static inline int ctx_shouldResume(LauncherContext* ctx) {
	return (ctx && ctx->should_resume) ? *ctx->should_resume : 0;
}

static inline void ctx_setShouldResume(LauncherContext* ctx, int value) {
	if (ctx && ctx->should_resume) {
		*ctx->should_resume = value;
	}
}

#endif /* LAUNCHER_CONTEXT_H */
