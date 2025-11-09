/**
 * sdl_fakes.h - SDL function fakes using fff framework
 *
 * Declares fake SDL functions for unit testing. Uses the Fake Function
 * Framework (fff) to create mockable versions of SDL functions.
 *
 * Functions are added incrementally as needed for different test suites:
 * - PAD testing: Event polling
 * - GFX testing: Surface management, blitting
 * - SND testing: Audio management
 *
 * Usage:
 *   #include "sdl_fakes.h"
 *   RESET_FAKE(SDL_PollEvent);  // Reset before each test
 *   SDL_PollEvent_fake.return_val = 1;  // Configure mock behavior
 */

#ifndef SDL_FAKES_H
#define SDL_FAKES_H

#include "fff/fff.h"
#include "sdl_stubs.h"

///////////////////////////////
// Event System Fakes
///////////////////////////////

/**
 * Fake for SDL_PollEvent - Used by PAD input system
 *
 * Usage:
 *   SDL_Event mock_event = {.type = SDL_KEYDOWN};
 *   SDL_PollEvent_fake.return_val = 1;  // Has event
 *   // On next call, copy mock_event to the output parameter
 */
DECLARE_FAKE_VALUE_FUNC(int, SDL_PollEvent, SDL_Event*);

///////////////////////////////
// TTF (TrueType Font) Fakes - Phase 4 GFX Testing
///////////////////////////////

/**
 * Fake for TTF_SizeUTF8 - Returns the size of rendered text
 *
 * Usage:
 *   TTF_SizeUTF8_fake.custom_fake = my_ttf_size_mock;
 *   // Custom function can calculate width based on string length
 */
DECLARE_FAKE_VALUE_FUNC(int, TTF_SizeUTF8, TTF_Font*, const char*, int*, int*);

///////////////////////////////
// Surface Management Fakes (for future GFX testing)
///////////////////////////////

// Will be added later if needed:
// DECLARE_FAKE_VALUE_FUNC(SDL_Surface*, SDL_CreateRGBSurface, ...);
// DECLARE_FAKE_VOID_FUNC(SDL_FreeSurface, SDL_Surface*);

///////////////////////////////
// Audio Fakes (for future SND testing)
///////////////////////////////

// Will be added in Phase 5:
// DECLARE_FAKE_VOID_FUNC(SDL_LockAudio);
// DECLARE_FAKE_VOID_FUNC(SDL_UnlockAudio);

#endif // SDL_FAKES_H
