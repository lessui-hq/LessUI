#ifndef SHELLUI_ANIMATED_VALUE_H
#define SHELLUI_ANIMATED_VALUE_H

#include <stdbool.h>
#include <sys/time.h>

/**
 * AnimatedValue - A reusable primitive for smooth value transitions.
 *
 * Usage:
 *   AnimatedValue v = {0};
 *   animated_value_set(&v, 100.0f, 300);  // animate to 100 over 300ms
 *
 *   // In render loop:
 *   float current = animated_value_get(&v);
 *   if (animated_value_is_animating(&v)) {
 *       // keep rendering
 *   }
 */
typedef struct {
	float start;           // Value at animation start
	float target;          // Target value
	float current;         // Current interpolated value
	struct timeval start_time;
	int duration_ms;       // Animation duration
	bool animating;        // Currently animating
} AnimatedValue;

/**
 * Set target value with animation duration.
 * If duration_ms <= 0, jumps instantly.
 */
void animated_value_set(AnimatedValue* v, float target, int duration_ms);

/**
 * Set value instantly without animation.
 */
void animated_value_set_immediate(AnimatedValue* v, float value);

/**
 * Get current value, updating animation progress.
 * Call this each frame during rendering.
 */
float animated_value_get(AnimatedValue* v);

/**
 * Check if currently animating.
 */
bool animated_value_is_animating(AnimatedValue* v);

/**
 * Reset to initial state.
 */
void animated_value_reset(AnimatedValue* v);

#endif // SHELLUI_ANIMATED_VALUE_H
