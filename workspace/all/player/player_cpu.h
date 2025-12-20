/**
 * player_cpu.h - Auto CPU scaling utilities
 *
 * Provides functions for dynamic CPU frequency scaling based on emulation
 * performance. Uses frame timing (core.run() execution time) to determine
 * optimal CPU frequency.
 *
 * Two modes are supported:
 * - Granular mode: Uses all available CPU frequencies (linear scaling)
 * - Fallback mode: Uses 3 fixed levels (powersave/normal/performance)
 *
 * Designed for testability with injectable state and callbacks.
 * Extracted from player.c.
 */

#ifndef __PLAYER_CPU_H__
#define __PLAYER_CPU_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum number of CPU frequencies that can be detected.
 */
#define PLAYER_CPU_MAX_FREQUENCIES 32

/**
 * Ring buffer size for frame timing samples.
 */
#define PLAYER_CPU_FRAME_BUFFER_SIZE 64

/**
 * Default tuning constants.
 * These can be overridden via PlayerCPUConfig.
 */
#define PLAYER_CPU_DEFAULT_WINDOW_FRAMES 30 // ~500ms at 60fps
#define PLAYER_CPU_DEFAULT_UTIL_HIGH 85 // Boost threshold (%)
#define PLAYER_CPU_DEFAULT_UTIL_LOW 55 // Reduce threshold (%)
#define PLAYER_CPU_DEFAULT_BOOST_WINDOWS 2 // Windows before boost (~1s)
#define PLAYER_CPU_DEFAULT_REDUCE_WINDOWS 4 // Windows before reduce (~2s)
#define PLAYER_CPU_DEFAULT_STARTUP_GRACE 300 // Frames to skip (~5s at 60fps)
#define PLAYER_CPU_DEFAULT_MIN_FREQ_KHZ 0 // No minimum (panic failsafe handles problematic freqs)
#define PLAYER_CPU_DEFAULT_TARGET_UTIL 70 // Target utilization after change
#define PLAYER_CPU_DEFAULT_MAX_STEP_DOWN 1 // Max frequency steps when reducing
#define PLAYER_CPU_DEFAULT_PANIC_STEP_UP 2 // Frequency steps on panic (underrun)
#define PLAYER_CPU_PANIC_THRESHOLD 3 // Block frequency after this many panics

/**
 * Preset level indices.
 */
typedef enum {
	PLAYER_CPU_LEVEL_POWERSAVE = 0,
	PLAYER_CPU_LEVEL_NORMAL = 1,
	PLAYER_CPU_LEVEL_PERFORMANCE = 2
} PlayerCPULevel;

/**
 * Decision type returned by PlayerCPU_update().
 */
typedef enum {
	PLAYER_CPU_DECISION_NONE = 0, // No change needed
	PLAYER_CPU_DECISION_BOOST, // Increase frequency/level
	PLAYER_CPU_DECISION_REDUCE, // Decrease frequency/level
	PLAYER_CPU_DECISION_PANIC, // Emergency boost (underrun detected)
	PLAYER_CPU_DECISION_SKIP // Skipped (grace period, menu, etc.)
} PlayerCPUDecision;

/**
 * Configuration constants for auto CPU scaling.
 */
typedef struct {
	int window_frames; // Frames per monitoring window
	unsigned int util_high; // High utilization threshold (%)
	unsigned int util_low; // Low utilization threshold (%)
	int boost_windows; // Consecutive windows before boost
	int reduce_windows; // Consecutive windows before reduce
	int startup_grace; // Grace period frames at startup
	int min_freq_khz; // Minimum frequency to consider (kHz)
	unsigned int target_util; // Target utilization after frequency change
	int max_step_down; // Max frequency steps when reducing
	int panic_step_up; // Frequency steps on panic (underrun)
} PlayerCPUConfig;

/**
 * State for auto CPU scaling.
 * All fields can be inspected for testing.
 */
typedef struct {
	// Frequency array (populated by detectFrequencies)
	int frequencies[PLAYER_CPU_MAX_FREQUENCIES]; // Available frequencies (kHz, sorted low→high)
	int freq_count; // Number of valid frequencies

	// Granular mode state
	int target_index; // Target frequency index (set by algorithm)
	int current_index; // Actually applied frequency index
	int preset_indices[3]; // Preset mappings [POWERSAVE, NORMAL, PERFORMANCE]
	int use_granular; // 1 if granular mode, 0 for 3-level fallback

	// Fallback mode state (3-level)
	int target_level; // Target level (0-2)
	int current_level; // Actually applied level

	// Monitoring state
	int frame_count; // Frames in current window
	int high_util_windows; // Consecutive high-util windows
	int low_util_windows; // Consecutive low-util windows
	unsigned last_underrun; // Last seen underrun count
	int startup_frames; // Frames since start (for grace period)
	int panic_cooldown; // Windows to wait after panic

	// Frame timing data
	uint64_t frame_times[PLAYER_CPU_FRAME_BUFFER_SIZE]; // Ring buffer of frame times (us)
	int frame_time_index; // Current ring buffer position
	uint64_t frame_budget_us; // Target frame time (from fps)

	// Flags for frequency detection
	int frequencies_detected; // 1 if frequencies have been detected
	int scaling_disabled; // 1 if scaling is disabled (0 or 1 frequency available)

	// Per-frequency panic tracking (failsafe for problematic frequencies)
	int panic_count[PLAYER_CPU_MAX_FREQUENCIES]; // Count of panics at each frequency
} PlayerCPUState;

/**
 * Result of an update operation (for detailed testing).
 */
typedef struct {
	PlayerCPUDecision decision; // What decision was made
	int new_index; // New frequency index (if granular)
	int new_level; // New level (if fallback)
	unsigned utilization; // Calculated utilization (%)
	uint64_t p90_time; // 90th percentile frame time
} PlayerCPUResult;

/**
 * Initializes config with default values.
 *
 * @param config Config to initialize
 */
void PlayerCPU_initConfig(PlayerCPUConfig* config);

/**
 * Initializes state to empty/zero state.
 *
 * @param state State to initialize
 */
void PlayerCPU_initState(PlayerCPUState* state);

/**
 * Finds the index of the nearest frequency to the target.
 *
 * @param frequencies Array of frequencies in kHz
 * @param count Number of frequencies in array
 * @param target_khz Target frequency to find
 * @return Index of nearest frequency (0 if count <= 0)
 */
int PlayerCPU_findNearestIndex(const int* frequencies, int count, int target_khz);

/**
 * Detects available CPU frequencies and initializes granular scaling.
 *
 * Populates state->frequencies and state->preset_indices based on
 * available system frequencies.
 *
 * @param state State to populate
 * @param config Configuration (uses min_freq_khz)
 * @param raw_frequencies Array of frequencies from platform
 * @param raw_count Number of frequencies from platform
 */
void PlayerCPU_detectFrequencies(PlayerCPUState* state, const PlayerCPUConfig* config,
                                 const int* raw_frequencies, int raw_count);

/**
 * Resets auto CPU state for a new session.
 *
 * Called when entering auto mode or starting a new game.
 *
 * @param state State to reset
 * @param config Configuration
 * @param fps Game's target FPS (for frame budget calculation)
 * @param current_underruns Current underrun count from audio system
 */
void PlayerCPU_reset(PlayerCPUState* state, const PlayerCPUConfig* config, double fps,
                     unsigned current_underruns);

/**
 * Records a frame time sample.
 *
 * Called after each frame with the execution time of core.run().
 *
 * @param state State to update
 * @param frame_time_us Frame execution time in microseconds
 */
void PlayerCPU_recordFrameTime(PlayerCPUState* state, uint64_t frame_time_us);

/**
 * Main update function - determines if CPU frequency should change.
 *
 * Should be called once per frame when in auto mode.
 * Returns a decision indicating what action should be taken.
 *
 * @param state Current state (will be modified)
 * @param config Configuration constants
 * @param fast_forward True if fast-forwarding (skip scaling)
 * @param show_menu True if menu is showing (skip scaling)
 * @param current_underruns Current underrun count from audio
 * @param result Optional output for detailed result info
 * @return Decision type (NONE, BOOST, REDUCE, PANIC, SKIP)
 */
PlayerCPUDecision PlayerCPU_update(PlayerCPUState* state, const PlayerCPUConfig* config,
                                   bool fast_forward, bool show_menu, unsigned current_underruns,
                                   PlayerCPUResult* result);

/**
 * Calculates the recommended frequency for a target utilization.
 *
 * Uses linear scaling: new_freq = current_freq * current_util / target_util
 *
 * @param current_freq Current frequency in kHz
 * @param current_util Current utilization percentage
 * @param target_util Target utilization percentage
 * @return Recommended frequency in kHz
 */
int PlayerCPU_predictFrequency(int current_freq, int current_util, int target_util);

/**
 * Returns the percentage of max frequency for a preset level.
 *
 * @param level Preset level (0=POWERSAVE, 1=NORMAL, 2=PERFORMANCE)
 * @return Percentage of max frequency (55, 80, or 100)
 */
int PlayerCPU_getPresetPercentage(PlayerCPULevel level);

/**
 * Calculates the 90th percentile of frame times.
 *
 * @param frame_times Array of frame times
 * @param count Number of samples (uses min of count and buffer size)
 * @return 90th percentile value
 */
uint64_t PlayerCPU_percentile90(const uint64_t* frame_times, int count);

#endif // __PLAYER_CPU_H__
