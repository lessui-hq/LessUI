#include "animated_value.h"
#include <string.h>

// Ease-out cubic: decelerates smoothly
static float ease_out_cubic(float t) {
	float t1 = t - 1.0f;
	return t1 * t1 * t1 + 1.0f;
}

void animated_value_set(AnimatedValue* v, float target, int duration_ms) {
	if (duration_ms <= 0) {
		// Instant jump
		v->current = target;
		v->target = target;
		v->start = target;
		v->animating = false;
		return;
	}

	// Start animation from current position
	v->start = v->current;
	v->target = target;
	v->duration_ms = duration_ms;
	v->animating = true;
	gettimeofday(&v->start_time, NULL);
}

void animated_value_set_immediate(AnimatedValue* v, float value) {
	v->current = value;
	v->target = value;
	v->start = value;
	v->animating = false;
}

float animated_value_get(AnimatedValue* v) {
	if (!v->animating) {
		return v->current;
	}

	struct timeval now;
	gettimeofday(&now, NULL);

	long elapsed_ms = (now.tv_sec - v->start_time.tv_sec) * 1000 +
	                  (now.tv_usec - v->start_time.tv_usec) / 1000;

	if (elapsed_ms >= v->duration_ms) {
		// Animation complete
		v->current = v->target;
		v->animating = false;
		return v->current;
	}

	// Calculate progress with easing
	float t = (float)elapsed_ms / (float)v->duration_ms;
	float eased = ease_out_cubic(t);

	v->current = v->start + (v->target - v->start) * eased;
	return v->current;
}

bool animated_value_is_animating(AnimatedValue* v) {
	return v->animating;
}

void animated_value_reset(AnimatedValue* v) {
	memset(v, 0, sizeof(AnimatedValue));
}
