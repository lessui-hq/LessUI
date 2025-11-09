/**
 * platform_mocks.c - Mock platform function implementations
 *
 * Provides controllable mock implementations of PLAT_* functions for testing.
 * These override the real platform implementations via link-time substitution.
 */

#include "platform_mocks.h"
#include "sdl_fakes.h"
#include <string.h>

///////////////////////////////
// Mock State
///////////////////////////////

static struct {
	// Battery state
	int battery_charge;
	int is_charging;

	// Event queue
	SDL_Event queued_events[10];
	int event_count;
	int event_index;

	// Video state
	int video_width;
	int video_height;
} mock_state = {
    .battery_charge = 100,
    .is_charging = 0,
    .event_count = 0,
    .event_index = 0,
    .video_width = 640,
    .video_height = 480,
};

///////////////////////////////
// Mock Control Functions
///////////////////////////////

void mock_set_battery(int charge, int charging) {
	mock_state.battery_charge = charge;
	mock_state.is_charging = charging;
}

void mock_queue_event(SDL_Event* event) {
	if (mock_state.event_count < 10) {
		memcpy(&mock_state.queued_events[mock_state.event_count], event, sizeof(SDL_Event));
		mock_state.event_count++;
	}
}

void mock_reset_all(void) {
	mock_state.battery_charge = 100;
	mock_state.is_charging = 0;
	mock_state.event_count = 0;
	mock_state.event_index = 0;
	mock_state.video_width = 640;
	mock_state.video_height = 480;

	// Reset fff fakes
	RESET_FAKE(SDL_PollEvent);
	FFF_RESET_HISTORY();
}

///////////////////////////////
// PLAT_* Mock Implementations
// (These override real platform functions at link time)
///////////////////////////////

void PLAT_initInput(void) {
	// No-op for tests
}

void PLAT_quitInput(void) {
	// No-op for tests
}

void PLAT_pollInput(void) {
	// Process queued events by calling the SDL_PollEvent fake
	// The fake's behavior is controlled by tests
	if (mock_state.event_index < mock_state.event_count) {
		SDL_Event* event = &mock_state.queued_events[mock_state.event_index];
		mock_state.event_index++;

		// Configure SDL_PollEvent to return this event
		SDL_PollEvent_fake.return_val = 1; // Has event

		// The actual SDL_PollEvent fake will copy event data
		// This is handled in the test setup
	} else {
		SDL_PollEvent_fake.return_val = 0; // No more events
	}
}

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = mock_state.is_charging;
	*charge = mock_state.battery_charge;
}

// Minimal no-op implementations for other PLAT functions
void PLAT_initVideo(void) {}
void PLAT_quitVideo(void) {}
void PLAT_clearVideo(SDL_Surface* screen) {}
void PLAT_flip(SDL_Surface* screen, int sync) {}
void PLAT_vsync(int remaining) {}
void PLAT_setCPUSpeed(int speed) {}
void PLAT_setRumble(int strength) {}
void PLAT_enableBacklight(int enable) {}
