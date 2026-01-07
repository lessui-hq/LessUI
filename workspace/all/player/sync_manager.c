/**
 * sync_manager.c - Audio/video synchronization mode management implementation
 */

#include "sync_manager.h"
#include "log.h"
#include "utils.h" // getMicroseconds
#include <math.h>

// Number of vsync samples before measurement is considered stable
// 120 samples (~2s at 60Hz): Long enough for EMA to converge and filter
// initial jitter, short enough that users don't notice startup delay
#define SYNC_WARMUP_SAMPLES 120

// Check for drift every 300 frames (~5 seconds at 60fps)
// Balance between responsiveness to actual drift and avoiding false positives
// from temporary frame drops or CPU frequency transitions
#define SYNC_DRIFT_CHECK_INTERVAL 300

// Tolerance for mode selection (1% mismatch)
// Based on RetroArch research (Arntzen, 2012):
// - Audio pitch changes ≤0.5% are inaudible to most listeners
// - Beyond ~1% mismatch, frame pacing or audio-clock should be used
// Using 1% as a conservative threshold for mode switching
#define SYNC_MODE_TOLERANCE 0.01

// Exponential moving average smoothing factor
// α=0.01 gives ~100-sample half-life: filters frame-drop spikes while
// still tracking genuine Hz drift within ~2 seconds. Lower would be more
// stable but slower to detect drift; higher would be noisier.
#define SYNC_EMA_ALPHA 0.01

// Outlier rejection bounds (50-120 Hz)
#define SYNC_MIN_HZ 50.0
#define SYNC_MAX_HZ 120.0

void SyncManager_init(SyncManager* manager, double game_fps, double display_hz) {
	// Start in AUDIO_CLOCK mode (safe default)
	manager->mode = SYNC_MODE_AUDIO_CLOCK;
	manager->game_fps = game_fps;
	manager->display_hz = (display_hz > 0.0) ? display_hz : 60.0;
	manager->measured_hz = 0.0;
	manager->measurement_samples = 0;
	manager->measurement_stable = false;
	manager->last_drift_check = 0;
	manager->last_vsync_time = 0;

	LOG_info("Sync: Starting in %s mode (%.2ffps @ %.1fHz reported)",
	         SyncManager_getModeName(manager->mode), manager->game_fps, manager->display_hz);
}

void SyncManager_recordVsync(SyncManager* manager) {
	uint64_t now = getMicroseconds();

	// First call - just record timestamp
	if (manager->last_vsync_time == 0) {
		manager->last_vsync_time = now;
		return;
	}

	// Calculate interval and Hz
	double interval = (double)(now - manager->last_vsync_time) / 1000000.0;

	// Protect against division by zero (identical timestamps)
	if (interval <= 0.0) {
		manager->last_vsync_time = now;
		return;
	}

	double hz = 1.0 / interval;

	// Reject outliers (frame drops, fast presents)
	if (hz < SYNC_MIN_HZ || hz > SYNC_MAX_HZ) {
		manager->last_vsync_time = now;
		return;
	}

	// Update measured Hz using exponential moving average
	if (manager->measured_hz == 0.0) {
		manager->measured_hz = hz; // First valid sample
	} else {
		manager->measured_hz = manager->measured_hz * (1.0 - SYNC_EMA_ALPHA) + hz * SYNC_EMA_ALPHA;
	}

	manager->measurement_samples++;
	manager->last_vsync_time = now;

	// Check if measurement just became stable
	if (!manager->measurement_stable && manager->measurement_samples >= SYNC_WARMUP_SAMPLES) {
		manager->measurement_stable = true;

		LOG_info(
		    "Sync: Measurement stable after %d samples: %.3fHz (reported: %.1fHz, diff: %.2f%%)",
		    manager->measurement_samples, manager->measured_hz, manager->display_hz,
		    fabs(manager->measured_hz - manager->display_hz) / manager->display_hz * 100.0);

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
	}

	// Monitor for drift in vsync mode (check every 5 seconds)
	if (manager->measurement_stable && manager->mode == SYNC_MODE_VSYNC) {
		manager->last_drift_check++;

		if (manager->last_drift_check >= SYNC_DRIFT_CHECK_INTERVAL) {
			manager->last_drift_check = 0;

			// Check if measured Hz has drifted beyond tolerance
			double mismatch = fabs(manager->measured_hz - manager->game_fps) / manager->game_fps;
			if (mismatch >= SYNC_MODE_TOLERANCE) {
				LOG_info("Sync: Drift detected! %.3fHz now differs by %.2f%% from %.2ffps",
				         manager->measured_hz, mismatch * 100.0, manager->game_fps);
				LOG_info("Sync: Switching to %s mode (fallback for unstable display)",
				         SyncManager_getModeName(SYNC_MODE_AUDIO_CLOCK));
				manager->mode = SYNC_MODE_AUDIO_CLOCK;
			}
		}
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
	// Only use rate control in vsync mode
	// Audio clock mode uses blocking writes for timing
	return manager->mode == SYNC_MODE_VSYNC;
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
