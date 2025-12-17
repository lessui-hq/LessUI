/**
 * test_helpers.c - Central test setup and teardown implementations
 *
 * Provides weak symbol implementations that can be overridden when
 * specific mock systems are linked. This allows test_reset_all() to
 * work regardless of which mocking systems are used.
 */

#include "test_helpers.h"
#include "test_temp.h"

// fff is header-only - FFF_H is defined when included by test files
// We use it to conditionally compile fff-specific reset code

///////////////////////////////
// Weak Symbol Declarations
//
// These are provided as weak symbols so they can be overridden
// when the actual mock implementations are linked.
///////////////////////////////

// Weak stub for SDL fake reset (overridden by sdl_fakes.c)
__attribute__((weak)) void reset_all_sdl_fakes(void) {
	// No-op when SDL fakes not linked
}

// Weak stub for FS mock reset (overridden by fs_mocks.c)
__attribute__((weak)) void mock_fs_reset(void) {
	// No-op when FS mocks not linked
}

///////////////////////////////
// Reset Function Implementations
///////////////////////////////

void test_reset_sdl_fakes(void) {
	reset_all_sdl_fakes();
}

void test_reset_fs_mocks(void) {
	mock_fs_reset();
}

void test_reset_fff_history(void) {
#ifdef FFF_H
	FFF_RESET_HISTORY();
#endif
}

void test_reset_all(void) {
	// Reset all mock systems
	test_reset_sdl_fakes();
	test_reset_fs_mocks();
	test_reset_fff_history();
}

void test_cleanup_all(void) {
	// Clean up temp files
	test_temp_cleanup();
}
