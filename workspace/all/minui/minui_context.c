/**
 * minui_context.c - Centralized state management for MinUI
 *
 * Provides the global context instance and lifecycle management.
 */

#include "minui_context.h"
#include <string.h>

///////////////////////////////
// Global instances
///////////////////////////////

static MinUIContext g_context;
static MinUICallbacks g_callbacks;
static int g_initialized = 0;

///////////////////////////////
// Context lifecycle
///////////////////////////////

MinUIContext* MinUIContext_get(void) {
	if (!g_initialized) {
		memset(&g_context, 0, sizeof(g_context));
		memset(&g_callbacks, 0, sizeof(g_callbacks));
		g_context.callbacks = &g_callbacks;
		g_initialized = 1;
	}
	return &g_context;
}

void MinUIContext_initGlobals(MinUIContext* ctx) {
	// This is called by minui.c to wire up pointers to existing globals.
	// The implementation sets ctx->quit = &quit, ctx->top = &top, etc.
	// See minui.c MinUIContext_setup() for actual wiring.
	(void)ctx;
}

MinUICallbacks* MinUIContext_getCallbacks(void) {
	MinUIContext_get(); // Ensure initialized
	return &g_callbacks;
}

void MinUIContext_initCallbacks(MinUIContext* ctx, MinUICallbacks* callbacks) {
	if (ctx && callbacks) {
		ctx->callbacks = callbacks;
	}
}
