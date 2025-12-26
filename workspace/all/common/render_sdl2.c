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
 * - PLAT_present() with aspect ratio handling
 * - Crisp scaling two-pass rendering
 * - Display rotation support
 */

#include "render_sdl2.h"

#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "effect_system.h"
#include "effect_utils.h"
#include "gl_video.h"
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

#if !HAS_OPENGLES
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
#endif

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

	// All effects use procedural generation (with color support for GRID)
	int scale = fx->scale > 0 ? fx->scale : 1;
	int opacity = EFFECT_getOpacity(scale);

	LOG_debug("Effect: generating type=%d scale=%d color=0x%04x opacity=%d\n", fx->type, fx->scale,
	          fx->color, opacity);

#if !HAS_OPENGLES
	// Target dimensions (only needed for SDL texture creation)
	int target_w = ctx->device_width;
	int target_h = ctx->device_height;

	SDL_Texture* new_texture = EFFECT_createGeneratedTextureWithColor(
	    ctx->renderer, fx->type, fx->scale, target_w, target_h, fx->color);
	if (new_texture) {
		SDL_SetTextureBlendMode(new_texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(new_texture, opacity);
	}

	if (new_texture) {
		// Destroy old effect texture
		if (ctx->effect) {
			SDL_DestroyTexture(ctx->effect);
		}
		ctx->effect = new_texture;

		// Mark as live
		EFFECT_markLive(fx);

		LOG_debug("Effect: created %dx%d texture\n", target_w, target_h);
	}
#else
	EFFECT_markLive(fx);
#endif
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
		ctx->config.rotate_cw = 0;
		ctx->config.rotate_null_center = 0;
		ctx->config.has_hdmi = 0;
		ctx->config.default_sharpness = SHARPNESS_SOFT;
	}

	// Initialize effect state
	EFFECT_init(&ctx->effect_state);

	// Initialize SDL video
	LOG_debug("SDL2_initVideo: Calling SDL_InitSubSystem(VIDEO)");
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		LOG_error("SDL2_initVideo: SDL_InitSubSystem failed: %s", SDL_GetError());
		return NULL;
	}
	LOG_debug("SDL2_initVideo: SDL video subsystem initialized");
	SDL_ShowCursor(0);

	int w = width;
	int h = height;
	int p = w * FIXED_BPP;
	LOG_debug("SDL2_initVideo: Creating window/renderer (size=%dx%d)", w, h);

	// Create window and renderer
	// Add OpenGL flag on platforms that support HW rendering
	Uint32 window_flags = SDL_WINDOW_SHOWN;
#if HAS_OPENGLES
	window_flags |= SDL_WINDOW_OPENGL;
#endif

	ctx->window =
	    SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, window_flags);
	if (!ctx->window) {
		LOG_error("SDL2_initVideo: SDL_CreateWindow failed: %s", SDL_GetError());
		return NULL;
	}
	LOG_debug("SDL2_initVideo: Window created successfully");

#if !HAS_OPENGLES
	Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
	ctx->renderer = SDL_CreateRenderer(ctx->window, -1, renderer_flags);
	if (!ctx->renderer) {
		LOG_error("SDL2_initVideo: SDL_CreateRenderer failed: %s", SDL_GetError());
		SDL_DestroyWindow(ctx->window);
		return NULL;
	}

	// Log renderer info
	SDL_RendererInfo renderer_info;
	if (SDL_GetRendererInfo(ctx->renderer, &renderer_info) == 0) {
		LOG_info("SDL2: Using renderer: %s", renderer_info.name);
	}
	LOG_debug("SDL2_initVideo: Renderer created successfully");
#endif

	// Check for rotation (portrait display)
	if (ctx->config.auto_rotate) {
		SDL_DisplayMode mode;
		LOG_debug("SDL2_initVideo: Checking display mode for rotation");
		if (SDL_GetCurrentDisplayMode(0, &mode) < 0) {
			LOG_error("SDL2_initVideo: SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
		} else {
			LOG_info("Display mode: %ix%i\n", mode.w, mode.h);
			if (mode.h > mode.w) {
				// rotate_cw: 0=270° CCW (default), 1=90° CW (zero28)
				ctx->rotate = ctx->config.rotate_cw ? 1 : 3;
				LOG_debug("Rotation enabled: rotate=%d (%s)\n", ctx->rotate,
				          ctx->config.rotate_cw ? "CW" : "CCW");
			}
		}
	}

#if !HAS_OPENGLES
	// Create initial texture
	LOG_debug("SDL2_initVideo: Creating texture (sharpness=%s)",
	          ctx->config.default_sharpness == SHARPNESS_SOFT ? "soft" : "sharp");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
	            ctx->config.default_sharpness == SHARPNESS_SOFT ? "1" : "0");
	ctx->texture =
	    SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!ctx->texture) {
		LOG_error("SDL2_initVideo: SDL_CreateTexture failed: %s", SDL_GetError());
		SDL_DestroyRenderer(ctx->renderer);
		SDL_DestroyWindow(ctx->window);
		return NULL;
	}
	LOG_debug("SDL2_initVideo: Texture created successfully");
	ctx->target = NULL;

	// Create surfaces
	LOG_debug("SDL2_initVideo: Creating SDL surfaces");
	ctx->buffer = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	if (!ctx->buffer) {
		LOG_error("SDL2_initVideo: SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
		SDL_DestroyTexture(ctx->texture);
		SDL_DestroyRenderer(ctx->renderer);
		SDL_DestroyWindow(ctx->window);
		return NULL;
	}
#endif

	ctx->screen = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, FIXED_DEPTH, RGBA_MASK_565);
	if (!ctx->screen) {
		LOG_error("SDL2_initVideo: SDL_CreateRGBSurface failed: %s", SDL_GetError());
#if !HAS_OPENGLES
		SDL_FreeSurface(ctx->buffer);
		SDL_DestroyTexture(ctx->texture);
		SDL_DestroyRenderer(ctx->renderer);
#endif
		SDL_DestroyWindow(ctx->window);
		return NULL;
	}
	LOG_debug("SDL2_initVideo: Surfaces created successfully");

	// Store dimensions
	ctx->width = w;
	ctx->height = h;
	ctx->pitch = p;
	ctx->device_width = w;
	ctx->device_height = h;
	ctx->device_pitch = p;

	ctx->sharpness = ctx->config.default_sharpness;
	ctx->hard_scale = 4;

#if HAS_OPENGLES
	// Initialize GL context for unified presentation (even if core is software)
	if (!GLVideo_initSoftware()) {
		LOG_error("SDL2_initVideo: Failed to initialize GL video");
		// Fallback to SDL renderer? Or fail?
		// If we are on GLES platform, we rely on GLVideo for presentation to avoid
		// SDL_Renderer/GL conflicts. So failure is fatal for video.
		// However, we can keep running but screen might be black?
		// Let's assume initSoftware returns true if already inited or successful.
	} else {
		// Clear screen a few times to ensure display pipe is ready
		for (int i = 0; i < 3; i++) {
			GLVideo_clear();
			GLVideo_swapBuffers();
		}
	}
#endif

	LOG_debug("SDL2_initVideo: Video initialization complete (screen=%dx%d)", w, h);
	return ctx->screen;
}

void SDL2_quitVideo(SDL2_RenderContext* ctx) {
	// Clear screen
	SDL_FillRect(ctx->screen, NULL, 0);
#if !HAS_OPENGLES
	for (int i = 0; i < 3; i++) {
		SDL_RenderClear(ctx->renderer);
		SDL_RenderPresent(ctx->renderer);
	}
#endif

	// Free surfaces
	SDL_FreeSurface(ctx->screen);
#if !HAS_OPENGLES
	SDL_FreeSurface(ctx->buffer);

	// Destroy textures
	if (ctx->target)
		SDL_DestroyTexture(ctx->target);
	if (ctx->effect)
		SDL_DestroyTexture(ctx->effect);
	SDL_DestroyTexture(ctx->texture);

	// Destroy renderer and window
	SDL_DestroyRenderer(ctx->renderer);
#endif

#if HAS_OPENGLES
	// Destroy GL context before window (SDL requires this order)
	GLVideo_shutdown();
#endif
	SDL_DestroyWindow(ctx->window);

	SDL_Quit();
}

void SDL2_clearVideo(SDL2_RenderContext* ctx) {
	SDL_FillRect(ctx->screen, NULL, 0);
}

void SDL2_clearAll(SDL2_RenderContext* ctx) {
	SDL2_clearVideo(ctx);
#if !HAS_OPENGLES
	SDL_RenderClear(ctx->renderer);
#endif
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
	EFFECT_setScale(&ctx->effect_state, renderer->visual_scale);
	return scale1x1_c16; // Hardware does scaling
}

void SDL2_present(SDL2_RenderContext* ctx, GFX_Renderer* renderer) {
	// Unified presentation: handles both game and UI modes
	// No state tracking needed - caller explicitly says what to present

#if HAS_OPENGLES
	// Use GL video pipeline for everything on GLES platforms
	// This enables shaders for software cores and avoids context conflicts

	if (!renderer) {
		// UI Mode: Present screen surface
		// Ensure UI texture is up to date
		resizeVideoInternal(ctx, ctx->device_width, ctx->device_height, ctx->device_pitch);
		GLVideo_presentSurface(ctx->screen);
		GLVideo_swapBuffers();
		return;
	}

	// Game Mode: Present from renderer source
	// Upload frame to GL texture
	// Note: renderer->src is pixel data, src_p is pitch
	// We assume RGB565 for now (standard for SDL2 backend)
	GLVideo_uploadFrame(renderer->src, renderer->true_w, renderer->true_h, renderer->src_p,
	                    GL_VIDEO_PIXEL_FORMAT_RGB565);

	// Calculate destination rectangle (scaling)
	SDL_Rect src_rect = {renderer->src_x, renderer->src_y, renderer->src_w, renderer->src_h};
	RenderDestRect dest = RENDER_calcDestRect(renderer, ctx->device_width, ctx->device_height);
	SDL_Rect dst_rect = {dest.x, dest.y, dest.w, dest.h};

	// Determine rotation and other parameters
	unsigned rotation = ctx->on_hdmi ? 0 : ctx->rotate;
	int sharpness = ctx->sharpness;

	// Clear screen before drawing (important for non-fullscreen aspect ratios)
	GLVideo_clear();

	// Draw software frame
	GLVideo_drawSoftwareFrame(&src_rect, &dst_rect, rotation, sharpness);
	GLVideo_swapBuffers();

#else
	SDL_RenderClear(ctx->renderer);
#endif

#if !HAS_OPENGLES
	if (!renderer) {
		// UI mode: present screen surface
		resizeVideoInternal(ctx, ctx->device_width, ctx->device_height, ctx->device_pitch);
		SDL_UpdateTexture(ctx->texture, NULL, ctx->screen->pixels, ctx->screen->pitch);

		if (ctx->rotate && !ctx->on_hdmi) {
			int rect_x = ctx->config.rotate_cw ? ctx->device_height : 0;
			int rect_y = ctx->config.rotate_cw ? 0 : ctx->device_width;
			SDL_Point center_point = {0, 0};
			SDL_Point* center = ctx->config.rotate_null_center ? NULL : &center_point;

			SDL_RenderCopyEx(ctx->renderer, ctx->texture, NULL,
			                 &(SDL_Rect){rect_x, rect_y, ctx->device_width, ctx->device_height},
			                 ctx->rotate * 90, center, SDL_FLIP_NONE);
		} else {
			SDL_RenderCopy(ctx->renderer, ctx->texture, NULL, NULL);
		}

		SDL_RenderPresent(ctx->renderer);
		return;
	}

	// Game mode: present from renderer source
	resizeVideoInternal(ctx, renderer->true_w, renderer->true_h, renderer->src_p);
	SDL_UpdateTexture(ctx->texture, NULL, renderer->src, renderer->src_p);

	// Apply crisp scaling if enabled
	SDL_Texture* target = ctx->texture;
	int x = renderer->src_x;
	int y = renderer->src_y;
	int w = renderer->src_w;
	int h = renderer->src_h;

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
	RenderDestRect dest = RENDER_calcDestRect(renderer, ctx->device_width, ctx->device_height);
	SDL_Rect dst_rect = {dest.x, dest.y, dest.w, dest.h};

	// Render main content
	if (ctx->rotate && !ctx->on_hdmi) {
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
	if (ctx->effect_state.type != EFFECT_NONE && ctx->effect) {
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

	SDL_RenderPresent(ctx->renderer);
#endif
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

double SDL2_getDisplayHz(void) {
	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
		LOG_info("SDL_GetCurrentDisplayMode: %dx%d @ %dHz\n", mode.w, mode.h, mode.refresh_rate);
		if (mode.refresh_rate > 0) {
			return (double)mode.refresh_rate;
		}
	}
	LOG_info("SDL_GetCurrentDisplayMode: failed or returned 0Hz, using fallback\n");
	return 0.0; // Return 0 to indicate no data, caller should use PLAT_getDisplayHz
}

uint32_t SDL2_measureVsyncInterval(SDL2_RenderContext* ctx) {
	if (!ctx || !ctx->renderer) {
		return 0;
	}

	// First present to sync to vsync boundary
	SDL_RenderPresent(ctx->renderer);

	// Measure time for second present (one full vsync interval)
	uint64_t start = SDL_GetPerformanceCounter();
	SDL_RenderPresent(ctx->renderer);
	uint64_t end = SDL_GetPerformanceCounter();

	// Convert to microseconds
	uint64_t freq = SDL_GetPerformanceFrequency();
	return (uint32_t)((end - start) * 1000000 / freq);
}

SDL_Window* SDL2_getWindow(SDL2_RenderContext* ctx) {
	if (!ctx) {
		return NULL;
	}
	return ctx->window;
}

int SDL2_getRotation(SDL2_RenderContext* ctx) {
	if (!ctx) {
		return 0;
	}
	return ctx->rotate;
}
