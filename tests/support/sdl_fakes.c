/**
 * sdl_fakes.c - SDL function fake implementations
 *
 * Defines the actual fake function instances using fff macros.
 * This file must be included/compiled exactly once in the test suite.
 */

#include "sdl_fakes.h"

// Define fff globals (must be done once per test executable)
DEFINE_FFF_GLOBALS;

///////////////////////////////
// Event System Fake Definitions
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(int, SDL_PollEvent, SDL_Event*);

///////////////////////////////
// TTF Fake Definitions - Phase 4
///////////////////////////////

DEFINE_FAKE_VALUE_FUNC(int, TTF_SizeUTF8, TTF_Font*, const char*, int*, int*);

///////////////////////////////
// Surface Management Fake Definitions (future)
///////////////////////////////

// Will be added later if needed

///////////////////////////////
// Audio Fake Definitions (future)
///////////////////////////////

// Will be added in Phase 5
