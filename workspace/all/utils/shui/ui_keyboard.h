#ifndef SHUI_UI_KEYBOARD_H
#define SHUI_UI_KEYBOARD_H

#include "common.h"
#include "sdl.h"

// Keyboard display options
typedef struct {
	char* title;          // Prompt title
	char* initial_value;  // Initial text value
} KeyboardOptions;

// Result from keyboard input
typedef struct {
	ExitCode exit_code;
	char* text;           // Entered text (caller must free)
} KeyboardResult;

// Show keyboard input dialog
KeyboardResult ui_keyboard_show(SDL_Surface* screen, const KeyboardOptions* opts);

#endif // SHUI_UI_KEYBOARD_H
