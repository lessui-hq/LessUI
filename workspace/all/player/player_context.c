/**
 * player_context.c - Context management implementation
 *
 * The context is initialized by player.c which links pointers to
 * existing globals. This allows incremental migration to context-based
 * function signatures.
 */

#include "player_context.h"

// Global context instance
static PlayerContext g_ctx = {0};
static int g_ctx_initialized = 0;

// Global callbacks instance
static PlayerCallbacks g_callbacks = {0};

PlayerContext* PlayerContext_get(void) {
	return &g_ctx;
}

PlayerCallbacks* PlayerContext_getCallbacks(void) {
	return &g_callbacks;
}

void PlayerContext_initGlobals(PlayerContext* ctx) {
	if (!ctx) {
		return;
	}
	g_ctx_initialized = 1;
}

void PlayerContext_initCallbacks(PlayerContext* ctx, PlayerCallbacks* callbacks) {
	if (!ctx || !callbacks) {
		return;
	}

	// Copy callbacks to global instance
	g_callbacks = *callbacks;

	// Link context to callbacks
	ctx->callbacks = &g_callbacks;
}
