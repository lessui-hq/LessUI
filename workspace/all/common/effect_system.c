/**
 * effect_system.c - Shared effect state management implementation
 *
 * This module consolidates the effect management logic that was previously
 * duplicated across all platform files. The opacity tables and pattern
 * path generation are now in a single location.
 *
 * All effects (LINE, GRID, GRILLE, SLOT) are procedurally generated at runtime.
 */

#include "effect_system.h"

#include <stdio.h>
#include <string.h>

#include "api.h"
#include "defines.h"

void EFFECT_init(EffectState* state) {
	memset(state, 0, sizeof(EffectState));
	state->type = EFFECT_NONE;
	state->next_type = EFFECT_NONE;
	state->scale = 1;
	state->next_scale = 1;
	state->color = 0;
	state->next_color = 0;
	state->live_type = EFFECT_NONE;
	state->live_scale = 0;
	state->live_color = 0;
}

void EFFECT_setType(EffectState* state, int type) {
	state->next_type = type;
}

void EFFECT_setScale(EffectState* state, int scale) {
	state->next_scale = scale;
}

void EFFECT_setColor(EffectState* state, int color) {
	state->next_color = color;
}

void EFFECT_applyPending(EffectState* state) {
	state->type = state->next_type;
	state->scale = state->next_scale;
	state->color = state->next_color;
}

int EFFECT_needsUpdate(const EffectState* state) {
	if (state->type == EFFECT_NONE) {
		return 0;
	}
	return (state->type != state->live_type || state->scale != state->live_scale ||
	        state->color != state->live_color);
}

void EFFECT_markLive(EffectState* state) {
	state->live_type = state->type;
	state->live_scale = state->scale;
	state->live_color = state->color;
}

int EFFECT_usesGeneration(int type) {
	// All effects use procedural generation (effect_generate.c)
	return (type == EFFECT_LINE || type == EFFECT_GRID || type == EFFECT_GRILLE ||
	        type == EFFECT_SLOT);
}

int EFFECT_getOpacity(int scale) {
	// Effects use opaque black patterns (alpha=255 for dark areas).
	// Control visibility via global opacity, scaling linearly:
	// - Lower scales (larger pixels/coarser patterns) = lighter/more subtle
	// - Higher scales (smaller pixels/finer patterns) = darker to remain visible
	//
	// Formula: opacity = 30 + (scale * 20)
	// Scale 2: 70 (27%), Scale 3: 90 (35%), Scale 4: 110 (43%), ... Scale 8: 190 (75%)
	//
	// This allows cranking up opacity for debugging and ensures effects remain
	// visible at high resolutions while not being too heavy at low resolutions.
	int opacity = 30 + (scale * 20);
	return (opacity > 255) ? 255 : opacity;
}
