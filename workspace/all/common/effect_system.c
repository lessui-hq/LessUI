/**
 * effect_system.c - Shared effect state management implementation
 *
 * This module consolidates the effect management logic that was previously
 * duplicated across all platform files. The opacity tables and pattern
 * path generation are now in a single location.
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

int EFFECT_getOpacity(int scale) {
	// All effects use opaque black patterns (alpha=255 in PNG).
	// Control visibility via global opacity, scaling linearly:
	// - Lower scales (coarser patterns) = lower opacity to stay subtle
	// - Higher scales (finer patterns) = higher opacity to remain visible
	//
	// Formula: opacity = 40 + (scale * 20)
	// Scale 2: 80, Scale 3: 100, ... Scale 8: 200
	int opacity = 40 + (scale * 20);
	return (opacity > 255) ? 255 : opacity;
}

int EFFECT_getPatternScale(int scale) {
	// All effects use scale-specific patterns (line-N.png, grid-N.png, crt-N.png)
	// Available scales: 2, 3, 4, 5, 6, 7, 8
	if (scale < 2)
		return 2;
	if (scale > 8)
		return 8;
	return scale;
}

const char* EFFECT_getPatternPath(char* buf, int bufsize, int type, int scale) {
	const char* pattern_name = NULL;
	switch (type) {
	case EFFECT_LINE:
		pattern_name = "line";
		break;
	case EFFECT_GRID:
		pattern_name = "grid";
		break;
	case EFFECT_GRILLE:
		pattern_name = "grille";
		break;
	case EFFECT_SLOT:
		pattern_name = "slot";
		break;
	default:
		return NULL;
	}

	// All patterns are scale-specific: line-2.png, grid-3.png, etc.
	snprintf(buf, bufsize, RES_PATH "/%s-%d.png", pattern_name, EFFECT_getPatternScale(scale));
	return buf;
}
