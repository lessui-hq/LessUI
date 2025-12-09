/**
 * minui_context.h - Centralized state management for MinUI
 *
 * This header defines the MinUIContext structure which provides unified
 * access to all runtime state for the launcher. By consolidating
 * global state access into a context object, we enable:
 *
 * 1. Testability - Functions can receive mock contexts
 * 2. Clarity - Dependencies are explicit in function signatures
 * 3. Modularity - Subsystems can be extracted to separate files
 *
 * Migration Strategy (same as MinArch):
 * - Context pointers reference existing globals (no memory layout changes)
 * - Functions are migrated incrementally to take context parameters
 * - Wrapper functions maintain backward compatibility during transition
 *
 * Usage:
 *   // Access global context
 *   MinUIContext* ctx = MinUIContext_get();
 *
 *   // Functions take context pointer
 *   void Entry_open_ctx(MinUIContext* ctx, Entry* entry);
 */

#ifndef MINUI_CONTEXT_H
#define MINUI_CONTEXT_H

#include "collections.h"
#include "minui_entry.h"
#include <stdbool.h>
#include <stddef.h>

// Note: We include these headers to get Directory and Recent types
// This makes the context fully typed instead of using void*
#include "minui_directory.h"
#include "recent_file.h"

///////////////////////////////
// Service callback types
///////////////////////////////
// These callbacks allow navigation functions to invoke minui.c services
// without creating bidirectional extern dependencies.

/**
 * Recent games callbacks
 */
typedef void (*MinUIAddRecentFunc)(char* path, char* alias);
typedef void (*MinUISaveRecentsFunc)(void);

/**
 * Command execution callbacks
 */
typedef void (*MinUIQueueNextFunc)(char* cmd);

/**
 * State persistence callbacks
 */
typedef void (*MinUISaveLastFunc)(char* path);
typedef void (*MinUILoadLastFunc)(void);

/**
 * Directory navigation callbacks
 */
typedef void (*MinUIOpenDirectoryFunc)(char* path, int auto_launch);
typedef Directory* (*MinUIDirectoryNewFunc)(char* path, int selected);

/**
 * File operation callbacks (allows mocking in tests)
 * Note: Signatures match actual functions in utils.h
 */
typedef int (*MinUIExistsFunc)(char* path);
typedef void (*MinUIPutFileFunc)(const char* path, const char* content);
typedef void (*MinUIGetFileFunc)(const char* path, char* buffer, size_t size);
typedef void (*MinUIPutIntFunc)(const char* path, int value);

///////////////////////////////
// Service callbacks container
///////////////////////////////

/**
 * MinUICallbacks - Function pointers for minui.c services
 *
 * These are set by minui.c during initialization to allow extracted
 * modules to call back into minui.c without extern declarations.
 */
typedef struct MinUICallbacks {
	// Recent games
	MinUIAddRecentFunc add_recent;
	MinUISaveRecentsFunc save_recents;

	// Command execution
	MinUIQueueNextFunc queue_next;

	// State persistence
	MinUISaveLastFunc save_last;
	MinUILoadLastFunc load_last;

	// Directory navigation
	MinUIOpenDirectoryFunc open_directory;
	MinUIDirectoryNewFunc directory_new;

	// File operations (for testability)
	MinUIExistsFunc exists;
	MinUIPutFileFunc put_file;
	MinUIGetFileFunc get_file;
	MinUIPutIntFunc put_int;

} MinUICallbacks;

///////////////////////////////
// UI state structure
///////////////////////////////

/**
 * MinUIState - UI layout and display state
 */
typedef struct MinUIUIState {
	int row_count; // Number of visible rows
} MinUIUIState;

///////////////////////////////
// Restore state structure
///////////////////////////////

/**
 * MinUIRestoreState - Navigation state restoration
 */
typedef struct MinUIRestoreState {
	int depth;
	int relative;
	int selected;
	int start;
	int end;
} MinUIRestoreState;

///////////////////////////////
// Context structure
///////////////////////////////

/**
 * MinUIContext - Unified access to launcher state
 *
 * All pointers reference existing globals in minui.c.
 * This allows incremental migration without restructuring.
 */
typedef struct MinUIContext {
	//----------------------------------
	// Navigation state
	//----------------------------------
	Directory** top; // Current directory being viewed
	Array** stack; // Stack of open Directory* (use DirectoryArray_* functions)
	Array** recents; // Array of Recent* (use RecentArray_* functions)

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
	MinUIRestoreState* restore;

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
	MinUICallbacks* callbacks;

} MinUIContext;

///////////////////////////////
// Context lifecycle
///////////////////////////////

/**
 * Get the global context instance.
 * Context is initialized on first call.
 */
MinUIContext* MinUIContext_get(void);

/**
 * Initialize context with pointers to existing globals.
 * Called from minui.c during startup.
 */
void MinUIContext_initGlobals(MinUIContext* ctx);

/**
 * Get the global callbacks instance.
 * Returns pointer to callbacks container that minui.c populates.
 */
MinUICallbacks* MinUIContext_getCallbacks(void);

/**
 * Initialize callbacks with function pointers from minui.c.
 * Called from minui.c during startup after context init.
 */
void MinUIContext_initCallbacks(MinUIContext* ctx, MinUICallbacks* callbacks);

///////////////////////////////
// Convenience accessors
///////////////////////////////

// Returns the current directory (properly typed)
static inline Directory* ctx_getTop(MinUIContext* ctx) {
	return (ctx && ctx->top) ? *ctx->top : NULL;
}

static inline Array* ctx_getStack(MinUIContext* ctx) {
	return (ctx && ctx->stack) ? *ctx->stack : NULL;
}

static inline Array* ctx_getRecents(MinUIContext* ctx) {
	return (ctx && ctx->recents) ? *ctx->recents : NULL;
}

static inline int ctx_isQuitting(MinUIContext* ctx) {
	return (ctx && ctx->quit) ? *ctx->quit : 0;
}

static inline void ctx_setQuit(MinUIContext* ctx, int value) {
	if (ctx && ctx->quit) {
		*ctx->quit = value;
	}
}

static inline int ctx_canResume(MinUIContext* ctx) {
	return (ctx && ctx->can_resume) ? *ctx->can_resume : 0;
}

static inline void ctx_setCanResume(MinUIContext* ctx, int value) {
	if (ctx && ctx->can_resume) {
		*ctx->can_resume = value;
	}
}

static inline int ctx_shouldResume(MinUIContext* ctx) {
	return (ctx && ctx->should_resume) ? *ctx->should_resume : 0;
}

static inline void ctx_setShouldResume(MinUIContext* ctx, int value) {
	if (ctx && ctx->should_resume) {
		*ctx->should_resume = value;
	}
}

#endif /* MINUI_CONTEXT_H */
