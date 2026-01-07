/**
 * cpu.h - CPU scaling and topology utilities
 *
 * Provides types and functions for CPU topology detection and dynamic
 * frequency scaling. Used by both the launcher (for topology detection)
 * and player (for performance-based autoscaling).
 *
 * Three scaling modes are supported:
 * - Topology mode: Multi-cluster SoCs (big.LITTLE, etc.) using PerfState ladder
 * - Granular mode: Single-cluster with all available frequencies (linear scaling)
 * - Fallback mode: 3 fixed levels (powersave/normal/performance)
 *
 * Topology mode:
 * - Detects CPU clusters via sysfs and builds a performance state ladder
 * - Uses GOVERNORS (powersave/schedutil/performance) rather than frequency bounds
 * - Works WITH the kernel's frequency scaling instead of fighting it
 * - Creates a gradient: 3 states per cluster tier (powersave/schedutil/performance)
 * - Progresses: LITTLE tier → BIG tier → PRIME tier (if available)
 * - Uses CPU affinity to guide which cluster the emulation thread runs on
 *
 * Designed for testability with injectable state and callbacks.
 */

#ifndef __CPU_H__
#define __CPU_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum number of CPU frequencies that can be detected.
 */
#define CPU_MAX_FREQUENCIES 32

/**
 * Ring buffer size for frame timing samples.
 */
#define CPU_FRAME_BUFFER_SIZE 64

/**
 * Default tuning constants.
 * These can be overridden via CPUConfig.
 */
#define CPU_DEFAULT_WINDOW_FRAMES 30 // ~500ms at 60fps
#define CPU_DEFAULT_UTIL_HIGH 85 // Boost threshold (%)
#define CPU_DEFAULT_UTIL_LOW 55 // Reduce threshold (%)
#define CPU_DEFAULT_BOOST_WINDOWS 2 // Windows before boost (~1s)
#define CPU_DEFAULT_REDUCE_WINDOWS 4 // Windows before reduce (~2s)
#define CPU_DEFAULT_STARTUP_GRACE 300 // Frames to skip (~5s at 60fps)
#define CPU_DEFAULT_MIN_FREQ_KHZ 0 // No minimum (panic failsafe handles problematic freqs)
#define CPU_DEFAULT_TARGET_UTIL 70 // Target utilization after change
#define CPU_DEFAULT_MAX_STEP_DOWN 1 // Max frequency steps when reducing
#define CPU_DEFAULT_PANIC_STEP_UP 1 // Frequency steps on panic (underrun)
#define CPU_PANIC_THRESHOLD 3 // Block frequency after this many panics
#define CPU_PANIC_GRACE_FRAMES 60 // Frames to ignore underruns after freq change (~1s at 60fps)
#define CPU_PANIC_GRACE_MAX_UNDERRUNS 5 // Max underruns during grace before panic anyway
#define CPU_STABILITY_DECAY_WINDOWS 8 // Stable windows before decaying panic counts (~4s)
#define CPU_DEFAULT_MIN_BUFFER_FOR_REDUCE 40 // Min audio buffer % to allow reduce

/**
 * Multi-cluster topology constants.
 */
#define CPU_MAX_CLUSTERS 8 // Maximum CPU clusters (policies)
#define CPU_MAX_PERF_STATES 16 // Maximum performance states in ladder
#define CPU_MAX_FREQS_PER_CLUSTER 16 // Maximum frequencies per cluster

/**
 * Preset level indices.
 */
typedef enum { CPU_LEVEL_POWERSAVE = 0, CPU_LEVEL_NORMAL = 1, CPU_LEVEL_PERFORMANCE = 2 } CPULevel;

/**
 * Cluster type classification based on relative performance.
 * Determined by sorting clusters by max_khz and analyzing the distribution.
 */
typedef enum {
	CPU_CLUSTER_LITTLE = 0, // Efficiency cores (lowest max_khz)
	CPU_CLUSTER_BIG = 1, // Performance cores (middle)
	CPU_CLUSTER_PRIME = 2, // Premium core (highest max_khz, often single)
} CPUClusterType;

/**
 * Governor types for PerfState ladder.
 *
 * Instead of manipulating frequency bounds, we use governors to create
 * a gradient of performance levels within each cluster tier:
 * - POWERSAVE: runs at minimum frequency (very efficient)
 * - SCHEDUTIL: dynamic scaling based on load (balanced)
 * - PERFORMANCE: runs at maximum frequency (full power)
 */
typedef enum {
	CPU_GOV_POWERSAVE = 0, // Min frequency - for light workloads
	CPU_GOV_SCHEDUTIL = 1, // Dynamic scaling - kernel finds sweet spot
	CPU_GOV_PERFORMANCE = 2, // Max frequency - for demanding workloads
} CPUGovernor;

/**
 * Information about a single CPU cluster (cpufreq policy).
 * Each cluster represents a group of CPUs that share a frequency.
 */
typedef struct {
	int policy_id; // Policy number (0, 4, 7, etc. from policyN)
	int cpu_mask; // Bitmask of CPUs in this cluster
	int cpu_count; // Number of CPUs in cluster
	int frequencies[CPU_MAX_FREQS_PER_CLUSTER]; // Available frequencies (kHz, sorted ascending)
	int freq_count; // Number of frequencies
	int min_khz; // cpuinfo_min_freq
	int max_khz; // cpuinfo_max_freq
	CPUClusterType type; // LITTLE/BIG/PRIME classification
} CPUCluster;

/**
 * A performance state represents one step in the autoscaler's ladder.
 *
 * Instead of manipulating frequency bounds, each state specifies:
 * - Which cluster is "active" (where the emulation thread should run)
 * - What governor to use on each cluster
 * - CPU affinity to guide the scheduler
 *
 * This works WITH the kernel's frequency scaling rather than against it.
 */
typedef struct {
	CPUGovernor cluster_governor[CPU_MAX_CLUSTERS]; // Governor per cluster
	int cpu_affinity_mask; // Bitmask of CPUs for emulation thread
	int active_cluster_idx; // Which cluster is the "active" one
} CPUPerfState;

/**
 * Complete CPU topology information detected from sysfs.
 * Populated by PWR_detectCPUTopology() at initialization.
 */
typedef struct CPUTopology {
	CPUCluster clusters[CPU_MAX_CLUSTERS]; // Detected clusters (sorted by max_khz)
	int cluster_count; // Number of clusters detected
	CPUPerfState states[CPU_MAX_PERF_STATES]; // Performance state ladder
	int state_count; // Number of states in ladder
	int topology_detected; // 1 if detection completed successfully
} CPUTopology;

/**
 * Decision type returned by CPU_update().
 */
typedef enum {
	CPU_DECISION_NONE = 0, // No change needed
	CPU_DECISION_BOOST, // Increase frequency/level
	CPU_DECISION_REDUCE, // Decrease frequency/level
	CPU_DECISION_PANIC, // Emergency boost (underrun detected)
	CPU_DECISION_SKIP // Skipped (grace period, menu, etc.)
} CPUDecision;

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
	unsigned int min_buffer_for_reduce; // Min audio buffer % to allow reduce
} CPUConfig;

/**
 * State for auto CPU scaling.
 * All fields can be inspected for testing.
 */
typedef struct {
	// Frequency array (populated by detectFrequencies)
	int frequencies[CPU_MAX_FREQUENCIES]; // Available frequencies (kHz, sorted low→high)
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
	uint64_t frame_times[CPU_FRAME_BUFFER_SIZE]; // Ring buffer of frame times (us)
	int frame_time_index; // Current ring buffer position
	uint64_t frame_budget_us; // Target frame time (from fps)

	// Flags for frequency detection
	int frequencies_detected; // 1 if frequencies have been detected
	int scaling_disabled; // 1 if scaling is disabled (0 or 1 frequency available)

	// Per-frequency panic tracking (failsafe for problematic frequencies)
	int panic_count[CPU_MAX_FREQUENCIES]; // Count of panics at each frequency

	// Grace period and stability tracking
	int panic_grace; // Frames remaining where underruns are ignored after freq change
	int grace_underruns; // Underruns accumulated during grace period
	int stability_streak; // Consecutive windows without panic (for decay)

	// Multi-cluster topology support
	CPUTopology topology; // Detected CPU topology
	int target_state; // Target PerfState index (multi-cluster mode)
	int current_state; // Currently applied PerfState index
	int use_topology; // 1 = multi-cluster mode active
	int pending_affinity; // CPU mask to apply from main thread (0 = none pending)
} CPUState;

/**
 * Result of an update operation (for detailed testing).
 */
typedef struct {
	CPUDecision decision; // What decision was made
	int new_index; // New frequency index (if granular)
	int new_level; // New level (if fallback)
	unsigned utilization; // Calculated utilization (%)
	uint64_t p90_time; // 90th percentile frame time
} CPUResult;

/**
 * Initializes config with default values.
 *
 * @param config Config to initialize
 */
void CPU_initConfig(CPUConfig* config);

/**
 * Initializes state to empty/zero state.
 *
 * @param state State to initialize
 */
void CPU_initState(CPUState* state);

/**
 * Finds the index of the nearest frequency to the target.
 *
 * @param frequencies Array of frequencies in kHz
 * @param count Number of frequencies in array
 * @param target_khz Target frequency to find
 * @return Index of nearest frequency (0 if count <= 0)
 */
int CPU_findNearestIndex(const int* frequencies, int count, int target_khz);

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
void CPU_detectFrequencies(CPUState* state, const CPUConfig* config, const int* raw_frequencies,
                           int raw_count);

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
void CPU_reset(CPUState* state, const CPUConfig* config, double fps, unsigned current_underruns);

/**
 * Records a frame time sample.
 *
 * Called after each frame with the execution time of core.run().
 *
 * @param state State to update
 * @param frame_time_us Frame execution time in microseconds
 */
void CPU_recordFrameTime(CPUState* state, uint64_t frame_time_us);

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
 * @param buffer_fill_percent Current audio buffer fill (0-100)
 * @param result Optional output for detailed result info
 * @return Decision type (NONE, BOOST, REDUCE, PANIC, SKIP)
 */
CPUDecision CPU_update(CPUState* state, const CPUConfig* config, bool fast_forward, bool show_menu,
                       unsigned current_underruns, unsigned buffer_fill_percent, CPUResult* result);

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
int CPU_predictFrequency(int current_freq, int current_util, int target_util);

/**
 * Returns the percentage of max frequency for a preset level.
 *
 * @param level Preset level (0=POWERSAVE, 1=NORMAL, 2=PERFORMANCE)
 * @return Percentage of max frequency (55, 80, or 100)
 */
int CPU_getPresetPercentage(CPULevel level);

/**
 * Returns the current performance level as a normalized percentage (0-100).
 *
 * Provides a unified view of CPU scaling state regardless of mode:
 * - Topology mode: (current_state / (state_count - 1)) * 100
 * - Granular mode: (current_index / (freq_count - 1)) * 100
 * - Fallback mode: current_level * 50 (0=0%, 1=50%, 2=100%)
 *
 * @param state CPU state to query
 * @return Performance level 0-100, or -1 if scaling disabled
 */
int CPU_getPerformancePercent(const CPUState* state);

/**
 * Returns a string describing the current CPU scaling mode.
 *
 * @param state CPU state to query
 * @return "topology", "granular", "fallback", or "disabled"
 */
const char* CPU_getModeName(const CPUState* state);

/**
 * Calculates the 90th percentile of frame times.
 *
 * @param frame_times Array of frame times
 * @param count Number of samples (uses min of count and buffer size)
 * @return 90th percentile value
 */
uint64_t CPU_percentile90(const uint64_t* frame_times, int count);

///////////////////////////////
// Multi-cluster topology functions
///////////////////////////////

/**
 * Initializes topology structure to empty state.
 *
 * @param topology Topology to initialize
 */
void CPU_initTopology(CPUTopology* topology);

/**
 * Builds the PerfState ladder from detected topology.
 *
 * Creates a progression of performance states using governors:
 * - Single-cluster: No states built (use existing frequency array)
 * - Dual-cluster: 6 states (LITTLE powersave/schedutil/performance,
 *                           BIG powersave/schedutil/performance)
 * - Tri-cluster: 9 states (add PRIME powersave/schedutil/performance)
 *
 * Each state sets:
 * - Active cluster's governor (powersave/schedutil/performance)
 * - Inactive clusters to powersave (let them idle)
 * - CPU affinity to guide emulation thread to active cluster
 *
 * @param state CPU state with populated topology.clusters
 * @param config Configuration
 */
void CPU_buildPerfStates(CPUState* state, const CPUConfig* config);

/**
 * Applies a PerfState by setting cluster governors and thread affinity.
 *
 * Called by background thread when target_state != current_state.
 * Sets governors on all clusters and queues affinity change for main thread.
 *
 * @param state CPU state with target_state set
 * @return 0 on success, -1 on failure
 */
int CPU_applyPerfState(CPUState* state);

/**
 * Parses a CPU list string (e.g., "0-3" or "0 1 2 3") into a bitmask.
 *
 * @param str CPU list string from sysfs (e.g., "0-3,5,7-8")
 * @param cpu_count Output: number of CPUs in the list
 * @return Bitmask of CPUs
 */
int CPU_parseCPUList(const char* str, int* cpu_count);

/**
 * Classifies clusters based on their relative performance.
 *
 * After clusters are sorted by max_khz, this assigns LITTLE/BIG/PRIME types:
 * - clusters[0] = LITTLE
 * - clusters[N-1] = PRIME if single CPU or >10% faster than next
 * - Middle clusters = BIG
 *
 * @param clusters Array of clusters (must be sorted by max_khz ascending)
 * @param count Number of clusters
 */
void CPU_classifyClusters(CPUCluster* clusters, int count);

/**
 * Picks 3 representative frequencies from a cluster's available frequencies.
 *
 * Selects low (min), mid (middle), and high (max) frequencies for building
 * the PerfState ladder.
 *
 * @param cluster Cluster with populated frequencies
 * @param low_khz Output: low frequency (freqs[0])
 * @param mid_khz Output: mid frequency (freqs[count/2])
 * @param high_khz Output: high frequency (freqs[count-1])
 */
void CPU_pickRepresentativeFreqs(const CPUCluster* cluster, int* low_khz, int* mid_khz,
                                 int* high_khz);

#endif // __CPU_H__
