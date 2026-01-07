/**
 * sync_manager.h - Audio/video synchronization mode management
 *
 * Manages runtime switching between audio-clock and vsync timing modes.
 *
 * Strategy:
 * - Start in AUDIO_CLOCK (safe, works on all hardware)
 * - Measure actual display refresh rate via vsync timing
 * - Switch to VSYNC if compatible (< 1% mismatch from game fps)
 * - Monitor for drift, fall back to AUDIO_CLOCK if needed
 *
 * This eliminates the need for:
 * - Frame pacing (Bresenham accumulator) - was problematic at >5% mismatch
 * - Compile-time SYNC_MODE selection - now runtime adaptive
 * - Aggressive audio rate control - only light adjustment in vsync mode
 */

#ifndef __SYNC_MANAGER_H__
#define __SYNC_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Synchronization mode determines timing source.
 */
typedef enum {
	/**
	 * Audio-clock mode: Audio hardware drives timing.
	 *
	 * Core runs every frame, audio writes block when buffer full.
	 * Natural backpressure from blocking maintains timing.
	 * No audio rate control needed.
	 *
	 * Benefits:
	 * - Works with any display refresh rate (no fps/Hz matching needed)
	 * - Frame duplication instead of frame skipping (less visible)
	 * - Audio buffer naturally stable (no rate control oscillation)
	 *
	 * Used when:
	 * - Initial startup (safe default)
	 * - Display Hz mismatch > 1% from game fps
	 * - Display Hz unstable (drift detected)
	 */
	SYNC_MODE_AUDIO_CLOCK,

	/**
	 * Vsync mode: Display vsync drives timing.
	 *
	 * GFX_present() blocks until vsync, providing frame timing.
	 * Core runs every frame (no pacing), light audio rate control
	 * adjusts pitch ±0.5% to maintain buffer at 50%.
	 *
	 * Benefits:
	 * - Minimal input latency (1 frame)
	 * - Perfect frame pacing when fps ≈ Hz
	 * - No frame duplication artifacts
	 *
	 * Used when:
	 * - Display Hz within 1% of game fps
	 * - Display Hz is stable (no drift)
	 */
	SYNC_MODE_VSYNC
} SyncMode;

/**
 * Sync manager state.
 */
typedef struct {
	SyncMode mode; // Current sync mode
	double game_fps; // Game target fps (e.g., 60.0, 59.94)
	double display_hz; // Reported display Hz from SDL
	double measured_hz; // Actual measured Hz from vsync timing
	int measurement_samples; // Number of vsync measurements collected
	bool measurement_stable; // True after enough samples collected
	uint32_t last_drift_check; // Frames since last drift check (resets at interval)
	uint64_t last_vsync_time; // Microsecond timestamp of last vsync
} SyncManager;

/**
 * Initialize sync manager.
 *
 * Starts in AUDIO_CLOCK mode (safe default).
 * Begins vsync measurement in background.
 *
 * @param manager Manager state to initialize
 * @param game_fps Game target fps (e.g., 60.0)
 * @param display_hz Display refresh rate from SDL (e.g., 60.0)
 */
void SyncManager_init(SyncManager* manager, double game_fps, double display_hz);

/**
 * Record vsync timing and update sync mode if needed.
 *
 * Call this immediately after GFX_present() returns.
 * Measures actual display refresh rate and switches modes when appropriate.
 *
 * Mode transitions:
 * - AUDIO_CLOCK → VSYNC: After 120 samples if mismatch < 1%
 * - VSYNC → AUDIO_CLOCK: If drift > 1% detected
 *
 * @param manager Manager state to update
 */
void SyncManager_recordVsync(SyncManager* manager);

/**
 * Check if core should run this frame.
 *
 * AUDIO_CLOCK: Always returns true (core runs every frame)
 * VSYNC: Always returns true (core runs every frame, no pacing)
 *
 * This exists for API consistency and future extensibility.
 *
 * @param manager Manager state
 * @return true if core.run() should be called
 */
bool SyncManager_shouldRunCore(const SyncManager* manager);

/**
 * Get current sync mode.
 *
 * @param manager Manager state
 * @return Current sync mode
 */
SyncMode SyncManager_getMode(const SyncManager* manager);

/**
 * Get mode name for logging/display.
 *
 * @param mode Sync mode
 * @return Human-readable mode name ("Audio Clock" or "Vsync")
 */
const char* SyncManager_getModeName(SyncMode mode);

/**
 * Check if audio rate control should be active.
 *
 * AUDIO_CLOCK: No rate control (blocking writes handle timing)
 * VSYNC: Yes (light rate control for ±0.5% adjustment)
 *
 * @param manager Manager state
 * @return true if audio rate control should run
 */
bool SyncManager_shouldUseRateControl(const SyncManager* manager);

/**
 * Check if audio writes should block.
 *
 * AUDIO_CLOCK: Yes (blocking provides timing backpressure)
 * VSYNC: No (vsync provides timing, audio is just output)
 *
 * @param manager Manager state
 * @return true if SND_batchSamples should block when buffer full
 */
bool SyncManager_shouldBlockAudio(const SyncManager* manager);

/**
 * Get measured display Hz.
 *
 * Returns actual measured Hz after enough samples collected.
 * Before measurement stable, returns 0.0.
 *
 * @param manager Manager state
 * @return Measured Hz, or 0.0 if not yet measured
 */
double SyncManager_getMeasuredHz(const SyncManager* manager);

/**
 * Check if measurement is stable.
 *
 * @param manager Manager state
 * @return true if enough samples collected for reliable measurement
 */
bool SyncManager_isMeasurementStable(const SyncManager* manager);

#endif // __SYNC_MANAGER_H__
