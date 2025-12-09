/**
 * test_minarch_env.c - Unit tests for environment callback handlers
 *
 * Tests the extracted environment callback handlers in minarch_env.c.
 * These handlers process libretro RETRO_ENVIRONMENT_* callbacks.
 */

#include "unity.h"

#include <string.h>

#include "minarch_env.h"

void setUp(void) {}
void tearDown(void) {}

///////////////////////////////
// VideoState Initialization
///////////////////////////////

void test_VideoState_init_sets_defaults(void) {
	MinArchVideoState state;
	// Set non-zero values first
	state.rotation = 2;
	state.geometry_changed = 1;
	state.av_info_changed = 1;
	state.frame_time_cb = (void*)0x1234;
	state.frame_time_ref = 1000;
	state.frame_time_last = 2000;

	MinArchVideoState_init(&state);

	TEST_ASSERT_EQUAL(0, state.rotation);
	TEST_ASSERT_EQUAL(0, state.geometry_changed);
	TEST_ASSERT_EQUAL(0, state.av_info_changed);
	TEST_ASSERT_NULL(state.frame_time_cb);
	TEST_ASSERT_EQUAL(0, state.frame_time_ref);
	TEST_ASSERT_EQUAL(0, state.frame_time_last);
}

void test_VideoState_init_handles_null(void) {
	// Should not crash
	MinArchVideoState_init(NULL);
}

///////////////////////////////
// Rotation Description
///////////////////////////////

void test_getRotationDesc_normal(void) {
	TEST_ASSERT_EQUAL_STRING("0 (normal)", MinArchEnv_getRotationDesc(0));
}

void test_getRotationDesc_90ccw(void) {
	TEST_ASSERT_EQUAL_STRING("90 CCW", MinArchEnv_getRotationDesc(1));
}

void test_getRotationDesc_180(void) {
	TEST_ASSERT_EQUAL_STRING("180", MinArchEnv_getRotationDesc(2));
}

void test_getRotationDesc_270ccw(void) {
	TEST_ASSERT_EQUAL_STRING("270 CCW", MinArchEnv_getRotationDesc(3));
}

void test_getRotationDesc_invalid(void) {
	TEST_ASSERT_EQUAL_STRING("invalid", MinArchEnv_getRotationDesc(4));
	TEST_ASSERT_EQUAL_STRING("invalid", MinArchEnv_getRotationDesc(99));
}

///////////////////////////////
// Pixel Format Description
///////////////////////////////

void test_getPixelFormatDesc_0rgb1555(void) {
	const char* desc = MinArchEnv_getPixelFormatDesc(RETRO_PIXEL_FORMAT_0RGB1555);
	TEST_ASSERT_NOT_NULL(strstr(desc, "15-bit"));
}

void test_getPixelFormatDesc_xrgb8888(void) {
	const char* desc = MinArchEnv_getPixelFormatDesc(RETRO_PIXEL_FORMAT_XRGB8888);
	TEST_ASSERT_NOT_NULL(strstr(desc, "32-bit"));
}

void test_getPixelFormatDesc_rgb565(void) {
	const char* desc = MinArchEnv_getPixelFormatDesc(RETRO_PIXEL_FORMAT_RGB565);
	TEST_ASSERT_NOT_NULL(strstr(desc, "native"));
}

void test_getPixelFormatDesc_unknown(void) {
	const char* desc = MinArchEnv_getPixelFormatDesc(99);
	TEST_ASSERT_EQUAL_STRING("unknown", desc);
}

///////////////////////////////
// SET_ROTATION Handler
///////////////////////////////

void test_setRotation_valid_values(void) {
	MinArchVideoState state = {0};
	EnvResult result;

	for (unsigned i = 0; i <= 3; i++) {
		result = MinArchEnv_setRotation(&state, &i);
		TEST_ASSERT_TRUE(result.handled);
		TEST_ASSERT_TRUE(result.success);
		TEST_ASSERT_EQUAL(i, state.rotation);
	}
}

void test_setRotation_invalid_value(void) {
	MinArchVideoState state = {0};
	unsigned rotation = 4;

	EnvResult result = MinArchEnv_setRotation(&state, &rotation);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
	// State should not be modified on error
	TEST_ASSERT_EQUAL(0, state.rotation);
}

void test_setRotation_null_data(void) {
	MinArchVideoState state = {0};
	EnvResult result = MinArchEnv_setRotation(&state, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

void test_setRotation_null_state(void) {
	unsigned rotation = 1;
	// Should not crash, but will fail since no state to update
	EnvResult result = MinArchEnv_setRotation(NULL, &rotation);
	// The handler checks data first, then state - so it should succeed
	// even with NULL state (it just won't store anything)
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
}

///////////////////////////////
// SET_PIXEL_FORMAT Handler
///////////////////////////////

void test_setPixelFormat_0rgb1555(void) {
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565; // Start with different value
	enum retro_pixel_format requested = RETRO_PIXEL_FORMAT_0RGB1555;

	EnvResult result = MinArchEnv_setPixelFormat(&format, &requested);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(RETRO_PIXEL_FORMAT_0RGB1555, format);
}

void test_setPixelFormat_xrgb8888(void) {
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
	enum retro_pixel_format requested = RETRO_PIXEL_FORMAT_XRGB8888;

	EnvResult result = MinArchEnv_setPixelFormat(&format, &requested);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(RETRO_PIXEL_FORMAT_XRGB8888, format);
}

void test_setPixelFormat_rgb565(void) {
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_0RGB1555;
	enum retro_pixel_format requested = RETRO_PIXEL_FORMAT_RGB565;

	EnvResult result = MinArchEnv_setPixelFormat(&format, &requested);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(RETRO_PIXEL_FORMAT_RGB565, format);
}

void test_setPixelFormat_unknown(void) {
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
	enum retro_pixel_format requested = 99;

	EnvResult result = MinArchEnv_setPixelFormat(&format, &requested);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
	// Format should not be changed on error
	TEST_ASSERT_EQUAL(RETRO_PIXEL_FORMAT_RGB565, format);
}

void test_setPixelFormat_null_data(void) {
	enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
	EnvResult result = MinArchEnv_setPixelFormat(&format, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

///////////////////////////////
// SET_GEOMETRY Handler
///////////////////////////////

void test_setGeometry_updates_flags(void) {
	MinArchVideoState state = {0};
	int renderer_dst_p = 100;

	struct retro_game_geometry geometry = {.base_width = 320,
	                                       .base_height = 240,
	                                       .max_width = 640,
	                                       .max_height = 480,
	                                       .aspect_ratio = 4.0f / 3.0f};

	EnvResult result = MinArchEnv_setGeometry(&state, &renderer_dst_p, &geometry);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(1, state.geometry_changed);
	TEST_ASSERT_EQUAL(0, renderer_dst_p); // Should be reset to force recalc
}

void test_setGeometry_null_data(void) {
	MinArchVideoState state = {0};
	int renderer_dst_p = 100;

	EnvResult result = MinArchEnv_setGeometry(&state, &renderer_dst_p, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

///////////////////////////////
// SET_FRAME_TIME_CALLBACK Handler
///////////////////////////////

static void dummy_frame_time_cb(retro_usec_t usec) {
	(void)usec;
}

void test_setFrameTimeCallback_registers(void) {
	MinArchVideoState state = {0};
	struct retro_frame_time_callback cb = {.callback = dummy_frame_time_cb, .reference = 16666};

	EnvResult result = MinArchEnv_setFrameTimeCallback(&state, &cb);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL_PTR(dummy_frame_time_cb, state.frame_time_cb);
	TEST_ASSERT_EQUAL(16666, state.frame_time_ref);
}

void test_setFrameTimeCallback_unregisters(void) {
	MinArchVideoState state = {.frame_time_cb = dummy_frame_time_cb,
	                           .frame_time_ref = 16666,
	                           .frame_time_last = 1000};

	struct retro_frame_time_callback cb = {.callback = NULL, .reference = 0};

	EnvResult result = MinArchEnv_setFrameTimeCallback(&state, &cb);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_NULL(state.frame_time_cb);
	TEST_ASSERT_EQUAL(0, state.frame_time_ref);
	TEST_ASSERT_EQUAL(0, state.frame_time_last);
}

void test_setFrameTimeCallback_null_data(void) {
	MinArchVideoState state = {0};
	EnvResult result = MinArchEnv_setFrameTimeCallback(&state, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

///////////////////////////////
// Query Handlers
///////////////////////////////

void test_getSystemDirectory_returns_path(void) {
	const char* bios_dir = "/path/to/bios";
	const char* out = NULL;

	EnvResult result = MinArchEnv_getSystemDirectory(bios_dir, &out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL_PTR(bios_dir, out);
}

void test_getSaveDirectory_returns_path(void) {
	const char* saves_dir = "/path/to/saves";
	const char* out = NULL;

	EnvResult result = MinArchEnv_getSaveDirectory(saves_dir, &out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL_PTR(saves_dir, out);
}

void test_getFastforwarding_true(void) {
	bool out = false;
	EnvResult result = MinArchEnv_getFastforwarding(1, &out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_TRUE(out);
}

void test_getFastforwarding_false(void) {
	bool out = true;
	EnvResult result = MinArchEnv_getFastforwarding(0, &out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_FALSE(out);
}

void test_getFastforwarding_null_data(void) {
	EnvResult result = MinArchEnv_getFastforwarding(1, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

void test_getTargetRefreshRate_returns_fps(void) {
	float out = 0.0f;
	EnvResult result = MinArchEnv_getTargetRefreshRate(60.0, &out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, out);
}

void test_getTargetRefreshRate_null_data(void) {
	EnvResult result = MinArchEnv_getTargetRefreshRate(60.0, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

void test_getAudioVideoEnable_sets_flags(void) {
	int out = 0;
	EnvResult result = MinArchEnv_getAudioVideoEnable(&out);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_TRUE(out & RETRO_AV_ENABLE_VIDEO);
	TEST_ASSERT_TRUE(out & RETRO_AV_ENABLE_AUDIO);
}

///////////////////////////////
// GET_THROTTLE_STATE Handler
///////////////////////////////

void test_getThrottleState_normal_speed(void) {
	MinArchThrottleInfo throttle = {.fast_forward = 0, .max_ff_speed = 3};
	struct retro_throttle_state state = {0};

	EnvResult result = MinArchEnv_getThrottleState(&throttle, &state);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(RETRO_THROTTLE_VSYNC, state.mode);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, state.rate);
}

void test_getThrottleState_fast_forward(void) {
	MinArchThrottleInfo throttle = {.fast_forward = 1, .max_ff_speed = 3};
	struct retro_throttle_state state = {0};

	EnvResult result = MinArchEnv_getThrottleState(&throttle, &state);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(RETRO_THROTTLE_FAST_FORWARD, state.mode);
	// max_ff_speed + 1 = 4
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, state.rate);
}

void test_getThrottleState_null_data(void) {
	MinArchThrottleInfo throttle = {.fast_forward = 0, .max_ff_speed = 3};
	EnvResult result = MinArchEnv_getThrottleState(&throttle, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

void test_getThrottleState_null_throttle(void) {
	struct retro_throttle_state state = {0};
	EnvResult result = MinArchEnv_getThrottleState(NULL, &state);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

///////////////////////////////
// Disk Control Handlers
///////////////////////////////

void test_setDiskControlInterface_copies_data(void) {
	struct retro_disk_control_ext_callback disk_control = {0};
	struct retro_disk_control_callback cb = {
	    .set_eject_state = (void*)0x1000,
	    .get_eject_state = (void*)0x2000,
	    .get_image_index = (void*)0x3000,
	    .set_image_index = (void*)0x4000,
	    .get_num_images = (void*)0x5000,
	    .replace_image_index = (void*)0x6000,
	    .add_image_index = (void*)0x7000,
	};

	EnvResult result = MinArchEnv_setDiskControlInterface(&disk_control, &cb);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);

	// Verify callbacks were copied
	TEST_ASSERT_EQUAL_PTR(cb.set_eject_state, disk_control.set_eject_state);
	TEST_ASSERT_EQUAL_PTR(cb.get_eject_state, disk_control.get_eject_state);
	TEST_ASSERT_EQUAL_PTR(cb.get_image_index, disk_control.get_image_index);
}

void test_setDiskControlExtInterface_copies_data(void) {
	struct retro_disk_control_ext_callback disk_control = {0};
	struct retro_disk_control_ext_callback cb = {
	    .set_eject_state = (void*)0x1000,
	    .get_eject_state = (void*)0x2000,
	    .get_image_index = (void*)0x3000,
	    .set_image_index = (void*)0x4000,
	    .get_num_images = (void*)0x5000,
	    .replace_image_index = (void*)0x6000,
	    .add_image_index = (void*)0x7000,
	    .set_initial_image = (void*)0x8000,
	    .get_image_path = (void*)0x9000,
	    .get_image_label = (void*)0xA000,
	};

	EnvResult result = MinArchEnv_setDiskControlExtInterface(&disk_control, &cb);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);

	// Verify extended callbacks were copied
	TEST_ASSERT_EQUAL_PTR(cb.set_initial_image, disk_control.set_initial_image);
	TEST_ASSERT_EQUAL_PTR(cb.get_image_path, disk_control.get_image_path);
	TEST_ASSERT_EQUAL_PTR(cb.get_image_label, disk_control.get_image_label);
}

///////////////////////////////
// SET_SYSTEM_AV_INFO Handler
///////////////////////////////

static int audio_reinit_called = 0;
static double audio_reinit_new_rate = 0;

static void test_audio_reinit(double old_rate, double new_rate, double fps) {
	(void)old_rate;
	(void)fps;
	audio_reinit_called = 1;
	audio_reinit_new_rate = new_rate;
}

void test_setSystemAVInfo_updates_values(void) {
	MinArchVideoState state = {0};
	double fps = 0;
	double sample_rate = 44100;
	double aspect_ratio = 0;
	int renderer_dst_p = 100;

	audio_reinit_called = 0;

	struct retro_system_av_info av_info = {
	    .geometry =
	        {
	            .base_width = 320,
	            .base_height = 240,
	            .aspect_ratio = 4.0f / 3.0f,
	        },
	    .timing =
	        {
	            .fps = 60.0,
	            .sample_rate = 48000.0,
	        },
	};

	EnvResult result = MinArchEnv_setSystemAVInfo(&state, &fps, &sample_rate, &aspect_ratio,
	                                              &renderer_dst_p, test_audio_reinit, &av_info);

	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_FLOAT_WITHIN(0.1, 60.0, fps);
	TEST_ASSERT_FLOAT_WITHIN(0.1, 48000.0, sample_rate);
	TEST_ASSERT_FLOAT_WITHIN(0.01, 4.0 / 3.0, aspect_ratio);
	TEST_ASSERT_EQUAL(0, renderer_dst_p);
	TEST_ASSERT_EQUAL(1, state.av_info_changed);
	TEST_ASSERT_EQUAL(1, audio_reinit_called);
	TEST_ASSERT_FLOAT_WITHIN(0.1, 48000.0, audio_reinit_new_rate);
}

void test_setSystemAVInfo_no_audio_reinit_if_same_rate(void) {
	MinArchVideoState state = {0};
	double fps = 0;
	double sample_rate = 48000; // Same as av_info
	double aspect_ratio = 0;
	int renderer_dst_p = 100;

	audio_reinit_called = 0;

	struct retro_system_av_info av_info = {
	    .geometry =
	        {
	            .base_width = 320,
	            .base_height = 240,
	            .aspect_ratio = 4.0f / 3.0f,
	        },
	    .timing =
	        {
	            .fps = 60.0,
	            .sample_rate = 48000.0,
	        },
	};

	EnvResult result = MinArchEnv_setSystemAVInfo(&state, &fps, &sample_rate, &aspect_ratio,
	                                              &renderer_dst_p, test_audio_reinit, &av_info);

	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL(0, audio_reinit_called); // Should NOT be called
}

void test_setSystemAVInfo_null_data(void) {
	MinArchVideoState state = {0};
	double fps = 0, sample_rate = 0, aspect_ratio = 0;
	int renderer_dst_p = 100;

	EnvResult result = MinArchEnv_setSystemAVInfo(&state, &fps, &sample_rate, &aspect_ratio,
	                                              &renderer_dst_p, test_audio_reinit, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
}

void test_setSystemAVInfo_calculates_aspect_from_geometry(void) {
	MinArchVideoState state = {0};
	double fps = 0, sample_rate = 0, aspect_ratio = 0;
	int renderer_dst_p = 0;

	struct retro_system_av_info av_info = {
	    .geometry =
	        {
	            .base_width = 320,
	            .base_height = 200,
	            .aspect_ratio = 0, // Not provided
	        },
	    .timing =
	        {
	            .fps = 60.0,
	            .sample_rate = 44100.0,
	        },
	};

	MinArchEnv_setSystemAVInfo(&state, &fps, &sample_rate, &aspect_ratio, &renderer_dst_p, NULL,
	                           &av_info);

	// Should calculate from base_width / base_height = 320/200 = 1.6
	TEST_ASSERT_FLOAT_WITHIN(0.01, 1.6, aspect_ratio);
}

///////////////////////////////
// Audio Buffer Status Handler
///////////////////////////////

static void dummy_audio_buffer_status(bool active, unsigned occupancy, bool underrun) {
	(void)active;
	(void)occupancy;
	(void)underrun;
}

void test_setAudioBufferStatusCallback_registers(void) {
	retro_audio_buffer_status_callback_t cb_ptr = NULL;
	struct retro_audio_buffer_status_callback cb = {.callback = dummy_audio_buffer_status};

	EnvResult result = MinArchEnv_setAudioBufferStatusCallback(&cb_ptr, &cb);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_EQUAL_PTR(dummy_audio_buffer_status, cb_ptr);
}

void test_setAudioBufferStatusCallback_unregisters(void) {
	retro_audio_buffer_status_callback_t cb_ptr = dummy_audio_buffer_status;

	EnvResult result = MinArchEnv_setAudioBufferStatusCallback(&cb_ptr, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_TRUE(result.success);
	TEST_ASSERT_NULL(cb_ptr);
}

///////////////////////////////
// Controller Info Handler
///////////////////////////////

void test_setControllerInfo_detects_dualshock(void) {
	int has_custom = 0;
	struct retro_controller_description types[] = {
	    {.id = 1, .desc = "RetroPad"},
	    {.id = 2, .desc = "dualshock"},
	    {.id = 0, .desc = NULL},
	};
	struct retro_controller_info infos[] = {
	    {.types = types, .num_types = 2},
	    {.types = NULL, .num_types = 0},
	};

	EnvResult result = MinArchEnv_setControllerInfo(&has_custom, infos);
	TEST_ASSERT_TRUE(result.handled);
	// Per libretro convention, this callback returns false
	TEST_ASSERT_FALSE(result.success);
	TEST_ASSERT_EQUAL(1, has_custom);
}

void test_setControllerInfo_no_dualshock(void) {
	int has_custom = 0;
	struct retro_controller_description types[] = {
	    {.id = 1, .desc = "RetroPad"},
	    {.id = 2, .desc = "RetroPad with Analog"},
	    {.id = 0, .desc = NULL},
	};
	struct retro_controller_info infos[] = {
	    {.types = types, .num_types = 2},
	    {.types = NULL, .num_types = 0},
	};

	EnvResult result = MinArchEnv_setControllerInfo(&has_custom, infos);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
	TEST_ASSERT_EQUAL(0, has_custom);
}

void test_setControllerInfo_null_data(void) {
	int has_custom = 0;
	EnvResult result = MinArchEnv_setControllerInfo(&has_custom, NULL);
	TEST_ASSERT_TRUE(result.handled);
	TEST_ASSERT_FALSE(result.success);
	TEST_ASSERT_EQUAL(0, has_custom);
}

///////////////////////////////
// EnvResult Helpers
///////////////////////////////

void test_EnvResult_ok(void) {
	EnvResult r = EnvResult_ok();
	TEST_ASSERT_TRUE(r.handled);
	TEST_ASSERT_TRUE(r.success);
}

void test_EnvResult_fail(void) {
	EnvResult r = EnvResult_fail();
	TEST_ASSERT_TRUE(r.handled);
	TEST_ASSERT_FALSE(r.success);
}

void test_EnvResult_unhandled(void) {
	EnvResult r = EnvResult_unhandled();
	TEST_ASSERT_FALSE(r.handled);
	TEST_ASSERT_FALSE(r.success);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// VideoState initialization
	RUN_TEST(test_VideoState_init_sets_defaults);
	RUN_TEST(test_VideoState_init_handles_null);

	// Rotation description
	RUN_TEST(test_getRotationDesc_normal);
	RUN_TEST(test_getRotationDesc_90ccw);
	RUN_TEST(test_getRotationDesc_180);
	RUN_TEST(test_getRotationDesc_270ccw);
	RUN_TEST(test_getRotationDesc_invalid);

	// Pixel format description
	RUN_TEST(test_getPixelFormatDesc_0rgb1555);
	RUN_TEST(test_getPixelFormatDesc_xrgb8888);
	RUN_TEST(test_getPixelFormatDesc_rgb565);
	RUN_TEST(test_getPixelFormatDesc_unknown);

	// SET_ROTATION handler
	RUN_TEST(test_setRotation_valid_values);
	RUN_TEST(test_setRotation_invalid_value);
	RUN_TEST(test_setRotation_null_data);
	RUN_TEST(test_setRotation_null_state);

	// SET_PIXEL_FORMAT handler
	RUN_TEST(test_setPixelFormat_0rgb1555);
	RUN_TEST(test_setPixelFormat_xrgb8888);
	RUN_TEST(test_setPixelFormat_rgb565);
	RUN_TEST(test_setPixelFormat_unknown);
	RUN_TEST(test_setPixelFormat_null_data);

	// SET_GEOMETRY handler
	RUN_TEST(test_setGeometry_updates_flags);
	RUN_TEST(test_setGeometry_null_data);

	// SET_FRAME_TIME_CALLBACK handler
	RUN_TEST(test_setFrameTimeCallback_registers);
	RUN_TEST(test_setFrameTimeCallback_unregisters);
	RUN_TEST(test_setFrameTimeCallback_null_data);

	// Query handlers
	RUN_TEST(test_getSystemDirectory_returns_path);
	RUN_TEST(test_getSaveDirectory_returns_path);
	RUN_TEST(test_getFastforwarding_true);
	RUN_TEST(test_getFastforwarding_false);
	RUN_TEST(test_getFastforwarding_null_data);
	RUN_TEST(test_getTargetRefreshRate_returns_fps);
	RUN_TEST(test_getTargetRefreshRate_null_data);
	RUN_TEST(test_getAudioVideoEnable_sets_flags);

	// GET_THROTTLE_STATE handler
	RUN_TEST(test_getThrottleState_normal_speed);
	RUN_TEST(test_getThrottleState_fast_forward);
	RUN_TEST(test_getThrottleState_null_data);
	RUN_TEST(test_getThrottleState_null_throttle);

	// Disk control handlers
	RUN_TEST(test_setDiskControlInterface_copies_data);
	RUN_TEST(test_setDiskControlExtInterface_copies_data);

	// SET_SYSTEM_AV_INFO handler
	RUN_TEST(test_setSystemAVInfo_updates_values);
	RUN_TEST(test_setSystemAVInfo_no_audio_reinit_if_same_rate);
	RUN_TEST(test_setSystemAVInfo_null_data);
	RUN_TEST(test_setSystemAVInfo_calculates_aspect_from_geometry);

	// Audio buffer status handler
	RUN_TEST(test_setAudioBufferStatusCallback_registers);
	RUN_TEST(test_setAudioBufferStatusCallback_unregisters);

	// Controller info handler
	RUN_TEST(test_setControllerInfo_detects_dualshock);
	RUN_TEST(test_setControllerInfo_no_dualshock);
	RUN_TEST(test_setControllerInfo_null_data);

	// EnvResult helpers
	RUN_TEST(test_EnvResult_ok);
	RUN_TEST(test_EnvResult_fail);
	RUN_TEST(test_EnvResult_unhandled);

	return UNITY_END();
}
