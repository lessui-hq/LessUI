/**
 * effect_surface.c - CPU-based pattern scaling and tiling
 */

#include "effect_surface.h"
#include <stdlib.h>

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
	uint32_t* src_pixels = (uint32_t*)src->pixels;
	uint32_t* dst_pixels = (uint32_t*)scaled->pixels;

	for (int sy = 0; sy < src->h; sy++) {
		for (int sx = 0; sx < src->w; sx++) {
			uint32_t pixel = src_pixels[sy * src->w + sx];
			// Replicate this pixel in a scale x scale block
			for (int dy = 0; dy < scale; dy++) {
				for (int dx = 0; dx < scale; dx++) {
					int out_y = sy * scale + dy;
					int out_x = sx * scale + dx;
					dst_pixels[out_y * new_w + out_x] = pixel;
				}
			}
		}
	}

	return scaled;
}

SDL_Surface* EFFECT_createTiledSurface(const char* pattern_path, int scale, int target_w,
                                       int target_h) {
	if (!pattern_path || scale < 1 || target_w < 1 || target_h < 1)
		return NULL;

	// Load base pattern
	SDL_Surface* base = IMG_Load(pattern_path);
	if (!base)
		return NULL;

	// Scale it
	SDL_Surface* scaled = scaleSurface(base, scale);
	SDL_FreeSurface(base);

	if (!scaled)
		return NULL;

	// Create target surface
	SDL_Surface* tiled = SDL_CreateRGBSurface(0, target_w, target_h, 32, 0x00FF0000, 0x0000FF00,
	                                          0x000000FF, 0xFF000000);
	if (!tiled) {
		SDL_FreeSurface(scaled);
		return NULL;
	}

	// Tile the scaled pattern
	int pattern_w = scaled->w;
	int pattern_h = scaled->h;

	for (int y = 0; y < target_h; y += pattern_h) {
		for (int x = 0; x < target_w; x += pattern_w) {
			SDL_Rect dst = {x, y, pattern_w, pattern_h};
			SDL_BlitSurface(scaled, NULL, tiled, &dst);
		}
	}

	SDL_FreeSurface(scaled);
	return tiled;
}
