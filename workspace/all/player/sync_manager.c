/**
 * sync_manager.c - Audio/video synchronization mode management implementation
 */

#include "sync_manager.h"
#include "log.h"
#include "utils.h" // getMicroseconds
#include <math.h>
#include <string.h>

// Minimum samples before checking stability
// 60 samples (~1s at 60Hz): Need enough for meaningful stddev
#define SYNC_MIN_SAMPLES 60

// Maximum samples before giving up on convergence
// 1800 samples (~30s at 60Hz): If not stable by then, display is unstable
#define SYNC_MAX_SAMPLES 1800

// Stability threshold (stddev/mean ratio)
// 1% relative deviation indicates stable measurement
#define SYNC_STABILITY_THRESHOLD 0.01

// Progress logging interval (DEBUG only)
// Log every 60 samples to show convergence progress
#define SYNC_LOG_INTERVAL 60

// Check for drift every 300 frames (~5 seconds at 60fps)
#define SYNC_DRIFT_CHECK_INTERVAL 300

// Tolerance for mode selection (1% mismatch)
#define SYNC_MODE_TOLERANCE 0.01

// Outlier rejection bounds (50-120 Hz)
#define SYNC_MIN_HZ 50.0
#define SYNC_MAX_HZ 120.0

void SyncManager_init(SyncManager* manager, double game_fps, double display_hz) {
	memset(manager, 0, sizeof(SyncManager));

	// Start in AUDIO_CLOCK mode (safe default)
	manager->mode = SYNC_MODE_AUDIO_CLOCK;
	manager->game_fps = game_fps;
	manager->display_hz = (display_hz > 0.0) ? display_hz : 60.0;

	LOG_info("Sync: Starting in %s mode (%.2ffps @ %.1fHz reported)",
	         SyncManager_getModeName(manager->mode), manager->game_fps, manager->display_hz);
	LOG_info("Sync: Measuring vsync timing...");
}

void SyncManager_recordVsync(SyncManager* manager) {
	uint64_t now = getMicroseconds();

	// First call - just record timestamp
	if (manager->last_vsync_time == 0) {
		manager->last_vsync_time = now;
		return;
	}

	// Calculate frame interval
	uint64_t interval = now - manager->last_vsync_time;
	manager->last_vsync_time = now;

	// Reject zero intervals (duplicate timestamps)
	if (interval == 0) {
		return;
	}

	// Reject outliers based on Hz (frame drops, fast presents)
	double hz = 1000000.0 / (double)interval;
	if (hz < SYNC_MIN_HZ || hz > SYNC_MAX_HZ) {
		return;
	}

	// Store interval in circular buffer
	manager->frame_intervals[manager->write_index] = interval;
	manager->write_index = (manager->write_index + 1) % SYNC_SAMPLE_BUFFER_SIZE;
	manager->sample_count++;

	// Skip measurement logic if already stable
	if (manager->measurement_stable) {
		// Monitor for drift in vsync mode
		if (manager->mode == SYNC_MODE_VSYNC) {
			manager->last_drift_check++;

			if (manager->last_drift_check >= SYNC_DRIFT_CHECK_INTERVAL) {
				manager->last_drift_check = 0;

				// Recalculate current Hz from buffer
				int samples = (manager->sample_count < SYNC_SAMPLE_BUFFER_SIZE)
				                  ? manager->sample_count
				                  : SYNC_SAMPLE_BUFFER_SIZE;
				uint64_t sum = 0;
				for (int i = 0; i < samples; i++) {
					sum += manager->frame_intervals[i];
				}
				double mean = (double)sum / samples;
				double current_hz = 1000000.0 / mean;

				// Check if drifted beyond tolerance
				double mismatch = fabs(current_hz - manager->game_fps) / manager->game_fps;
				if (mismatch >= SYNC_MODE_TOLERANCE) {
					LOG_info("Sync: Drift detected! %.3fHz now differs by %.2f%% from %.2ffps",
					         current_hz, mismatch * 100.0, manager->game_fps);
					LOG_info("Sync: Switching to %s mode (fallback for unstable display)",
					         SyncManager_getModeName(SYNC_MODE_AUDIO_CLOCK));
					manager->mode = SYNC_MODE_AUDIO_CLOCK;
				}
			}
		}
		return;
	}

	// Check for convergence (need minimum samples first)
	if (manager->sample_count < SYNC_MIN_SAMPLES) {
		return;
	}

	// Calculate statistics from circular buffer
	int samples = (manager->sample_count < SYNC_SAMPLE_BUFFER_SIZE) ? manager->sample_count
	                                                                : SYNC_SAMPLE_BUFFER_SIZE;

	// Calculate mean
	uint64_t sum = 0;
	for (int i = 0; i < samples; i++) {
		sum += manager->frame_intervals[i];
	}
	double mean = (double)sum / samples;

	// Calculate standard deviation
	double variance_sum = 0.0;
	for (int i = 0; i < samples; i++) {
		double diff = (double)manager->frame_intervals[i] - mean;
		variance_sum += diff * diff;
	}
	double stddev = sqrt(variance_sum / (samples - 1));

	// Calculate confidence (relative stddev)
	double confidence = stddev / mean;
	double measured_hz = 1000000.0 / mean;

	// Progress logging (DEBUG only)
	if (manager->sample_count % SYNC_LOG_INTERVAL == 0) {
		LOG_debug("Sync: %d samples, mean=%.3fHz, confidence=%.3f%% (%s)", manager->sample_count,
		          measured_hz, confidence * 100.0,
		          confidence < SYNC_STABILITY_THRESHOLD ? "STABLE" : "measuring...");
	}

	// Check for stability
	if (confidence < SYNC_STABILITY_THRESHOLD) {
		// Measurement converged!
		manager->measurement_stable = true;
		manager->measured_hz = measured_hz;
		manager->measurement_confidence = confidence;

		LOG_info("Sync: Measurement stable after %d samples: %.3fHz ± %.2f%%",
		         manager->sample_count, manager->measured_hz,
		         manager->measurement_confidence * 100.0);

		// Try switching to vsync mode if compatible
		double mismatch = fabs(manager->measured_hz - manager->game_fps) / manager->game_fps;
		if (mismatch < SYNC_MODE_TOLERANCE) {
			manager->mode = SYNC_MODE_VSYNC;
			LOG_info("Sync: Switching to %s mode (%.3fHz within 1%% of %.2ffps)",
			         SyncManager_getModeName(manager->mode), manager->measured_hz,
			         manager->game_fps);
		} else {
			LOG_info("Sync: Staying in %s mode (%.3fHz differs by %.2f%% from %.2ffps)",
			         SyncManager_getModeName(manager->mode), manager->measured_hz, mismatch * 100.0,
			         manager->game_fps);
		}

		return;
	}

	// Timeout: give up if not stable after max samples
	if (manager->sample_count >= SYNC_MAX_SAMPLES) {
		manager->measurement_stable = true; // Stop trying
		manager->measured_hz = measured_hz;
		manager->measurement_confidence = confidence;

		LOG_info(
		    "Sync: Measurement unstable after %d samples (confidence %.2f%% > 1%%), staying in %s "
		    "mode",
		    manager->sample_count, confidence * 100.0, SyncManager_getModeName(manager->mode));
	}
}

bool SyncManager_shouldRunCore(const SyncManager* manager) {
	// Always run core every frame in both modes
	// AUDIO_CLOCK: blocking audio provides timing
	// VSYNC: vsync provides timing
	return true;
}

SyncMode SyncManager_getMode(const SyncManager* manager) {
	return manager->mode;
}

const char* SyncManager_getModeName(SyncMode mode) {
	switch (mode) {
	case SYNC_MODE_AUDIO_CLOCK:
		return "Audio Clock";
	case SYNC_MODE_VSYNC:
		return "Vsync";
	default:
		return "Unknown";
	}
}

bool SyncManager_shouldUseRateControl(const SyncManager* manager) {
	// Both modes use rate control (±0.8%) as buffer health mechanism
	// This handles timing variations when true blocking can't provide pacing
	(void)manager;
	return true;
}

bool SyncManager_shouldBlockAudio(const SyncManager* manager) {
	// Only block audio in audio clock mode
	// Vsync mode uses non-blocking writes with rate control
	return manager->mode == SYNC_MODE_AUDIO_CLOCK;
}

double SyncManager_getMeasuredHz(const SyncManager* manager) {
	return manager->measurement_stable ? manager->measured_hz : 0.0;
}

bool SyncManager_isMeasurementStable(const SyncManager* manager) {
	return manager->measurement_stable;
}
