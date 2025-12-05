#ifndef SHELLUI_UI_MESSAGE_H
#define SHELLUI_UI_MESSAGE_H

#include "common.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

// Message display options
typedef struct {
	char* text;              // Message text (supports \n for newlines)
	int timeout;             // Seconds before auto-dismiss (-1 = forever)
	char* background_color;  // Hex color like "#FF0000"
	char* background_image;  // Path to image file
	char* confirm_text;      // Confirm button label (NULL to hide)
	char* cancel_text;       // Cancel button label (NULL to hide)
	bool show_pill;          // Draw pill background around text
	bool show_time_left;     // Show countdown timer
} MessageOptions;

// Initialize message UI resources (fonts)
void ui_message_init(void);

// Clean up message UI resources
void ui_message_cleanup(void);

// Show a message dialog
// Returns exit code based on user action
ExitCode ui_message_show(SDL_Surface* screen, const MessageOptions* opts);

#endif // SHELLUI_UI_MESSAGE_H
