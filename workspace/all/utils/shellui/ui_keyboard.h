#ifndef SHELLUI_UI_KEYBOARD_H
#define SHELLUI_UI_KEYBOARD_H

#include "common.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

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

// Initialize keyboard UI resources
void ui_keyboard_init(void);

// Clean up keyboard UI resources
void ui_keyboard_cleanup(void);

// Show keyboard input dialog
KeyboardResult ui_keyboard_show(SDL_Surface* screen, const KeyboardOptions* opts);

#endif // SHELLUI_UI_KEYBOARD_H
