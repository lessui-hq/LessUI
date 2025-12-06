#ifndef SHUI_UI_PROGRESS_H
#define SHUI_UI_PROGRESS_H

#include "common.h"
#include "animated_value.h"
#include "sdl.h"

// Animation duration for value transitions (ms)
#define PROGRESS_ANIMATION_MS 200

// Progress display options (per-call parameters)
typedef struct {
	char* message;        // Message to display
	char* title;          // Optional title above progress (also used as context key)
	char* subtext;        // Secondary text below message (smaller, gray)
	int value;            // Progress percentage 0-100
	bool indeterminate;   // Show spinner instead of progress bar
} ProgressOptions;

// Progress state (persists between calls)
typedef struct {
	char* context_title;   // Context key part 1 (copy of title)
	char* context_message; // Context key part 2 (copy of message)
	AnimatedValue value;   // Animated progress value
	bool indeterminate;    // Current mode
	bool active;           // State is valid/active
} ProgressState;

/**
 * Update progress state based on new options.
 * Handles context matching and animation setup.
 *
 * @param state  Persistent state (managed by daemon)
 * @param opts   New options from current command
 */
void ui_progress_update(ProgressState* state, const ProgressOptions* opts);

/**
 * Render progress UI using current state.
 * Call this each frame when progress is active.
 *
 * @param screen  SDL surface to render to
 * @param state   Current progress state
 * @param opts    Current options (for message text)
 */
void ui_progress_render(SDL_Surface* screen, ProgressState* state, const ProgressOptions* opts);

/**
 * Check if progress needs animation (indeterminate or value transitioning).
 */
bool ui_progress_needs_animation(ProgressState* state);

/**
 * Reset progress state (e.g., when switching to different UI).
 */
void ui_progress_reset(ProgressState* state);

#endif // SHUI_UI_PROGRESS_H
