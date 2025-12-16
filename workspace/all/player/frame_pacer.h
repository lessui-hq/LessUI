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
 * - RetroArch's audio rate control can compensate for ±2% drift
 * - Speed changes ≤2% are imperceptible in gameplay
 *
 * Using 2% allows direct mode (no frame pacing overhead) when the mismatch
 * is small enough for audio rate control to handle without perceptible
 * pitch shift. Larger mismatches (e.g., 60fps @ 72Hz = 20%) use Bresenham
 * pacing to maintain correct speed with frame repeats.
 *
 * Examples at 2% tolerance:
 * - 59.94fps @ 60Hz → direct mode (0.1% diff) ✓
 * - 60.0fps @ 61Hz → direct mode (1.6% diff) ✓
 * - 50.0fps @ 60Hz → paced mode (16.7% diff)
 * - 60.0fps @ 72Hz → paced mode (16.7% diff)
 */
#define FRAME_PACER_TOLERANCE 0.02

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

#endif // __FRAME_PACER_H__
