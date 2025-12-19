/**
 * effect_system.h - Shared effect state management for retro visual effects
 *
 * This module provides a unified API for managing visual effects (scanlines,
 * pixel grids, CRT simulation) across all platforms. It extracts common logic
 * that was previously duplicated in each platform's video code.
 *
 * Features:
 * - Effect state tracking with deferred updates
 * - Opacity calculation based on effect type and scale factor
 *
 * Usage:
 *   EffectState effect;
 *   EFFECT_init(&effect);
 *   EFFECT_setType(&effect, EFFECT_LINE);
 *   EFFECT_setScale(&effect, 3);
 *   if (EFFECT_needsUpdate(&effect)) {
 *       // Generate effect pattern using effect_generate.h functions
 *       EFFECT_markLive(&effect);
 *   }
 */

#ifndef __EFFECT_SYSTEM_H__
#define __EFFECT_SYSTEM_H__

#include <stdint.h>

/**
 * Effect state structure.
 *
 * Tracks both current and pending effect settings to support deferred updates.
 * The "next_*" fields hold requested changes that are applied on the next frame.
 * The "live_*" fields track what is currently rendered to detect when updates
 * are needed.
 *
 * This pattern allows settings to change mid-frame without causing visual
 * artifacts, as the actual texture is only updated between frames.
 */
typedef struct EffectState {
	int type; // Current effect type (EFFECT_NONE, EFFECT_LINE, etc.)
	int next_type; // Pending effect type for next frame
	int scale; // Current scale factor (typically 2-8)
	int next_scale; // Pending scale factor for next frame
	int color; // Current DMG color for grid effect (RGB565)
	int next_color; // Pending DMG color for next frame
	int live_type; // Effect type currently rendered (for change detection)
	int live_scale; // Scale currently rendered (for change detection)
	int live_color; // Color currently rendered (for change detection)
} EffectState;

/**
 * Initializes effect state to default values.
 *
 * Sets all fields to EFFECT_NONE with scale 1 and no color tint.
 * Should be called once during video initialization.
 *
 * @param state Effect state to initialize
 */
void EFFECT_init(EffectState* state);

/**
 * Sets the pending effect type for next frame.
 *
 * The change is deferred until EFFECT_applyPending() is called.
 *
 * @param state Effect state to modify
 * @param type  Effect type (EFFECT_NONE, EFFECT_LINE, EFFECT_GRID, etc.)
 */
void EFFECT_setType(EffectState* state, int type);

/**
 * Sets the pending scale factor for next frame.
 *
 * Scale affects effect opacity and pattern selection.
 *
 * @param state Effect state to modify
 * @param scale Integer scale factor (typically 1-6)
 */
void EFFECT_setScale(EffectState* state, int scale);

/**
 * Sets the pending DMG color for grid effects.
 *
 * Used to colorize monochrome grid effects to match Game Boy palettes.
 *
 * @param state Effect state to modify
 * @param color RGB565 color value
 */
void EFFECT_setColor(EffectState* state, int color);

/**
 * Applies pending changes to current state.
 *
 * Copies next_* values to current values. Should be called once per frame
 * before rendering to sync pending changes.
 *
 * @param state Effect state to update
 */
void EFFECT_applyPending(EffectState* state);

/**
 * Checks if effect texture needs to be regenerated.
 *
 * Compares current settings against live (rendered) settings to determine
 * if the effect overlay texture needs to be recreated.
 *
 * @param state Effect state to check
 * @return 1 if update needed, 0 if current texture is still valid
 */
int EFFECT_needsUpdate(const EffectState* state);

/**
 * Marks current effect as live (rendered).
 *
 * Call after successfully updating the effect texture to prevent
 * unnecessary regeneration on subsequent frames.
 *
 * @param state Effect state to update
 */
void EFFECT_markLive(EffectState* state);

/**
 * Checks if an effect type uses procedural generation.
 *
 * All effects (LINE, GRID, CRT, SLOT) are procedurally generated at runtime
 * via effect_generate.c functions.
 *
 * @param type Effect type to check
 * @return 1 if type uses generation, 0 otherwise
 */
int EFFECT_usesGeneration(int type);

/**
 * Gets the appropriate opacity for a legacy (PNG-based) effect.
 *
 * Legacy effect patterns use opaque black (alpha=255 in PNG). Visibility is
 * controlled via global opacity using a simple linear formula:
 *
 *   opacity = 30 + (scale * 20)
 *
 * This gives:
 *   Scale 2: 70    (coarse patterns, subtle)
 *   Scale 3: 90
 *   Scale 4: 110
 *   Scale 5: 130
 *   Scale 6: 150
 *   Scale 7: 170
 *   Scale 8: 190   (fine patterns, more visible)
 *
 * @param scale Current scale factor
 * @return Opacity value 0-255
 */
int EFFECT_getOpacity(int scale);

/**
 * Gets the opacity for generated effects (LINE, GRID, CRT).
 *
 * Generated effects have alpha baked into each pixel, so they always
 * return 255 (fully opaque surface) and let per-pixel alpha do the work.
 *
 * @param type Effect type (unused, always returns 255)
 * @return Always 255
 */
int EFFECT_getGeneratedOpacity(int type);

#endif /* __EFFECT_SYSTEM_H__ */
