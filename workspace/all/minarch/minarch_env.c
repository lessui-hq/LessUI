/**
 * minarch_env.c - Environment callback handlers for MinArch
 *
 * Implementation of testable environment callback handlers.
 * Each function handles one or more related RETRO_ENVIRONMENT_* commands.
 */

#include "minarch_env.h"

#include <string.h>

#include "log.h"
#include "utils.h"

///////////////////////////////
// Initialization
///////////////////////////////

void MinArchVideoState_init(MinArchVideoState* state) {
	if (!state)
		return;

	state->rotation = 0;
	state->geometry_changed = 0;
	state->av_info_changed = 0;
	state->frame_time_cb = NULL;
	state->frame_time_ref = 0;
	state->frame_time_last = 0;
}

const char* MinArchEnv_getRotationDesc(unsigned rotation) {
	switch (rotation) {
	case 0:
		return "0 (normal)";
	case 1:
		return "90 CCW";
	case 2:
		return "180";
	case 3:
		return "270 CCW";
	default:
		return "invalid";
	}
}

const char* MinArchEnv_getPixelFormatDesc(enum retro_pixel_format format) {
	switch (format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
		return "0RGB1555 (15-bit, conversion to RGB565)";
	case RETRO_PIXEL_FORMAT_XRGB8888:
		return "XRGB8888 (32-bit, conversion to RGB565)";
	case RETRO_PIXEL_FORMAT_RGB565:
		return "RGB565 (native, no conversion needed)";
	default:
		return "unknown";
	}
}

///////////////////////////////
// Video Handlers
///////////////////////////////

EnvResult MinArchEnv_setRotation(MinArchVideoState* state, const void* data) {
	const unsigned* rotation = (const unsigned*)data;

	if (!rotation) {
		LOG_error("SET_ROTATION called with NULL data");
		return EnvResult_fail();
	}

	if (*rotation > 3) {
		LOG_error("SET_ROTATION invalid value: %u (must be 0-3)", *rotation);
		return EnvResult_fail();
	}

	if (state) {
		state->rotation = *rotation;
		LOG_info("SET_ROTATION: %u (%s)", *rotation, MinArchEnv_getRotationDesc(*rotation));
	}

	return EnvResult_ok();
}

EnvResult MinArchEnv_setPixelFormat(enum retro_pixel_format* pixel_format, const void* data) {
	const enum retro_pixel_format* format = (const enum retro_pixel_format*)data;

	if (!format) {
		LOG_error("SET_PIXEL_FORMAT called with NULL data");
		return EnvResult_fail();
	}

	switch (*format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
	case RETRO_PIXEL_FORMAT_XRGB8888:
	case RETRO_PIXEL_FORMAT_RGB565:
		if (pixel_format) {
			*pixel_format = *format;
			LOG_info("Core requested %s format", MinArchEnv_getPixelFormatDesc(*format));
		}
		return EnvResult_ok();

	default:
		LOG_error("Core requested unknown pixel format %d", *format);
		return EnvResult_fail();
	}
}

EnvResult MinArchEnv_setGeometry(MinArchVideoState* state, int* renderer_dst_p, const void* data) {
	const struct retro_game_geometry* geometry = (const struct retro_game_geometry*)data;

	if (!geometry) {
		LOG_error("SET_GEOMETRY called with NULL data");
		return EnvResult_fail();
	}

	LOG_debug("SET_GEOMETRY: %ux%u aspect: %.3f", geometry->base_width, geometry->base_height,
	          geometry->aspect_ratio);

	// NOTE: Do NOT update aspect_ratio here!
	// SET_GEOMETRY reports "display" dimensions which may differ from actual
	// video_refresh frame dimensions (e.g. Stella reports 320 but sends 160).
	// Aspect ratio should only be updated via SET_SYSTEM_AV_INFO.

	// Force scaler recalculation on next video_refresh
	if (renderer_dst_p) {
		*renderer_dst_p = 0;
	}

	if (state) {
		state->geometry_changed = 1;
	}

	return EnvResult_ok();
}

EnvResult MinArchEnv_setSystemAVInfo(MinArchVideoState* state, double* fps, double* sample_rate,
                                     double* aspect_ratio, int* renderer_dst_p,
                                     void (*reinit_audio)(double old_rate, double new_rate,
                                                          double fps),
                                     const void* data) {
	const struct retro_system_av_info* av_info = (const struct retro_system_av_info*)data;

	if (!av_info) {
		LOG_error("SET_SYSTEM_AV_INFO called with NULL data");
		return EnvResult_fail();
	}

	LOG_debug("SET_SYSTEM_AV_INFO: %ux%u @ %.2f fps, %.0f Hz", av_info->geometry.base_width,
	          av_info->geometry.base_height, av_info->timing.fps, av_info->timing.sample_rate);

	// Update aspect ratio
	if (aspect_ratio) {
		if (av_info->geometry.aspect_ratio > 0.0) {
			*aspect_ratio = av_info->geometry.aspect_ratio;
		} else {
			*aspect_ratio = (double)av_info->geometry.base_width / av_info->geometry.base_height;
		}
	}

	// Update timing and possibly reinitialize audio
	double old_rate = sample_rate ? *sample_rate : 0;

	if (fps) {
		*fps = av_info->timing.fps;
	}

	if (sample_rate) {
		*sample_rate = av_info->timing.sample_rate;
	}

	// Reinitialize audio if sample rate changed
	if (reinit_audio && old_rate != av_info->timing.sample_rate) {
		reinit_audio(old_rate, av_info->timing.sample_rate, av_info->timing.fps);
	}

	// Force scaler recalculation
	if (renderer_dst_p) {
		*renderer_dst_p = 0;
	}

	if (state) {
		state->av_info_changed = 1;
	}

	return EnvResult_ok();
}

EnvResult MinArchEnv_setFrameTimeCallback(MinArchVideoState* state, const void* data) {
	const struct retro_frame_time_callback* cb = (const struct retro_frame_time_callback*)data;

	if (!cb) {
		LOG_error("SET_FRAME_TIME_CALLBACK called with NULL data");
		return EnvResult_fail();
	}

	if (!state) {
		return EnvResult_fail();
	}

	if (!cb->callback) {
		// NULL callback = unregister
		state->frame_time_cb = NULL;
		state->frame_time_ref = 0;
		state->frame_time_last = 0;
		return EnvResult_ok();
	}

	state->frame_time_cb = cb->callback;
	state->frame_time_ref = cb->reference;
	return EnvResult_ok();
}

///////////////////////////////
// Query Handlers
///////////////////////////////

EnvResult MinArchEnv_getSystemDirectory(const char* bios_dir, void* data) {
	const char** out = (const char**)data;
	if (out) {
		*out = bios_dir;
	}
	return EnvResult_ok();
}

EnvResult MinArchEnv_getSaveDirectory(const char* saves_dir, void* data) {
	const char** out = (const char**)data;
	if (out) {
		*out = saves_dir;
	}
	return EnvResult_ok();
}

EnvResult MinArchEnv_getFastforwarding(int fast_forward, void* data) {
	bool* out = (bool*)data;
	if (out) {
		*out = fast_forward != 0;
		return EnvResult_ok();
	}
	return EnvResult_fail();
}

EnvResult MinArchEnv_getTargetRefreshRate(double fps, void* data) {
	float* out = (float*)data;
	if (out) {
		*out = (float)fps;
		return EnvResult_ok();
	}
	return EnvResult_fail();
}

EnvResult MinArchEnv_getThrottleState(const MinArchThrottleInfo* throttle, void* data) {
	struct retro_throttle_state* state = (struct retro_throttle_state*)data;

	if (!state || !throttle) {
		return EnvResult_fail();
	}

	if (throttle->fast_forward) {
		state->mode = RETRO_THROTTLE_FAST_FORWARD;
		// max_ff_speed+1: 0→1x, 1→2x, 2→3x, 3→4x (matches original)
		state->rate = (float)(throttle->max_ff_speed + 1);
	} else {
		state->mode = RETRO_THROTTLE_VSYNC;
		state->rate = 1.0f;
	}

	return EnvResult_ok();
}

EnvResult MinArchEnv_getAudioVideoEnable(void* data) {
	int* out_p = (int*)data;
	if (out_p) {
		int out = 0;
		out |= RETRO_AV_ENABLE_VIDEO;
		out |= RETRO_AV_ENABLE_AUDIO;
		*out_p = out;
	}
	return EnvResult_ok();
}

///////////////////////////////
// Controller Handlers
///////////////////////////////

EnvResult MinArchEnv_setControllerInfo(int* has_custom_controllers, const void* data) {
	const struct retro_controller_info* infos = (const struct retro_controller_info*)data;

	if (!infos) {
		// Per libretro convention, return false for this callback
		return (EnvResult){.handled = true, .success = false};
	}

	// Check first port for custom controller types
	const struct retro_controller_info* info = &infos[0];
	for (unsigned int i = 0; i < info->num_types; i++) {
		const struct retro_controller_description* type = &info->types[i];
		// Currently only enabled for PlayStation (DualShock)
		if (exactMatch((char*)type->desc, "dualshock")) {
			if (has_custom_controllers) {
				*has_custom_controllers = 1;
			}
			break;
		}
	}

	// Per libretro convention, return false for this callback
	return (EnvResult){.handled = true, .success = false};
}

///////////////////////////////
// Disk Control Handlers
///////////////////////////////

EnvResult MinArchEnv_setDiskControlInterface(struct retro_disk_control_ext_callback* disk_control,
                                             const void* data) {
	const struct retro_disk_control_callback* var = (const struct retro_disk_control_callback*)data;

	if (var && disk_control) {
		memset(disk_control, 0, sizeof(struct retro_disk_control_ext_callback));
		memcpy(disk_control, var, sizeof(struct retro_disk_control_callback));
	}

	return EnvResult_ok();
}

EnvResult
MinArchEnv_setDiskControlExtInterface(struct retro_disk_control_ext_callback* disk_control,
                                      const void* data) {
	const struct retro_disk_control_ext_callback* var =
	    (const struct retro_disk_control_ext_callback*)data;

	if (var && disk_control) {
		memcpy(disk_control, var, sizeof(struct retro_disk_control_ext_callback));
	}

	return EnvResult_ok();
}

///////////////////////////////
// Audio Handlers
///////////////////////////////

EnvResult
MinArchEnv_setAudioBufferStatusCallback(retro_audio_buffer_status_callback_t* audio_buffer_status,
                                        const void* data) {
	const struct retro_audio_buffer_status_callback* cb =
	    (const struct retro_audio_buffer_status_callback*)data;

	if (audio_buffer_status) {
		*audio_buffer_status = cb ? cb->callback : NULL;
		LOG_info("SET_AUDIO_BUFFER_STATUS_CALLBACK: %s", cb ? "enabled" : "disabled");
	}

	return EnvResult_ok();
}
