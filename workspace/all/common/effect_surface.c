/**
 * effect_surface.c - CPU-based pattern scaling and tiling
 */

#include "effect_surface.h"

#include <stdlib.h>

#include "log.h"
#include "render_common.h"

/**
 * Helper to apply color tint to a surface's non-transparent pixels.
 * Replaces RGB values while preserving alpha channel.
 */
static void tintSurface(SDL_Surface* surface, int color) {
	if (!surface || !color)
		return;

	// Convert RGB565 to RGB888
	uint8_t r, g, b;
	RENDER_rgb565ToRgb888(color, &r, &g, &b);

	uint32_t* pixels = (uint32_t*)surface->pixels;
	int pixel_count = surface->w * surface->h;

	for (int i = 0; i < pixel_count; i++) {
		uint32_t pixel = pixels[i];
		uint8_t a = (pixel >> 24) & 0xFF;
		// Only tint pixels that have alpha (are visible)
		if (a > 0) {
			pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
}

/**
 * Scales an SDL_Surface using nearest-neighbor (pixel replication).
 */
static SDL_Surface* scaleSurface(SDL_Surface* src, int scale) {
	if (!src || scale < 1)
		return NULL;

	int new_w = src->w * scale;
	int new_h = src->h * scale;

	SDL_Surface* scaled =
	    SDL_CreateRGBSurface(0, new_w, new_h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (!scaled)
		return NULL;

	// Simple nearest-neighbor: replicate each pixel scale x scale times
	// Use pitch for row stride (not width) to handle potential padding
	for (int sy = 0; sy < src->h; sy++) {
		uint32_t* src_row = (uint32_t*)((uint8_t*)src->pixels + sy * src->pitch);
		for (int sx = 0; sx < src->w; sx++) {
			uint32_t pixel = src_row[sx];
			// Replicate this pixel in a scale x scale block
			for (int dy = 0; dy < scale; dy++) {
				int out_y = sy * scale + dy;
				uint32_t* dst_row = (uint32_t*)((uint8_t*)scaled->pixels + out_y * scaled->pitch);
				for (int dx = 0; dx < scale; dx++) {
					int out_x = sx * scale + dx;
					dst_row[out_x] = pixel;
				}
			}
		}
	}

	return scaled;
}

SDL_Surface* EFFECT_createTiledSurface(const char* pattern_path, int scale, int target_w,
                                       int target_h) {
	return EFFECT_createTiledSurfaceWithColor(pattern_path, scale, target_w, target_h, 0);
}

SDL_Surface* EFFECT_createTiledSurfaceWithColor(const char* pattern_path, int scale, int target_w,
                                                int target_h, int color) {
	if (!pattern_path || scale < 1 || target_w < 1 || target_h < 1) {
		LOG_info("EFFECT_createTiledSurfaceWithColor: invalid params\n");
		return NULL;
	}

	// Load base pattern
	SDL_Surface* loaded = IMG_Load(pattern_path);
	if (!loaded) {
		LOG_info("EFFECT_createTiledSurface: IMG_Load failed for %s: %s\n", pattern_path,
		         IMG_GetError());
		return NULL;
	}
	LOG_info("EFFECT_createTiledSurface: loaded %s (%dx%d bpp=%d Amask=0x%08X pitch=%d)\n",
	         pattern_path, loaded->w, loaded->h, loaded->format->BitsPerPixel,
	         loaded->format->Amask, loaded->pitch);

	// Convert to 32-bit ARGB if needed (scaleSurface assumes 32-bit)
	SDL_Surface* base;
	if (loaded->format->BitsPerPixel != 32 || loaded->format->Amask != 0xFF000000) {
		SDL_Surface* converted = SDL_CreateRGBSurface(0, loaded->w, loaded->h, 32, 0x00FF0000,
		                                              0x0000FF00, 0x000000FF, 0xFF000000);
		if (converted) {
			SDLX_SetAlpha(loaded, 0, 255); // Disable alpha for copy
			SDL_BlitSurface(loaded, NULL, converted, NULL);
			base = converted;
			LOG_info("EFFECT_createTiledSurface: converted to 32-bit ARGB\n");
		} else {
			base = loaded;
			loaded = NULL;
		}
		if (loaded)
			SDL_FreeSurface(loaded);
	} else {
		base = loaded;
	}

	// Scale it
	SDL_Surface* scaled = scaleSurface(base, scale);
	SDL_FreeSurface(base);

	if (!scaled) {
		LOG_info("EFFECT_createTiledSurface: scaleSurface failed\n");
		return NULL;
	}

	// Create target surface
	SDL_Surface* tiled = SDL_CreateRGBSurface(0, target_w, target_h, 32, 0x00FF0000, 0x0000FF00,
	                                          0x000000FF, 0xFF000000);
	if (!tiled) {
		LOG_info("EFFECT_createTiledSurface: SDL_CreateRGBSurface failed\n");
		SDL_FreeSurface(scaled);
		return NULL;
	}

	// Tile the scaled pattern
	// IMPORTANT: Disable alpha blending during tiling so we get a straight pixel copy
	// (otherwise alpha blending black-on-black gives black, losing alpha values)
	SDLX_SetAlpha(scaled, 0, 255);

	int pattern_w = scaled->w;
	int pattern_h = scaled->h;

	for (int y = 0; y < target_h; y += pattern_h) {
		for (int x = 0; x < target_w; x += pattern_w) {
			SDL_Rect dst = {x, y, pattern_w, pattern_h};
			SDL_BlitSurface(scaled, NULL, tiled, &dst);
		}
	}

	SDL_FreeSurface(scaled);

	// Apply color tinting if specified
	if (color) {
		tintSurface(tiled, color);
	}

	LOG_info("EFFECT_createTiledSurfaceWithColor: created %dx%d tiled surface (color=0x%04x)\n",
	         target_w, target_h, color);
	return tiled;
}
