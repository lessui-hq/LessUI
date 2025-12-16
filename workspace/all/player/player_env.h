/**
 * player_env.h - Environment callback handlers for Player
 *
 * This module provides testable handler functions for libretro environment
 * callbacks. Each handler takes explicit parameters rather than accessing
 * globals, enabling unit testing with mock state.
 *
 * Usage:
 *   The main environment_callback() in player.c dispatches to these handlers.
 *   Each handler receives only the state it needs and returns a result.
 *
 * Extraction from player.c environment_callback():
 *   - Original: 413 lines, 40 switch cases
 *   - Handlers extract complex logic for independent testing
 *   - Simple cases (return bool) remain inline in dispatcher
 */

#ifndef PLAYER_ENV_H
#define PLAYER_ENV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libretro.h"

///////////////////////////////
// Types
///////////////////////////////

/**
 * VideoState - Video geometry and timing state
 *
 * Tracks dynamic video configuration received from libretro core:
 * - Rotation (0°, 90°, 180°, 270°)
 * - Geometry changes (resolution/aspect ratio)
 * - Frame timing callbacks for VRR/delta time support
 */
typedef struct PlayerVideoState {
	unsigned rotation; // 0=0°, 1=90° CCW, 2=180°, 3=270° CCW
	int geometry_changed; // Flag: SET_GEOMETRY was called
	int av_info_changed; // Flag: SET_SYSTEM_AV_INFO was called
	retro_frame_time_callback_t frame_time_cb; // Frame timing callback
	retro_usec_t frame_time_ref; // Reference frame time (μs)
	retro_usec_t frame_time_last; // Last frame timestamp for delta
} PlayerVideoState;

/**
 * ThrottleMode - Current execution speed mode
 */
typedef struct PlayerThrottleInfo {
	int fast_forward; // Currently fast-forwarding
	int max_ff_speed; // FF speed multiplier (0=2x, 1=3x, 2=4x, 3=5x)
} PlayerThrottleInfo;

/**
 * CoreInfo - Subset of Core struct needed by env handlers
 */
typedef struct PlayerEnvCoreInfo {
	const char* bios_dir; // BIOS files directory
	const char* saves_dir; // Save files directory
	double fps; // Target framerate
	double sample_rate; // Audio sample rate
	double aspect_ratio; // Display aspect ratio
} PlayerEnvCoreInfo;

///////////////////////////////
// Handler Results
///////////////////////////////

/**
 * EnvResult - Result from environment handlers
 *
 * Handlers return this to indicate success/failure and whether
 * the caller should continue processing.
 */
typedef struct EnvResult {
	bool handled; // Request was handled
	bool success; // Handler succeeded
} EnvResult;

static inline EnvResult EnvResult_ok(void) {
	return (EnvResult){.handled = true, .success = true};
}

static inline EnvResult EnvResult_fail(void) {
	return (EnvResult){.handled = true, .success = false};
}

static inline EnvResult EnvResult_unhandled(void) {
	return (EnvResult){.handled = false, .success = false};
}

///////////////////////////////
// Video Handlers
///////////////////////////////

/**
 * Handle RETRO_ENVIRONMENT_SET_ROTATION (1)
 *
 * Sets display rotation. Valid values: 0-3 (0°, 90°, 180°, 270° CCW)
 *
 * @param state Video state to update
 * @param data Pointer to unsigned rotation value
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_setRotation(PlayerVideoState* state, const void* data);

/**
 * Handle RETRO_ENVIRONMENT_SET_PIXEL_FORMAT (10)
 *
 * Sets pixel format for video output.
 *
 * @param pixel_format Pointer to current pixel format (updated on success)
 * @param data Pointer to requested retro_pixel_format
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_setPixelFormat(enum retro_pixel_format* pixel_format, const void* data);

/**
 * Handle RETRO_ENVIRONMENT_SET_GEOMETRY (37)
 *
 * Updates video geometry (width/height/aspect) during runtime.
 *
 * @param state Video state to update
 * @param renderer_dst_p Pointer to renderer dst_p (set to 0 to force recalc)
 * @param data Pointer to retro_game_geometry
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_setGeometry(PlayerVideoState* state, int* renderer_dst_p, const void* data);

/**
 * Handle RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO (32)
 *
 * Updates full AV info (geometry + timing). May reinitialize audio.
 *
 * @param state Video state to update
 * @param core_info Core info to update (fps, sample_rate, aspect_ratio)
 * @param renderer_dst_p Pointer to renderer dst_p (set to 0 to force recalc)
 * @param reinit_audio Callback to reinitialize audio (old_rate, new_rate, fps)
 * @param data Pointer to retro_system_av_info
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_setSystemAVInfo(PlayerVideoState* state, double* fps, double* sample_rate,
                                    double* aspect_ratio, int* renderer_dst_p,
                                    void (*reinit_audio)(double old_rate, double new_rate,
                                                         double fps),
                                    const void* data);

/**
 * Handle RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK (21)
 *
 * Registers a callback for frame delta time reporting.
 *
 * @param state Video state to update
 * @param data Pointer to retro_frame_time_callback
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_setFrameTimeCallback(PlayerVideoState* state, const void* data);

///////////////////////////////
// Query Handlers
///////////////////////////////

/**
 * Handle RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY (9)
 *
 * Returns BIOS/system directory path.
 *
 * @param bios_dir BIOS directory path
 * @param data Output pointer (const char**)
 * @return EnvResult (always succeeds)
 */
EnvResult PlayerEnv_getSystemDirectory(const char* bios_dir, void* data);

/**
 * Handle RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY (31)
 *
 * Returns save files directory path.
 *
 * @param saves_dir Saves directory path
 * @param data Output pointer (const char**)
 * @return EnvResult (always succeeds)
 */
EnvResult PlayerEnv_getSaveDirectory(const char* saves_dir, void* data);

/**
 * Handle RETRO_ENVIRONMENT_GET_FASTFORWARDING (49)
 *
 * Returns whether fast-forward is currently active.
 *
 * @param fast_forward Current fast-forward state
 * @param data Output pointer (bool*)
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_getFastforwarding(int fast_forward, void* data);

/**
 * Handle RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE (50)
 *
 * Returns target display refresh rate.
 *
 * @param fps Target framerate
 * @param data Output pointer (float*)
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_getTargetRefreshRate(double fps, void* data);

/**
 * Handle RETRO_ENVIRONMENT_GET_THROTTLE_STATE (71)
 *
 * Returns current throttle mode and rate.
 *
 * @param throttle Current throttle info
 * @param data Output pointer (retro_throttle_state*)
 * @return EnvResult indicating success/failure
 */
EnvResult PlayerEnv_getThrottleState(const PlayerThrottleInfo* throttle, void* data);

/**
 * Handle RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47)
 *
 * Returns which AV outputs are enabled.
 *
 * @param data Output pointer (int*)
 * @return EnvResult (always succeeds)
 */
EnvResult PlayerEnv_getAudioVideoEnable(void* data);

///////////////////////////////
// Controller Handlers
///////////////////////////////

/**
 * Handle RETRO_ENVIRONMENT_SET_CONTROLLER_INFO (35)
 *
 * Processes controller type information from core.
 * Sets has_custom_controllers flag if DualShock detected.
 *
 * @param has_custom_controllers Flag to set if custom controllers found
 * @param data Pointer to retro_controller_info array
 * @return EnvResult (returns false per libretro convention)
 */
EnvResult PlayerEnv_setControllerInfo(int* has_custom_controllers, const void* data);

///////////////////////////////
// Disk Control Handlers
///////////////////////////////

/**
 * Handle RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE (13)
 *
 * Registers legacy disk control callbacks.
 *
 * @param disk_control Pointer to disk_control_ext_callback to update
 * @param data Pointer to retro_disk_control_callback
 * @return EnvResult (always succeeds)
 */
EnvResult PlayerEnv_setDiskControlInterface(struct retro_disk_control_ext_callback* disk_control,
                                            const void* data);

/**
 * Handle RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE (58)
 *
 * Registers extended disk control callbacks.
 *
 * @param disk_control Pointer to disk_control_ext_callback to update
 * @param data Pointer to retro_disk_control_ext_callback
 * @return EnvResult (always succeeds)
 */
EnvResult PlayerEnv_setDiskControlExtInterface(struct retro_disk_control_ext_callback* disk_control,
                                               const void* data);

///////////////////////////////
// Audio Handlers
///////////////////////////////

/**
 * Handle RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK (62)
 *
 * Registers callback for audio buffer status monitoring.
 *
 * @param audio_buffer_status Pointer to callback pointer to update
 * @param data Pointer to retro_audio_buffer_status_callback
 * @return EnvResult (always succeeds)
 */
EnvResult
PlayerEnv_setAudioBufferStatusCallback(retro_audio_buffer_status_callback_t* audio_buffer_status,
                                       const void* data);

///////////////////////////////
// Initialization
///////////////////////////////

/**
 * Initialize VideoState to default values.
 *
 * @param state State to initialize
 */
void PlayerVideoState_init(PlayerVideoState* state);

/**
 * Get description string for rotation value.
 *
 * @param rotation Rotation value (0-3)
 * @return Human-readable description
 */
const char* PlayerEnv_getRotationDesc(unsigned rotation);

/**
 * Get description string for pixel format.
 *
 * @param format Pixel format enum value
 * @return Human-readable description
 */
const char* PlayerEnv_getPixelFormatDesc(enum retro_pixel_format format);

#endif /* PLAYER_ENV_H */
