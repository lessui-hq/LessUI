/**
 * minarch_context.c - Context management implementation
 *
 * The context is initialized by minarch.c which links pointers to
 * existing globals. This allows incremental migration to context-based
 * function signatures.
 */

#include "minarch_context.h"

// Global context instance
static MinArchContext g_ctx = {0};
static int g_ctx_initialized = 0;

// Global callbacks instance
static MinArchCallbacks g_callbacks = {0};

MinArchContext* MinArchContext_get(void) {
	return &g_ctx;
}

MinArchCallbacks* MinArchContext_getCallbacks(void) {
	return &g_callbacks;
}

void MinArchContext_initGlobals(MinArchContext* ctx) {
	if (!ctx) {
		return;
	}
	g_ctx_initialized = 1;
}

void MinArchContext_initCallbacks(MinArchContext* ctx, MinArchCallbacks* callbacks) {
	if (!ctx || !callbacks) {
		return;
	}

	// Copy callbacks to global instance
	g_callbacks = *callbacks;

	// Link context to callbacks
	ctx->callbacks = &g_callbacks;
}
