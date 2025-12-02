/**
 * render_sdl2.h - Shared SDL2 rendering backend
 *
 * This module provides a unified SDL2 rendering implementation used by
 * multiple platforms: tg5040, rg35xxplus, rgb30, my282, my355, zero28, magicmini.
 *
 * Features:
 * - Hardware-accelerated rendering via SDL2 Renderer
 * - Crisp scaling (nearest-neighbor upscale + linear downscale)
 * - Display effects (scanlines, grid, CRT)
 * - HDMI support with resolution switching
 * - Display rotation for portrait screens
 * - Aspect ratio preservation and centering
 *
 * Usage:
 *   SDL2_RenderContext ctx;
 *   SDL2_Config config = { .auto_rotate = true, .has_hdmi = false };
 *   SDL_Surface* screen = SDL2_initVideo(&ctx, 640, 480, &config);
 *   // ... rendering loop ...
 *   SDL2_quitVideo(&ctx);
 */

#ifndef __RENDER_SDL2_H__
#define __RENDER_SDL2_H__

#include "sdl.h"

#include "api.h"
#include "effect_system.h"
#include "scaler.h"

/**
 * SDL2 render backend configuration.
 *
 * Platform-specific settings that control rendering behavior.
 * Set once during initialization.
 */
typedef struct SDL2_Config {
	int auto_rotate; // Auto-detect and apply rotation for portrait displays
	int has_hdmi; // Platform supports HDMI output
	int brightness_alpha; // Use brightness for effect alpha (magicmini)
	int default_sharpness; // Initial sharpness (SHARPNESS_SOFT, SHARPNESS_CRISP, SHARPNESS_SHARP)
} SDL2_Config;

/**
 * SDL2 render context.
 *
 * Contains all state for the SDL2 rendering backend. One instance per platform.
 * Allocated by caller, initialized by SDL2_initVideo().
 */
typedef struct SDL2_RenderContext {
	// SDL resources
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture; // Main texture (source resolution)
	SDL_Texture* target; // Intermediate texture for crisp scaling
	SDL_Texture* effect; // Effect overlay texture

	// Surfaces
	SDL_Surface* buffer; // Wrapper for texture lock (unused pixels pointer)
	SDL_Surface* screen; // Main screen surface for UI rendering

	// Current renderer (set during blitRenderer)
	GFX_Renderer* blit;

	// Video dimensions
	int width; // Current source width
	int height; // Current source height
	int pitch; // Current source pitch

	// Device dimensions
	int device_width; // Screen width (may differ from FIXED_WIDTH on HDMI)
	int device_height; // Screen height
	int device_pitch; // Screen pitch

	// Rendering settings
	int sharpness; // SHARPNESS_SOFT, SHARPNESS_CRISP, SHARPNESS_SHARP
	int hard_scale; // Crisp scaling multiplier (1, 2, or 4)
	int rotate; // Rotation in 90-degree increments (0-3)

	// Effect state
	EffectState effect_state;

	// Platform config
	SDL2_Config config;

	// HDMI state
	int on_hdmi; // Currently outputting to HDMI
} SDL2_RenderContext;

/**
 * Initializes SDL2 video subsystem.
 *
 * Creates SDL window, renderer, textures, and surfaces. Configures
 * rendering based on platform config (rotation, HDMI, etc.).
 *
 * @param ctx     Render context to initialize
 * @param width   Initial screen width (typically FIXED_WIDTH)
 * @param height  Initial screen height (typically FIXED_HEIGHT)
 * @param config  Platform-specific configuration (may be NULL for defaults)
 * @return Screen surface for rendering, or NULL on error
 */
SDL_Surface* SDL2_initVideo(SDL2_RenderContext* ctx, int width, int height,
                            const SDL2_Config* config);

/**
 * Shuts down SDL2 video subsystem.
 *
 * Destroys all SDL resources and cleans up context.
 *
 * @param ctx Render context to cleanup
 */
void SDL2_quitVideo(SDL2_RenderContext* ctx);

/**
 * Clears the screen surface to black.
 *
 * @param ctx Render context
 */
void SDL2_clearVideo(SDL2_RenderContext* ctx);

/**
 * Clears both screen surface and renderer.
 *
 * @param ctx Render context
 */
void SDL2_clearAll(SDL2_RenderContext* ctx);

/**
 * Resizes video output for new source dimensions.
 *
 * Recreates textures and calculates hard_scale factor based on
 * new source resolution vs device resolution.
 *
 * @param ctx    Render context
 * @param width  New source width
 * @param height New source height
 * @param pitch  New source pitch
 * @return Screen surface (unchanged)
 */
SDL_Surface* SDL2_resizeVideo(SDL2_RenderContext* ctx, int width, int height, int pitch);

/**
 * Sets sharpness mode for scaling.
 *
 * @param ctx       Render context
 * @param sharpness SHARPNESS_SOFT (bilinear), SHARPNESS_CRISP (NN+linear), or SHARPNESS_SHARP (NN)
 */
void SDL2_setSharpness(SDL2_RenderContext* ctx, int sharpness);

/**
 * Sets effect type for next frame.
 *
 * @param ctx  Render context
 * @param type Effect type (EFFECT_NONE, EFFECT_LINE, EFFECT_GRID, etc.)
 */
void SDL2_setEffect(SDL2_RenderContext* ctx, int type);

/**
 * Sets effect color (for DMG grid colorization).
 *
 * @param ctx   Render context
 * @param color RGB565 color value
 */
void SDL2_setEffectColor(SDL2_RenderContext* ctx, int color);

/**
 * Gets scaler function for current renderer.
 *
 * Updates effect scale state based on renderer dimensions.
 * For SDL2 backend, always returns scale1x1_c16 since hardware does scaling.
 *
 * @param ctx      Render context
 * @param renderer Current GFX_Renderer
 * @return Scaler function pointer
 */
scaler_t SDL2_getScaler(SDL2_RenderContext* ctx, GFX_Renderer* renderer);

/**
 * Prepares for frame rendering.
 *
 * Stores renderer reference and resizes video if needed.
 * Call before PLAT_flip().
 *
 * @param ctx      Render context
 * @param renderer GFX_Renderer with source buffer and dimensions
 */
void SDL2_blitRenderer(SDL2_RenderContext* ctx, GFX_Renderer* renderer);

/**
 * Presents rendered frame to display.
 *
 * Updates texture from source, applies scaling and effects,
 * handles rotation if needed, and presents to screen.
 *
 * @param ctx  Render context
 * @param sync Unused (vsync always enabled via SDL_RENDERER_PRESENTVSYNC)
 */
void SDL2_flip(SDL2_RenderContext* ctx, int sync);

/**
 * Delays to maintain frame timing.
 *
 * @param remaining Milliseconds to delay
 */
void SDL2_vsync(int remaining);

/**
 * Checks if HDMI connection state changed.
 *
 * Only functional on platforms with has_hdmi=true.
 *
 * @param ctx Render context
 * @return 1 if HDMI state changed, 0 otherwise
 */
int SDL2_hdmiChanged(SDL2_RenderContext* ctx);

#endif /* __RENDER_SDL2_H__ */
