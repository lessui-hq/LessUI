/**
 * @file rate_meter.c
 * @brief Unified rate measurement implementation.
 *
 * See rate_meter.h for API documentation.
 */

#include "rate_meter.h"
#include <string.h>

void RateMeter_init(RateMeter* meter, int window_size, float stability_threshold) {
	if (!meter)
		return;

	memset(meter, 0, sizeof(RateMeter));

	// Clamp window size
	if (window_size > RATE_METER_MAX_WINDOW)
		window_size = RATE_METER_MAX_WINDOW;
	if (window_size < 1)
		window_size = 1;

	meter->window_size = window_size;
	meter->stability_threshold = stability_threshold;
}

void RateMeter_addSample(RateMeter* meter, float hz) {
	if (!meter || hz <= 0)
		return;

	// Add to ring buffer
	meter->buffer[meter->index] = hz;
	meter->index = (meter->index + 1) % meter->window_size;
	if (meter->count < meter->window_size)
		meter->count++;

	// Need at least a few samples for meaningful stats
	if (meter->count < 3)
		return;

	// Sort copy to find median and min/max
	float sorted[RATE_METER_MAX_WINDOW] = {0};
	for (int i = 0; i < meter->count; i++)
		sorted[i] = meter->buffer[i];

	// Simple insertion sort for small arrays (faster than qsort for n < ~20)
	for (int i = 1; i < meter->count; i++) {
		float key = sorted[i];
		int j = i - 1;
		while (j >= 0 && sorted[j] > key) {
			sorted[j + 1] = sorted[j];
			j--;
		}
		sorted[j + 1] = key;
	}

	// Update statistics
	meter->min = sorted[0];
	meter->max = sorted[meter->count - 1];
	// Median: for even counts, uses lower middle value (e.g., sorted[5] for count=10)
	// This is simpler than averaging two middle values and difference is negligible for rate measurement
	meter->median = sorted[meter->count / 2];
	float spread = meter->max - meter->min;

	// Check stability and update locked value
	if (meter->count >= meter->window_size && spread < meter->stability_threshold) {
		// If not yet stable, or if this is MORE stable than before, update the locked value
		if (!meter->stable || spread < meter->locked_spread) {
			meter->stable = 1;
			meter->locked_value = meter->median;
			meter->locked_spread = spread;
		}
	}
}

float RateMeter_getRate(const RateMeter* meter) {
	if (!meter || meter->count < 3)
		return 0.0f;
	// Only return a value if we've achieved stability at least once
	// This prevents using garbage data when readings are too noisy
	if (meter->stable)
		return meter->locked_value;
	return 0.0f; // Not stable yet - caller should use default
}

float RateMeter_getSwing(const RateMeter* meter) {
	if (!meter || meter->count < 3)
		return 0.0f;
	return meter->max - meter->min;
}

int RateMeter_isStable(const RateMeter* meter) {
	if (!meter)
		return 0;
	return meter->stable;
}

int RateMeter_getSampleCount(const RateMeter* meter) {
	if (!meter)
		return 0;
	return meter->count;
}
