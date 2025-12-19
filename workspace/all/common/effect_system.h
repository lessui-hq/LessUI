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
 * All effects (LINE, GRID, GRILLE, SLOT) are procedurally generated at runtime
 * via effect_generate.c functions.
 *
 * @param type Effect type to check
 * @return 1 if type uses generation, 0 otherwise
 */
int EFFECT_usesGeneration(int type);

/**
 * Gets the global opacity for effect overlays.
 *
 * Returns constant opacity (128) for all scales. Per-pixel alpha values in
 * the generated patterns control pattern structure; this global opacity
 * controls overall visibility.
 *
 * @param scale Current scale factor (unused, kept for API compatibility)
 * @return Opacity value (always 128)
 */
int EFFECT_getOpacity(int scale);

#endif /* __EFFECT_SYSTEM_H__ */
