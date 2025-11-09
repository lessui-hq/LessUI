/**
 * sdl_stubs.h - Minimal SDL type definitions for testing
 *
 * Provides stub SDL types needed for unit testing MinUI's api.c functions.
 * Only includes the fields actually used in tests (minimal approach).
 *
 * This is NOT a complete SDL implementation - just enough type structure
 * to allow compilation and testing of platform-independent logic.
 *
 * Do NOT include real SDL headers when using these stubs.
 */

#ifndef SDL_STUBS_H
#define SDL_STUBS_H

#include <stdint.h>

///////////////////////////////
// Basic SDL typedefs
///////////////////////////////

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t Sint16;
typedef int32_t Sint32;

///////////////////////////////
// SDL_PixelFormat
///////////////////////////////

typedef struct SDL_PixelFormat {
	Uint8 BitsPerPixel;
	Uint8 BytesPerPixel;
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;
} SDL_PixelFormat;

///////////////////////////////
// SDL_Surface
///////////////////////////////

typedef struct SDL_Surface {
	Uint32 flags;
	SDL_PixelFormat* format;
	int w;
	int h;
	int pitch;
	void* pixels;
	void* userdata;
	int locked;
	void* lock_data;
	// Clipping rectangle
	struct {
		int x, y, w, h;
	} clip_rect;
	int refcount;
} SDL_Surface;

///////////////////////////////
// SDL_Rect
///////////////////////////////

typedef struct SDL_Rect {
	int x, y;
	int w, h;
} SDL_Rect;

///////////////////////////////
// SDL Event System
///////////////////////////////

typedef enum {
	SDL_KEYDOWN = 0x300,
	SDL_KEYUP = 0x301,
	SDL_QUIT = 0x100,
} SDL_EventType;

typedef enum {
	SDLK_UNKNOWN = 0,
	SDLK_a = 'a',
	SDLK_b = 'b',
	SDLK_ESCAPE = 27,
	// Add more as needed
} SDL_Keycode;

typedef enum {
	SDL_SCANCODE_UNKNOWN = 0,
	SDL_SCANCODE_A = 4,
	SDL_SCANCODE_B = 5,
	SDL_SCANCODE_ESCAPE = 41,
	SDL_SCANCODE_SPACE = 44,
	SDL_SCANCODE_LCTRL = 224,
	SDL_SCANCODE_LSHIFT = 225,
	SDL_SCANCODE_LALT = 226,
	// Add more as needed
} SDL_Scancode;

typedef struct SDL_Keysym {
	SDL_Scancode scancode;
	SDL_Keycode sym;
	Uint16 mod;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint8 state;
	Uint8 repeat;
	SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef union SDL_Event {
	Uint32 type;
	SDL_KeyboardEvent key;
} SDL_Event;

///////////////////////////////
// SDL_ttf (TrueType Font) Types
///////////////////////////////

/**
 * Opaque font structure
 * In real SDL_ttf this is complex, but for testing we just need a placeholder
 */
typedef struct TTF_Font {
	int point_size;
	int style;
} TTF_Font;

#endif // SDL_STUBS_H
