/**
 * test_helpers.h - Central test setup and teardown utilities
 *
 * Provides standardized reset functions to prevent test pollution.
 * Call these in setUp()/tearDown() to ensure tests are isolated.
 *
 * Usage:
 *   void setUp(void) {
 *       test_reset_all();  // Reset all test state
 *   }
 *
 *   void tearDown(void) {
 *       test_cleanup_all();  // Clean up any resources
 *   }
 *
 * Or selectively:
 *   void setUp(void) {
 *       test_reset_sdl_fakes();   // Only reset SDL mocks
 *       test_reset_fs_mocks();    // Only reset file system mocks
 *   }
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

///////////////////////////////
// Complete Reset Functions
///////////////////////////////

/**
 * Reset all test state to initial values.
 * Call this in setUp() before each test.
 *
 * Resets:
 * - SDL fakes (if linked)
 * - File system mocks (if linked)
 * - fff history
 */
void test_reset_all(void);

/**
 * Clean up all test resources.
 * Call this in tearDown() after each test.
 *
 * Cleans:
 * - Temp files created via test_temp.h
 * - Any allocated mock resources
 */
void test_cleanup_all(void);

///////////////////////////////
// Selective Reset Functions
///////////////////////////////

/**
 * Reset SDL fakes to initial state.
 * Wrapper around reset_all_sdl_fakes() from sdl_fakes.h.
 *
 * Call this if your test uses SDL function mocks.
 */
void test_reset_sdl_fakes(void);

/**
 * Reset file system mocks to empty state.
 * Wrapper around mock_fs_reset() from fs_mocks.h.
 *
 * Call this if your test uses file mocking (--wrap).
 */
void test_reset_fs_mocks(void);

/**
 * Reset fff call history.
 * Clears the global call sequence history in fff.
 */
void test_reset_fff_history(void);

///////////////////////////////
// Test Lifecycle Macros
///////////////////////////////

/**
 * Standard setUp/tearDown implementations.
 *
 * Use these macros to get default behavior without writing functions.
 * Tests can still override setUp/tearDown for custom behavior.
 *
 * These handle both mock resets AND temp file cleanup, so they work
 * for all test types.
 *
 * Example (in test file):
 *   TEST_SETUP_DEFAULT;     // Expands to setUp() that calls test_reset_all()
 *   TEST_TEARDOWN_DEFAULT;  // Expands to tearDown() that calls test_cleanup_all()
 */
#define TEST_SETUP_DEFAULT                                                                         \
	void setUp(void) {                                                                             \
		test_reset_all();                                                                          \
	}

#define TEST_TEARDOWN_DEFAULT                                                                      \
	void tearDown(void) {                                                                          \
		test_cleanup_all();                                                                        \
	}

#endif // TEST_HELPERS_H
