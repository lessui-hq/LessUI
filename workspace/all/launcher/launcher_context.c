/**
 * launcher_context.c - Centralized state management for Launcher
 *
 * Provides the global context instance and lifecycle management.
 */

#include "launcher_context.h"
#include <string.h>

///////////////////////////////
// Global instances
///////////////////////////////

static LauncherContext g_context;
static LauncherCallbacks g_callbacks;
static int g_initialized = 0;

///////////////////////////////
// Context lifecycle
///////////////////////////////

LauncherContext* LauncherContext_get(void) {
	if (!g_initialized) {
		memset(&g_context, 0, sizeof(g_context));
		memset(&g_callbacks, 0, sizeof(g_callbacks));
		g_context.callbacks = &g_callbacks;
		g_initialized = 1;
	}
	return &g_context;
}

void LauncherContext_initGlobals(LauncherContext* ctx) {
	// This is called by launcher.c to wire up pointers to existing globals.
	// The implementation sets ctx->quit = &quit, ctx->top = &top, etc.
	// See launcher.c LauncherContext_setup() for actual wiring.
	(void)ctx;
}

LauncherCallbacks* LauncherContext_getCallbacks(void) {
	LauncherContext_get(); // Ensure initialized
	return &g_callbacks;
}

void LauncherContext_initCallbacks(LauncherContext* ctx, LauncherCallbacks* callbacks) {
	if (ctx && callbacks) {
		ctx->callbacks = callbacks;
	}
}
