/**
 * test_minarch_core.c - Unit tests for core AV info processing
 *
 * Tests the pure functions for processing libretro core audio/video
 * information structures.
 *
 * Test coverage:
 * - MinArchCore_buildGameInfo - Build game info from Game struct
 * - MinArchCore_calculateAspectRatio - Aspect ratio calculation
 * - MinArchCore_processAVInfo - Full AV info processing
 */

#include "../../support/unity/unity.h"
#include "minarch_core.h"

#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArchCore_buildGameInfo tests
///////////////////////////////

void test_buildGameInfo_normal_path(void) {
	struct Game game = {0};
	strcpy((char*)game.path, "/path/to/game.nes");
	game.data = (void*)0x1234;
	game.size = 0x10000;

	struct retro_game_info info = {0};
	MinArchCore_buildGameInfo(&game, &info);

	TEST_ASSERT_EQUAL_STRING("/path/to/game.nes", info.path);
	TEST_ASSERT_EQUAL_PTR((void*)0x1234, info.data);
	TEST_ASSERT_EQUAL_UINT32(0x10000, info.size);
	TEST_ASSERT_NULL(info.meta);
}

void test_buildGameInfo_with_tmp_path(void) {
	struct Game game = {0};
	strcpy((char*)game.path, "/path/to/game.zip");
	strcpy(game.tmp_path, "/tmp/minarch-abc123/game.nes");
	game.data = (void*)0x5678;
	game.size = 0x20000;

	struct retro_game_info info = {0};
	MinArchCore_buildGameInfo(&game, &info);

	// Should use tmp_path since it's set
	TEST_ASSERT_EQUAL_STRING("/tmp/minarch-abc123/game.nes", info.path);
	TEST_ASSERT_EQUAL_PTR((void*)0x5678, info.data);
	TEST_ASSERT_EQUAL_UINT32(0x20000, info.size);
}

void test_buildGameInfo_empty_tmp_path(void) {
	struct Game game = {0};
	strcpy((char*)game.path, "/path/to/game.gb");
	game.tmp_path[0] = '\0'; // Empty tmp_path
	game.data = NULL;
	game.size = 0;

	struct retro_game_info info = {0};
	MinArchCore_buildGameInfo(&game, &info);

	// Should use original path when tmp_path is empty
	TEST_ASSERT_EQUAL_STRING("/path/to/game.gb", info.path);
	TEST_ASSERT_NULL(info.data);
	TEST_ASSERT_EQUAL_UINT32(0, info.size);
}

void test_buildGameInfo_null_game(void) {
	struct retro_game_info info = {0};
	info.path = "should not change";

	MinArchCore_buildGameInfo(NULL, &info);

	// Should not crash, should not modify output
	TEST_ASSERT_EQUAL_STRING("should not change", info.path);
}

void test_buildGameInfo_null_output(void) {
	struct Game game = {0};
	strcpy((char*)game.path, "/path/to/game.nes");

	// Should not crash
	MinArchCore_buildGameInfo(&game, NULL);
}

///////////////////////////////
// MinArchCore_calculateAspectRatio tests
///////////////////////////////

void test_calculateAspectRatio_provided(void) {
	double result = MinArchCore_calculateAspectRatio(1.5, 256, 224);
	TEST_ASSERT_EQUAL_INT(150, (int)(result * 100));
}

void test_calculateAspectRatio_4_3_provided(void) {
	double result = MinArchCore_calculateAspectRatio(4.0 / 3.0, 320, 240);
	// Use integer comparison for floating point
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(result * 100));
}

void test_calculateAspectRatio_zero_calculates(void) {
	double result = MinArchCore_calculateAspectRatio(0, 256, 224);
	// Should calculate: 256/224 = 1.142857
	TEST_ASSERT_INT_WITHIN(1, 114, (int)(result * 100));
}

void test_calculateAspectRatio_negative_calculates(void) {
	double result = MinArchCore_calculateAspectRatio(-1.0, 320, 240);
	// Should calculate: 320/240 = 1.333
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(result * 100));
}

void test_calculateAspectRatio_square(void) {
	double result = MinArchCore_calculateAspectRatio(0, 256, 256);
	TEST_ASSERT_EQUAL_INT(100, (int)(result * 100));
}

void test_calculateAspectRatio_wide(void) {
	double result = MinArchCore_calculateAspectRatio(0, 640, 240);
	// 640/240 = 2.666...
	TEST_ASSERT_INT_WITHIN(1, 267, (int)(result * 100));
}

void test_calculateAspectRatio_tall(void) {
	double result = MinArchCore_calculateAspectRatio(0, 240, 640);
	// 240/640 = 0.375
	TEST_ASSERT_INT_WITHIN(1, 38, (int)(result * 100));
}

void test_calculateAspectRatio_zero_height(void) {
	double result = MinArchCore_calculateAspectRatio(0, 256, 0);
	// Should return fallback of 1.0
	TEST_ASSERT_EQUAL_INT(100, (int)(result * 100));
}

void test_calculateAspectRatio_snes(void) {
	// Typical SNES: 256x224, provided aspect 4:3
	double result = MinArchCore_calculateAspectRatio(4.0 / 3.0, 256, 224);
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(result * 100));
}

void test_calculateAspectRatio_genesis(void) {
	// Genesis: 320x224, no provided aspect
	double result = MinArchCore_calculateAspectRatio(0, 320, 224);
	// 320/224 = 1.428571
	TEST_ASSERT_INT_WITHIN(1, 143, (int)(result * 100));
}

void test_calculateAspectRatio_gba(void) {
	// GBA: 240x160, aspect 3:2
	double result = MinArchCore_calculateAspectRatio(3.0 / 2.0, 240, 160);
	TEST_ASSERT_EQUAL_INT(150, (int)(result * 100));
}

///////////////////////////////
// MinArchCore_processAVInfo tests
///////////////////////////////

void test_processAVInfo_typical_snes(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 60.0988;
	av.timing.sample_rate = 32040.5;
	av.geometry.base_width = 256;
	av.geometry.base_height = 224;
	av.geometry.aspect_ratio = 4.0 / 3.0;

	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av, &info);

	TEST_ASSERT_INT_WITHIN(1, 6010, (int)(info.fps * 100));
	TEST_ASSERT_INT_WITHIN(5, 3204050, (int)(info.sample_rate * 100));
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(info.aspect_ratio * 100));
}

void test_processAVInfo_no_aspect_provided(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 60.0;
	av.timing.sample_rate = 48000.0;
	av.geometry.base_width = 320;
	av.geometry.base_height = 240;
	av.geometry.aspect_ratio = 0; // Not provided

	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av, &info);

	TEST_ASSERT_EQUAL_INT(6000, (int)(info.fps * 100));
	TEST_ASSERT_EQUAL_INT(4800000, (int)(info.sample_rate * 100));
	// Should calculate: 320/240 = 1.333
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(info.aspect_ratio * 100));
}

void test_processAVInfo_psx(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 59.826;
	av.timing.sample_rate = 44100.0;
	av.geometry.base_width = 320;
	av.geometry.base_height = 240;
	av.geometry.aspect_ratio = 4.0 / 3.0;

	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av, &info);

	TEST_ASSERT_INT_WITHIN(1, 5983, (int)(info.fps * 100));
	TEST_ASSERT_INT_WITHIN(5, 4410000, (int)(info.sample_rate * 100));
	TEST_ASSERT_INT_WITHIN(1, 133, (int)(info.aspect_ratio * 100));
}

void test_processAVInfo_gba(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 59.727;
	av.timing.sample_rate = 32768.0;
	av.geometry.base_width = 240;
	av.geometry.base_height = 160;
	av.geometry.aspect_ratio = 3.0 / 2.0;

	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av, &info);

	TEST_ASSERT_INT_WITHIN(1, 5973, (int)(info.fps * 100));
	TEST_ASSERT_INT_WITHIN(5, 3276800, (int)(info.sample_rate * 100));
	TEST_ASSERT_EQUAL_INT(150, (int)(info.aspect_ratio * 100));
}

void test_processAVInfo_nes_pal(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 50.0070; // PAL
	av.timing.sample_rate = 48000.0;
	av.geometry.base_width = 256;
	av.geometry.base_height = 240;
	av.geometry.aspect_ratio = 0; // FCEUmm doesn't provide aspect

	MinArchCoreAVInfo info;
	MinArchCore_processAVInfo(&av, &info);

	TEST_ASSERT_INT_WITHIN(1, 5001, (int)(info.fps * 100));
	// Aspect calculated from geometry
	TEST_ASSERT_INT_WITHIN(1, 107, (int)(info.aspect_ratio * 100));
}

void test_processAVInfo_null_input(void) {
	MinArchCoreAVInfo info = {.fps = 99.9, .sample_rate = 99999, .aspect_ratio = 9.9};

	MinArchCore_processAVInfo(NULL, &info);

	// Should not crash, should not modify output
	TEST_ASSERT_INT_WITHIN(1, 9990, (int)(info.fps * 100));
}

void test_processAVInfo_null_output(void) {
	struct retro_system_av_info av = {0};
	av.timing.fps = 60.0;

	// Should not crash
	MinArchCore_processAVInfo(&av, NULL);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// buildGameInfo tests
	RUN_TEST(test_buildGameInfo_normal_path);
	RUN_TEST(test_buildGameInfo_with_tmp_path);
	RUN_TEST(test_buildGameInfo_empty_tmp_path);
	RUN_TEST(test_buildGameInfo_null_game);
	RUN_TEST(test_buildGameInfo_null_output);

	// calculateAspectRatio tests
	RUN_TEST(test_calculateAspectRatio_provided);
	RUN_TEST(test_calculateAspectRatio_4_3_provided);
	RUN_TEST(test_calculateAspectRatio_zero_calculates);
	RUN_TEST(test_calculateAspectRatio_negative_calculates);
	RUN_TEST(test_calculateAspectRatio_square);
	RUN_TEST(test_calculateAspectRatio_wide);
	RUN_TEST(test_calculateAspectRatio_tall);
	RUN_TEST(test_calculateAspectRatio_zero_height);
	RUN_TEST(test_calculateAspectRatio_snes);
	RUN_TEST(test_calculateAspectRatio_genesis);
	RUN_TEST(test_calculateAspectRatio_gba);

	// processAVInfo tests
	RUN_TEST(test_processAVInfo_typical_snes);
	RUN_TEST(test_processAVInfo_no_aspect_provided);
	RUN_TEST(test_processAVInfo_psx);
	RUN_TEST(test_processAVInfo_gba);
	RUN_TEST(test_processAVInfo_nes_pal);
	RUN_TEST(test_processAVInfo_null_input);
	RUN_TEST(test_processAVInfo_null_output);

	return UNITY_END();
}
