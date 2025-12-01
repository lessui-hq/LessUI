/**
 * render_sdl2.c - Shared SDL2 rendering backend implementation
 *
 * This module consolidates the video rendering code that was previously
 * duplicated across 7 platform files: tg5040, rg35xxplus, my282, my355,
 * zero28, rgb30, magicmini.
 *
 * Key features unified:
 * - resizeVideo() with hard_scale calculation
 * - updateEffect() with opacity tables
 * - PLAT_flip() with aspect ratio handling
 * - Crisp scaling two-pass rendering
 * - Display rotation support
 */

#include "render_sdl2.h"

#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "effect_utils.h"
#include "render_common.h"
#include "utils.h"

// Internal helper to resize video resources
static void resizeVideoInternal(SDL2_RenderContext* ctx, int w, int h, int p) {
	if (w == ctx->width && h == ctx->height && p == ctx->pitch)
		return;

	// Calculate hard scale based on source resolution
	ctx->hard_scale = RENDER_calcHardScale(w, h, ctx->device_width, ctx->device_height);

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n", w, h, p, ctx->hard_scale,
	         ctx->sharpness == SHARPNESS_CRISP);

	// Cleanup old resources
	SDL_FreeSurface(ctx->buffer);
	SDL_DestroyTexture(ctx->texture);
	if (ctx->target) {
		SDL_DestroyTexture(ctx->target);
		ctx->target = NULL;
	}

	// Create main texture with appropriate filtering
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY,
	                        ctx->sharpness == SHARPNESS_SOFT ? "1" : "0", SDL_HINT_OVERRIDE);
	ctx->texture =
	    SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);

	// Create intermediate target texture for crisp scaling
	if (ctx->sharpness == SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		ctx->target =
		    SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET,
		                      w * ctx->hard_scale, h * ctx->hard_scale);
	}

	// Recreate buffer wrapper
	ctx->buffer = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);

	ctx->width = w;
	ctx->height = h;
	ctx->pitch = p;
}

// Internal helper to update effect texture
static void updateEffectInternal(SDL2_RenderContext* ctx) {
	EffectState* fx = &ctx->effect_state;

	// Apply pending state
	EFFECT_applyPending(fx);

	// Check if update needed
	if (!EFFECT_needsUpdate(fx)) {
		return;
	}

	// Effect is disabled
	if (fx->type == EFFECT_NONE) {
		return;
	}

	// Get pattern path and opacity
	char pattern_path[256];
	const char* pattern = NULL;
	int opacity = 255;

	// All platforms use scale-specific patterns (line-2.png, grid-3.png, etc.)
	pattern = EFFECT_getPatternPath(pattern_path, sizeof(pattern_path), fx->type, fx->scale);
	opacity = EFFECT_getOpacity(fx->scale);

	if (!pattern) {
		return;
	}

	// Target dimensions
	int target_w = ctx->device_width;
	int target_h = ctx->device_height;

	LOG_info("Effect: creating type=%d scale=%d opacity=%d pattern=%s\n", fx->type, fx->scale,
	         opacity, pattern);

	// Load and tile pattern
	SDL_Texture* tiled = EFFECT_loadAndTile(ctx->renderer, pattern, 1, target_w, target_h);
	if (tiled) {
		SDL_SetTextureBlendMode(tiled, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(tiled, opacity);

		// Destroy old effect texture
		if (ctx->effect) {
			SDL_DestroyTexture(ctx->effect);
		}
		ctx->effect = tiled;

		// Mark as live
		EFFECT_markLive(fx);

		LOG_info("Effect: created %dx%d texture, opacity=%d\n", target_w, target_h, opacity);
	}
}

SDL_Surface* SDL2_initVideo(SDL2_RenderContext* ctx, int width, int height,
                            const SDL2_Config* config) {
	// Initialize context
	memset(ctx, 0, sizeof(SDL2_RenderContext));

	// Copy config or use defaults
	if (config) {
		ctx->config = *config;
	} else {
		ctx->config.auto_rotate = 0;
		ctx->config.has_hdmi = 0;
		ctx->config.brightness_alpha = 0;
		ctx->config.default_sharpness = SHARPNESS_SOFT;
	}

	// Initialize effect state
	EFFECT_init(&ctx->effect_state);

	// Initialize SDL video
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);

	int w = width;
	int h = height;
	int p = w * FIXED_BPP;

	// Create window and renderer
	ctx->window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
	                               SDL_WINDOW_SHOWN);
	ctx->renderer =
	    SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	// Check for rotation (portrait display)
	if (ctx->config.auto_rotate) {
		SDL_DisplayMode mode;
		SDL_GetCurrentDisplayMode(0, &mode);
		LOG_info("Display mode: %ix%i\n", mode.w, mode.h);
		if (mode.h > mode.w) {
			ctx->rotate = 3; // 270 degrees
		}
	}

	// Create initial texture
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
	            ctx->config.default_sharpness == SHARPNESS_SOFT ? "1" : "0");
	ctx->texture =
	    SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
	ctx->target = NULL;

	// Create surfaces
	ctx->buffer = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	ctx->screen = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, FIXED_DEPTH, RGBA_MASK_565);

	// Store dimensions
	ctx->width = w;
	ctx->height = h;
	ctx->pitch = p;
	ctx->device_width = w;
	ctx->device_height = h;
	ctx->device_pitch = p;

	ctx->sharpness = ctx->config.default_sharpness;
	ctx->hard_scale = 4;

	return ctx->screen;
}

void SDL2_quitVideo(SDL2_RenderContext* ctx) {
	// Clear screen
	SDL_FillRect(ctx->screen, NULL, 0);
	for (int i = 0; i < 3; i++) {
		SDL_RenderClear(ctx->renderer);
		SDL_RenderPresent(ctx->renderer);
	}

	// Free surfaces
	SDL_FreeSurface(ctx->screen);
	SDL_FreeSurface(ctx->buffer);

	// Destroy textures
	if (ctx->target)
		SDL_DestroyTexture(ctx->target);
	if (ctx->effect)
		SDL_DestroyTexture(ctx->effect);
	SDL_DestroyTexture(ctx->texture);

	// Destroy renderer and window
	SDL_DestroyRenderer(ctx->renderer);
	SDL_DestroyWindow(ctx->window);

	SDL_Quit();
}

void SDL2_clearVideo(SDL2_RenderContext* ctx) {
	SDL_FillRect(ctx->screen, NULL, 0);
}

void SDL2_clearAll(SDL2_RenderContext* ctx) {
	SDL2_clearVideo(ctx);
	SDL_RenderClear(ctx->renderer);
}

SDL_Surface* SDL2_resizeVideo(SDL2_RenderContext* ctx, int width, int height, int pitch) {
	resizeVideoInternal(ctx, width, height, pitch);
	return ctx->screen;
}

void SDL2_setSharpness(SDL2_RenderContext* ctx, int sharpness) {
	if (ctx->sharpness == sharpness)
		return;

	// Force resize to recreate textures with new filtering
	int p = ctx->pitch;
	ctx->pitch = 0;
	ctx->sharpness = sharpness;
	resizeVideoInternal(ctx, ctx->width, ctx->height, p);
}

void SDL2_setEffect(SDL2_RenderContext* ctx, int type) {
	EFFECT_setType(&ctx->effect_state, type);
}

void SDL2_setEffectColor(SDL2_RenderContext* ctx, int color) {
	EFFECT_setColor(&ctx->effect_state, color);
}

scaler_t SDL2_getScaler(SDL2_RenderContext* ctx, GFX_Renderer* renderer) {
	EFFECT_setScale(&ctx->effect_state, renderer->scale);
	return scale1x1_c16; // Hardware does scaling
}

void SDL2_blitRenderer(SDL2_RenderContext* ctx, GFX_Renderer* renderer) {
	ctx->blit = renderer;
	SDL_RenderClear(ctx->renderer);
	resizeVideoInternal(ctx, renderer->true_w, renderer->true_h, renderer->src_p);
}

void SDL2_flip(SDL2_RenderContext* ctx, int sync) {
	// UI mode (no blit renderer)
	if (!ctx->blit) {
		resizeVideoInternal(ctx, ctx->device_width, ctx->device_height, ctx->device_pitch);
		SDL_UpdateTexture(ctx->texture, NULL, ctx->screen->pixels, ctx->screen->pitch);

		if (ctx->rotate && !ctx->on_hdmi) {
			// Rotated render
			SDL_RenderCopyEx(
			    ctx->renderer, ctx->texture, NULL,
			    &(SDL_Rect){0, ctx->device_width, ctx->device_width, ctx->device_height},
			    ctx->rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
		} else {
			SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
		}

		SDL_RenderPresent(ctx->renderer);
		return;
	}

	// Game mode - update texture from renderer source
	SDL_UpdateTexture(ctx->texture, NULL, ctx->blit->src, ctx->blit->src_p);

	// Apply crisp scaling if enabled
	SDL_Texture* target = ctx->texture;
	int x = ctx->blit->src_x;
	int y = ctx->blit->src_y;
	int w = ctx->blit->src_w;
	int h = ctx->blit->src_h;

	if (ctx->sharpness == SHARPNESS_CRISP && ctx->target) {
		SDL_SetRenderTarget(ctx->renderer, ctx->target);
		SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
		SDL_SetRenderTarget(ctx->renderer, NULL);
		x *= ctx->hard_scale;
		y *= ctx->hard_scale;
		w *= ctx->hard_scale;
		h *= ctx->hard_scale;
		target = ctx->target;
	}

	// Calculate destination rectangle
	SDL_Rect src_rect = {x, y, w, h};
	RenderDestRect dest = RENDER_calcDestRect(ctx->blit, ctx->device_width, ctx->device_height);
	SDL_Rect dst_rect = {dest.x, dest.y, dest.w, dest.h};

	// Render main content
	if (ctx->rotate && !ctx->on_hdmi) {
		// Calculate rotation offsets
		int ox = -(ctx->device_width - ctx->device_height) / 2;
		int oy = -ox;
		SDL_RenderCopyEx(ctx->renderer, target, &src_rect,
		                 &(SDL_Rect){ox + dst_rect.x, oy + dst_rect.y, dst_rect.w, dst_rect.h},
		                 ctx->rotate * 90, NULL, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopy(ctx->renderer, target, &src_rect, &dst_rect);
	}

	// Update and render effect overlay
	updateEffectInternal(ctx);
	if (ctx->blit && ctx->effect_state.type != EFFECT_NONE && ctx->effect) {
		SDL_Rect effect_src = {0, 0, dst_rect.w, dst_rect.h};
		if (ctx->rotate && !ctx->on_hdmi) {
			int ox = -(ctx->device_width - ctx->device_height) / 2;
			int oy = -ox;
			SDL_RenderCopyEx(ctx->renderer, ctx->effect, &effect_src,
			                 &(SDL_Rect){ox + dst_rect.x, oy + dst_rect.y, dst_rect.w, dst_rect.h},
			                 ctx->rotate * 90, NULL, SDL_FLIP_NONE);
		} else {
			SDL_RenderCopy(ctx->renderer, ctx->effect, &effect_src, &dst_rect);
		}
	}

	// Present
	SDL_RenderPresent(ctx->renderer);
	ctx->blit = NULL;
}

void SDL2_vsync(int remaining) {
	if (remaining > 0) {
		SDL_Delay(remaining);
	}
}

int SDL2_hdmiChanged(SDL2_RenderContext* ctx) {
	// Platform-specific HDMI detection should set ctx->on_hdmi
	// This function just reports if it changed
	// (Actual detection happens in platform code)
	return 0;
}
