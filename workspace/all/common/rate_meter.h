/**
 * @file rate_meter.h
 * @brief Unified rate measurement for display and audio clock tracking.
 *
 * RateMeter provides a common algorithm for measuring refresh/sample rates
 * with continuous refinement. Both display and audio measurements use the
 * same structure with different configuration constants.
 *
 * Features:
 * - Ring buffer of Hz samples with running median
 * - Min/max tracking for swing detection (used for buffer sizing)
 * - Stability detection based on spread threshold
 * - Continuous refinement (values improve over time)
 *
 * See docs/audio-rate-control.md for algorithm details.
 */

#ifndef RATE_METER_H
#define RATE_METER_H

#include <stdint.h>

// Configuration constants (separate for display vs audio)
#define RATE_METER_DISPLAY_WINDOW 30 // Samples for display rate (~0.5 sec at 60fps)
#define RATE_METER_AUDIO_WINDOW 10 // Samples for audio rate (fewer needed with longer intervals)
#define RATE_METER_DISPLAY_STABILITY 1.0f // Hz spread threshold for stability
#define RATE_METER_AUDIO_STABILITY 500.0f // Hz spread threshold (some SDL jitter tolerance)

// Measurement intervals (how often to sample)
#define RATE_METER_DISPLAY_INTERVAL 0.0f // Every frame (no minimum interval)
#define RATE_METER_AUDIO_INTERVAL 2.0f // 2 seconds (average out callback bursts)

// Maximum window size (for static allocation)
#define RATE_METER_MAX_WINDOW 30

/**
 * Rate measurement state.
 *
 * Tracks Hz samples in a ring buffer and maintains running statistics.
 * Use separate instances for display and audio measurement.
 */
typedef struct {
	float buffer[RATE_METER_MAX_WINDOW]; // Ring buffer of Hz samples
	int window_size; // Configured window size
	int index; // Current write position
	int count; // Number of valid samples (0 to window_size)

	// Running statistics (updated on each sample)
	float median; // Best estimate of true rate
	float min; // Minimum observed in window
	float max; // Maximum observed in window

	// Configuration
	float stability_threshold; // Max spread for stable reading

	// State
	int stable; // Has achieved stability (spread < threshold)
	float locked_value; // Best median so far
	float locked_spread; // Spread at time of lock (for improvement detection)
} RateMeter;

/**
 * Initializes a rate meter with specified configuration.
 *
 * @param meter Meter to initialize
 * @param window_size Number of samples to track (max RATE_METER_MAX_WINDOW)
 * @param stability_threshold Max spread (Hz) to consider stable
 */
void RateMeter_init(RateMeter* meter, int window_size, float stability_threshold);

/**
 * Adds a new Hz sample to the meter.
 *
 * Updates the ring buffer and recalculates statistics (median, min, max).
 * Stability flag is set when spread < threshold.
 *
 * @param meter Meter to update
 * @param hz Sample rate in Hz
 */
void RateMeter_addSample(RateMeter* meter, float hz);

/**
 * Gets the current rate estimate (median).
 *
 * @param meter Meter to query
 * @return Median Hz value, or 0 if no samples yet
 */
float RateMeter_getRate(const RateMeter* meter);

/**
 * Gets the current swing (max - min).
 *
 * Used to calculate required buffer headroom.
 *
 * @param meter Meter to query
 * @return Swing in Hz, or 0 if insufficient samples
 */
float RateMeter_getSwing(const RateMeter* meter);

/**
 * Checks if the meter has achieved stability.
 *
 * @param meter Meter to query
 * @return 1 if stable (spread < threshold), 0 otherwise
 */
int RateMeter_isStable(const RateMeter* meter);

/**
 * Gets the number of samples collected.
 *
 * @param meter Meter to query
 * @return Sample count (0 to window_size)
 */
int RateMeter_getSampleCount(const RateMeter* meter);

#endif // RATE_METER_H
