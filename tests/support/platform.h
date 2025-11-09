// Stub platform.h for testing
// This file is included by workspace/all/common/defines.h

#ifndef PLATFORM_H
#define PLATFORM_H

// Platform identification
#define PLATFORM "test"

// Build information (for linting/testing)
#define BUILD_DATE "2025-01-01"
#define BUILD_HASH "test-build"

// Path definitions
#define SDCARD_PATH "/tmp/test"

// Display configuration
#define FIXED_SCALE 1
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define FIXED_PITCH 640
#define FIXED_SIZE 307200

// Button definitions (required by defines.h)
#define BUTTON_NA -1
#define CODE_NA -1
#define JOY_NA -1

#define BUTTON_POWER BUTTON_NA
#define CODE_POWER CODE_NA
#define JOY_POWER JOY_NA

#define BUTTON_MENU BUTTON_NA
#define CODE_MENU CODE_NA
#define JOY_MENU JOY_NA

// Modifier button definitions (required by pad.c)
// Set these to BTN_NONE to indicate no modifier button configuration
#define BTN_MOD_VOLUME BTN_NONE
#define BTN_MOD_BRIGHTNESS BTN_NONE
#define BTN_MOD_PLUS BTN_NONE
#define BTN_MOD_MINUS BTN_NONE

#endif
