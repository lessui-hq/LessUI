/**
 * frame_pacer.c - Display-agnostic frame pacing implementation
 *
 * Uses Q16.16 fixed-point arithmetic for precision without float drift.
 * Q16.16 means: 16 bits integer, 16 bits fraction (multiply by 65536).
 */

#include "frame_pacer.h"
#include "log.h"
#include "utils.h" // For getMicroseconds
#include <math.h>

// Platform function we need - declared in api.h but we avoid including it
// to keep frame_pacer testable without SDL dependencies
extern double PLAT_getDisplayHz(void);

// Q16.16 conversion factor
#define Q16_SHIFT 16
#define Q16_SCALE 65536.0

void FramePacer_init(FramePacer* pacer, double game_fps, double display_hz) {
	// Fallback to 60Hz if display_hz detection failed
	if (display_hz <= 0.0) {
		display_hz = 60.0;
	}

	// Store original game fps for potential reinit with measured Hz
	pacer->game_fps = game_fps;

	// Convert to Q16.16 fixed-point for precise integer math
	// 59.73fps becomes 3,913,359 (59.73 * 65536)
	pacer->game_fps_q16 = (int32_t)(game_fps * Q16_SCALE);
	pacer->display_hz_q16 = (int32_t)(display_hz * Q16_SCALE);

	// Initialize accumulator to display_hz so first vsync triggers a step
	// This avoids showing a black/stale frame on startup
	pacer->accumulator = pacer->display_hz_q16;

	// Initialize vsync measurement state
	pacer->last_vsync_time = 0;
	pacer->measured_hz = 0.0;
	pacer->vsync_samples = 0;

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

// Smoothing factor for EMA: 0.01 = very smooth (100 frame time constant)
// Lower values = more stable but slower to converge
#define VSYNC_EMA_ALPHA 0.01

// Minimum Hz to accept (reject outliers from frame drops)
#define VSYNC_MIN_HZ 50.0
// Maximum Hz to accept (reject outliers from fast presents)
#define VSYNC_MAX_HZ 120.0

void FramePacer_recordVsync(FramePacer* pacer) {
	uint64_t now = getMicroseconds();

	if (pacer->last_vsync_time > 0) {
		// Calculate interval in seconds (getMicroseconds returns µs)
		double interval = (double)(now - pacer->last_vsync_time) / 1000000.0;

		// Convert to Hz
		double hz = 1.0 / interval;

		// Reject outliers (frame drops, fast presents, etc.)
		if (hz >= VSYNC_MIN_HZ && hz <= VSYNC_MAX_HZ) {
			pacer->vsync_samples++;

			if (pacer->measured_hz == 0.0) {
				// First sample: initialize directly
				pacer->measured_hz = hz;
			} else {
				// Exponential moving average for stability
				pacer->measured_hz =
				    pacer->measured_hz * (1.0 - VSYNC_EMA_ALPHA) + hz * VSYNC_EMA_ALPHA;
			}

			// Log when measurement becomes stable
			if (pacer->vsync_samples == FRAME_PACER_VSYNC_WARMUP) {
				double reported_hz = PLAT_getDisplayHz();
				LOG_info("Vsync measurement stable: %.3fHz (reported: %.1fHz, diff: %.2f%%)\n",
				         pacer->measured_hz, reported_hz,
				         fabs(pacer->measured_hz - reported_hz) / reported_hz * 100.0);
			}

			// Check for drift and reinit if needed (both at warmup and periodically after)
			// Check every 300 samples after warmup to catch drift
			if (pacer->vsync_samples >= FRAME_PACER_VSYNC_WARMUP &&
			    (pacer->vsync_samples == FRAME_PACER_VSYNC_WARMUP ||
			     pacer->vsync_samples % 300 == 0)) {
				double current_hz = pacer->display_hz_q16 / Q16_SCALE;
				double diff = fabs(pacer->measured_hz - current_hz) / current_hz;
				if (diff > 0.001) { // >0.1% difference
					LOG_info("Display Hz drift detected: %.3f -> %.3f (%.2f%% change)\n",
					         current_hz, pacer->measured_hz, diff * 100.0);

					// Update display Hz in Q16.16
					pacer->display_hz_q16 = (int32_t)(pacer->measured_hz * Q16_SCALE);

					// Reset accumulator to new display_hz to avoid frame skip glitches
					// When Hz changes, the old accumulator state is invalid
					pacer->accumulator = pacer->display_hz_q16;

					// Re-evaluate direct mode with new Hz
					double fps_diff =
					    fabs(pacer->game_fps - pacer->measured_hz) / pacer->measured_hz;
					bool should_be_direct = (fps_diff < FRAME_PACER_TOLERANCE);
					if (pacer->direct_mode != should_be_direct) {
						LOG_info("Frame pacer mode changed: %s -> %s\n",
						         pacer->direct_mode ? "direct" : "paced",
						         should_be_direct ? "direct" : "paced");
						pacer->direct_mode = should_be_direct;
					}
				}
			}
		}
	}

	pacer->last_vsync_time = now;
}

double FramePacer_getMeasuredHz(const FramePacer* pacer) {
	if (pacer->vsync_samples >= FRAME_PACER_VSYNC_WARMUP) {
		return pacer->measured_hz;
	}
	return 0.0; // Not enough samples yet
}

bool FramePacer_isMeasurementStable(const FramePacer* pacer) {
	return pacer->vsync_samples >= FRAME_PACER_VSYNC_WARMUP;
}
