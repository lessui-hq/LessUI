/**
 * frame_pacer.c - Display-agnostic frame pacing implementation
 *
 * Uses Q16.16 fixed-point arithmetic for precision without float drift.
 * Q16.16 means: 16 bits integer, 16 bits fraction (multiply by 65536).
 */

#include "frame_pacer.h"
#include <math.h>

// Platform function we need - declared in api.h but we avoid including it
// to keep frame_pacer testable without SDL dependencies
extern double PLAT_getDisplayHz(void);

// Q16.16 conversion factor
#define Q16_SHIFT 16
#define Q16_SCALE 65536.0

void FramePacer_init(FramePacer* pacer, double game_fps, double display_hz) {
	// Convert to Q16.16 fixed-point for precise integer math
	// 59.73fps becomes 3,913,359 (59.73 * 65536)
	pacer->game_fps_q16 = (int32_t)(game_fps * Q16_SCALE);
	pacer->display_hz_q16 = (int32_t)(display_hz * Q16_SCALE);

	// Initialize accumulator to display_hz so first vsync triggers a step
	// This avoids showing a black/stale frame on startup
	pacer->accumulator = pacer->display_hz_q16;

	// Direct mode if rates are within tolerance
	// This handles 59.94fps @ 60Hz, etc.
	double diff = fabs(game_fps - display_hz) / display_hz;
	pacer->direct_mode = (diff < FRAME_PACER_TOLERANCE);
}

bool FramePacer_step(FramePacer* pacer) {
	// Direct mode: always step
	if (pacer->direct_mode) {
		return true;
	}

	// Bresenham accumulator: check threshold THEN add
	// Since we initialized to display_hz, first call will step
	if (pacer->accumulator >= pacer->display_hz_q16) {
		pacer->accumulator -= pacer->display_hz_q16;
		pacer->accumulator += pacer->game_fps_q16;
		return true;
	}

	// Not enough accumulated - repeat frame
	pacer->accumulator += pacer->game_fps_q16;
	return false;
}

void FramePacer_reset(FramePacer* pacer) {
	// Reset to display_hz so next vsync triggers a step
	pacer->accumulator = pacer->display_hz_q16;
}

bool FramePacer_isDirectMode(const FramePacer* pacer) {
	return pacer->direct_mode;
}

double FramePacer_getDisplayHz(void) {
	// Use platform-provided display Hz directly.
	// On SDL2 platforms, this queries SDL_GetCurrentDisplayMode().
	// On SDL1 or platforms where SDL doesn't know, this returns a hardcoded value.
	return PLAT_getDisplayHz();
}
