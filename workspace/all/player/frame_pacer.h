/**
 * frame_pacer.h - Display-agnostic frame pacing
 *
 * Decouples emulation timing from display refresh rate using a Bresenham-style
 * fixed-point accumulator. Determines each vsync whether to step emulation or
 * repeat the previous frame.
 *
 * Example: 60fps game on 72Hz display
 *   - Vsync 1: acc >= hz -> step, acc -= hz (first frame always steps)
 *   - Vsync 2: acc < hz -> repeat
 *   - Vsync 3: acc >= hz -> step, acc -= hz
 *   - Result: 5 steps per 6 vsyncs (83.3%) = 60fps
 *
 * Benefits:
 *   - Q16.16 fixed-point: no floating-point drift, stable forever
 *   - Direct mode bypass: zero overhead on 60Hz displays
 *   - Pure functions: fully testable, no SDL/globals
 */

#ifndef __FRAME_PACER_H__
#define __FRAME_PACER_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Tolerance for "exact match" detection (direct mode bypass).
 *
 * Based on RetroArch's dynamic rate control research (Arntzen, 2012):
 * - Audio pitch changes ≤0.5% are inaudible to most listeners
 * - RetroArch recommends d = 0.2% to 0.5% for rate control
 * - Beyond ~0.5% mismatch, "other methods should be employed"
 *
 * Using 1% as a compromise - allows direct mode for well-matched displays
 * while triggering frame pacing for displays with noticeable drift.
 * After vsync measurement, the pacer may switch modes based on actual Hz.
 *
 * Examples at 1% tolerance:
 * - 59.94fps @ 60Hz → direct mode (0.1% diff) ✓
 * - 60.0fps @ 60.5Hz → direct mode (0.83% diff) ✓
 * - 60.0fps @ 61Hz → paced mode (1.6% diff)
 * - 50.0fps @ 60Hz → paced mode (16.7% diff)
 */
#define FRAME_PACER_TOLERANCE 0.01

/**
 * Number of vsync samples before measurement is considered stable.
 * At 60Hz, 120 samples = 2 seconds of measurement.
 */
#define FRAME_PACER_VSYNC_WARMUP 120

/**
 * Frame pacing state.
 *
 * Uses Q16.16 fixed-point (multiply by 65536) to preserve fractional precision
 * while avoiding floating point drift. For example, 59.73fps becomes 3,913,359.
 */
typedef struct {
	int32_t game_fps_q16; // Game FPS in Q16.16 fixed-point
	int32_t display_hz_q16; // Display Hz in Q16.16 fixed-point
	int32_t accumulator; // Bresenham accumulator (Q16.16)
	bool direct_mode; // True if fps ~= hz (skip accumulator)

	// Vsync measurement state
	uint64_t last_vsync_time; // Performance counter at last vsync
	double measured_hz; // Exponential moving average of measured Hz
	int vsync_samples; // Number of samples collected
	double game_fps; // Original game fps (for reinit)
} FramePacer;

/**
 * Initialize pacer for given game and display rates.
 *
 * Automatically detects if rates are close enough to use direct mode
 * (within FRAME_PACER_TOLERANCE).
 *
 * Accumulator is initialized to display_hz so the first vsync always
 * triggers a step (avoids showing a black/stale frame).
 *
 * @param pacer     Pacer state to initialize
 * @param game_fps  Game's target FPS (e.g., 60.0, 59.94, 50.0)
 * @param display_hz Display refresh rate in Hz (e.g., 60.0, 72.0)
 */
void FramePacer_init(FramePacer* pacer, double game_fps, double display_hz);

/**
 * Call once per vsync. Returns true if emulation should step.
 *
 * In direct mode, always returns true.
 * In paced mode, uses Bresenham accumulator to decide.
 *
 * @param pacer Pacer state (accumulator will be modified)
 * @return true if core.run() should be called, false to repeat last frame
 */
bool FramePacer_step(FramePacer* pacer);

/**
 * Reset accumulator to initial state (display_hz).
 *
 * Call on game load, state load, or any timing discontinuity.
 * Ensures first frame after reset will step.
 *
 * @param pacer Pacer state to reset
 */
void FramePacer_reset(FramePacer* pacer);

/**
 * Check if pacer is in direct mode.
 *
 * @param pacer Pacer state
 * @return true if direct mode (no pacing needed)
 */
bool FramePacer_isDirectMode(const FramePacer* pacer);

/**
 * Gets display refresh rate for frame pacing.
 *
 * Calls PLAT_getDisplayHz() which either:
 * - Queries SDL_GetCurrentDisplayMode() on SDL2 platforms
 * - Returns a hardcoded value for the platform's panel
 *
 * @return Display Hz (e.g., 60.0, 72.0, 73.0)
 */
double FramePacer_getDisplayHz(void);

/**
 * Record vsync timing after present.
 *
 * Call this immediately after GFX_present() or SDL_RenderPresent() returns.
 * Measures time between vsyncs to determine actual display refresh rate.
 *
 * After FRAME_PACER_VSYNC_WARMUP samples, the measured Hz becomes stable.
 * If measured Hz differs significantly from reported Hz, the pacer
 * automatically reinitializes with the measured value.
 *
 * @param pacer Pacer state to update
 */
void FramePacer_recordVsync(FramePacer* pacer);

/**
 * Get measured display Hz.
 *
 * Returns the measured refresh rate based on vsync timing.
 * Before enough samples are collected, returns 0.0.
 *
 * @param pacer Pacer state
 * @return Measured Hz, or 0.0 if not yet measured
 */
double FramePacer_getMeasuredHz(const FramePacer* pacer);

/**
 * Check if vsync measurement is stable.
 *
 * @param pacer Pacer state
 * @return true if enough samples collected for stable measurement
 */
bool FramePacer_isMeasurementStable(const FramePacer* pacer);

#endif // __FRAME_PACER_H__
