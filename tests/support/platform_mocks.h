/**
 * platform_mocks.h - Mock platform functions for testing
 *
 * Provides mock implementations of PLAT_* functions that can be controlled
 * from tests. Uses link-time substitution to replace real platform code.
 *
 * Mock State Control Functions:
 * - mock_set_battery() - Set battery charge and charging state
 * - mock_set_video_size() - Configure mock video surface dimensions
 * - mock_queue_event() - Queue an SDL event for next pollInput call
 * - mock_reset_all() - Reset all mock state to defaults
 *
 * Usage:
 *   mock_set_battery(75, 1);  // 75% charge, charging
 *   mock_queue_event(&event);  // Queue keyboard event
 *   PLAT_pollInput();  // Will process queued event
 */

#ifndef PLATFORM_MOCKS_H
#define PLATFORM_MOCKS_H

#include "sdl_stubs.h"

///////////////////////////////
// Mock State Control
///////////////////////////////

/**
 * Sets the mock battery state returned by PLAT_getBatteryStatus()
 *
 * @param charge Battery charge percentage (0-100)
 * @param charging 1 if charging, 0 if not charging
 */
void mock_set_battery(int charge, int charging);

/**
 * Queues an SDL event to be returned by the next PLAT_pollInput() call
 *
 * @param event Event to queue (will be copied internally)
 */
void mock_queue_event(SDL_Event* event);

/**
 * Resets all mock state to default values
 *
 * Default state:
 * - Battery: 100%, not charging
 * - No queued events
 * - Video: 640x480
 */
void mock_reset_all(void);

///////////////////////////////
// Mock PLAT_* implementations
// (Defined in platform_mocks.c, replace real platform code via linking)
///////////////////////////////

// These are declared here for documentation but implemented in platform_mocks.c
// They override the real PLAT_* functions at link time.

// void PLAT_initInput(void);
// void PLAT_quitInput(void);
// void PLAT_pollInput(void);
// void PLAT_getBatteryStatus(int* is_charging, int* charge);

#endif // PLATFORM_MOCKS_H
