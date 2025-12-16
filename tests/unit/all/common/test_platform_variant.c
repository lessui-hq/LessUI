/**
 * test_platform_variant.c - Unit tests for platform variant detection
 *
 * Tests the platform variant system that provides runtime hardware detection.
 * Uses test-specific platform definitions.
 *
 * Test coverage:
 * - PLAT_getDeviceName - Device name formatting
 * - PlatformVariant structure - Field initialization
 * - VARIANT_IS macro - Variant checking
 * - HAS_FEATURE macro - Feature flag checking
 */

#include "unity.h"
#include "../../../../workspace/all/common/platform_variant.h"
#include "platform.h" // For PLATFORM, FIXED_WIDTH, etc.
#include <string.h>

// Test device registry
static const DeviceInfo test_device_full = {
    .device_id = "test_device",
    .display_name = "Test Device",
    .manufacturer = "TestCo"
};

static const DeviceInfo test_device_no_manufacturer = {
    .device_id = "simple",
    .display_name = "Simple Device",
    .manufacturer = NULL
};

void setUp(void) {
	// Reset platform_variant to known state
	memset(&platform_variant, 0, sizeof(PlatformVariant));
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// PLAT_getDeviceName tests
///////////////////////////////

void test_getDeviceName_with_manufacturer(void) {
	platform_variant.device = &test_device_full;

	const char* name = PLAT_getDeviceName();
	TEST_ASSERT_EQUAL_STRING("TestCo Test Device", name);
}

void test_getDeviceName_without_manufacturer(void) {
	platform_variant.device = &test_device_no_manufacturer;

	const char* name = PLAT_getDeviceName();
	TEST_ASSERT_EQUAL_STRING("Simple Device", name);
}

void test_getDeviceName_null_device(void) {
	platform_variant.device = NULL;

	const char* name = PLAT_getDeviceName();
	TEST_ASSERT_EQUAL_STRING("Unknown Device", name);
}

///////////////////////////////
// PlatformVariant structure tests
///////////////////////////////

void test_platform_variant_initial_state(void) {
	// After setUp, should be zeroed
	TEST_ASSERT_NULL(platform_variant.platform);
	TEST_ASSERT_EQUAL_INT(VARIANT_NONE, platform_variant.variant);
	TEST_ASSERT_NULL(platform_variant.device);
	TEST_ASSERT_EQUAL_INT(0, platform_variant.screen_width);
	TEST_ASSERT_EQUAL_INT(0, platform_variant.screen_height);
	TEST_ASSERT_EQUAL_INT(0, platform_variant.has_hdmi);
	TEST_ASSERT_EQUAL_INT(0, platform_variant.hw_features);
}

void test_platform_variant_set_fields(void) {
	platform_variant.platform = "miyoomini";
	platform_variant.variant = VARIANT_STANDARD;
	platform_variant.screen_width = 640;
	platform_variant.screen_height = 480;
	platform_variant.has_hdmi = 0;
	platform_variant.hw_features = HW_FEATURE_NEON;

	TEST_ASSERT_EQUAL_STRING("miyoomini", platform_variant.platform);
	TEST_ASSERT_EQUAL_INT(VARIANT_STANDARD, platform_variant.variant);
	TEST_ASSERT_EQUAL_INT(640, platform_variant.screen_width);
	TEST_ASSERT_EQUAL_INT(480, platform_variant.screen_height);
	TEST_ASSERT_EQUAL_INT(0, platform_variant.has_hdmi);
	TEST_ASSERT_TRUE(platform_variant.hw_features & HW_FEATURE_NEON);
}

///////////////////////////////
// VARIANT_IS macro tests
///////////////////////////////

void test_VARIANT_IS_matches_correct_variant(void) {
	platform_variant.variant = VARIANT_STANDARD;
	TEST_ASSERT_TRUE(VARIANT_IS(VARIANT_STANDARD));
}

void test_VARIANT_IS_returns_false_for_different_variant(void) {
	platform_variant.variant = VARIANT_STANDARD;
	TEST_ASSERT_FALSE(VARIANT_IS(VARIANT_ALTERNATE));
}

void test_VARIANT_IS_with_none(void) {
	platform_variant.variant = VARIANT_NONE;
	TEST_ASSERT_TRUE(VARIANT_IS(VARIANT_NONE));
	TEST_ASSERT_FALSE(VARIANT_IS(VARIANT_STANDARD));
}

///////////////////////////////
// HAS_FEATURE macro tests
///////////////////////////////

void test_HAS_FEATURE_single_flag(void) {
	platform_variant.hw_features = HW_FEATURE_NEON;
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_NEON));
	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_ANALOG));
}

void test_HAS_FEATURE_multiple_flags(void) {
	platform_variant.hw_features = HW_FEATURE_NEON | HW_FEATURE_ANALOG | HW_FEATURE_RUMBLE;

	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_NEON));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_ANALOG));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_RUMBLE));
	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_LID));
	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_PMIC));
}

void test_HAS_FEATURE_no_flags(void) {
	platform_variant.hw_features = 0;

	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_NEON));
	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_ANALOG));
	TEST_ASSERT_FALSE(HAS_FEATURE(HW_FEATURE_LID));
}

void test_HAS_FEATURE_all_flags(void) {
	platform_variant.hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE |
	                               HW_FEATURE_PMIC | HW_FEATURE_ANALOG | HW_FEATURE_VOLUME_HW;

	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_NEON));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_LID));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_RUMBLE));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_PMIC));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_ANALOG));
	TEST_ASSERT_TRUE(HAS_FEATURE(HW_FEATURE_VOLUME_HW));
}

///////////////////////////////
// PLAT_detectVariant tests (weak fallback implementation)
///////////////////////////////

void test_PLAT_detectVariant_sets_platform(void) {
	PlatformVariant v = {0};
	PLAT_detectVariant(&v);
	TEST_ASSERT_EQUAL_STRING(PLATFORM, v.platform);
}

void test_PLAT_detectVariant_sets_variant_standard(void) {
	PlatformVariant v = {0};
	PLAT_detectVariant(&v);
	TEST_ASSERT_EQUAL_INT(VARIANT_STANDARD, v.variant);
}

void test_PLAT_detectVariant_sets_screen_dimensions(void) {
	PlatformVariant v = {0};
	PLAT_detectVariant(&v);
	TEST_ASSERT_EQUAL_INT(FIXED_WIDTH, v.screen_width);
	TEST_ASSERT_EQUAL_INT(FIXED_HEIGHT, v.screen_height);
}

void test_PLAT_detectVariant_null_device(void) {
	PlatformVariant v = {0};
	PLAT_detectVariant(&v);
	TEST_ASSERT_NULL(v.device);
}

///////////////////////////////
// DeviceInfo structure tests
///////////////////////////////

void test_DeviceInfo_fields(void) {
	TEST_ASSERT_EQUAL_STRING("test_device", test_device_full.device_id);
	TEST_ASSERT_EQUAL_STRING("Test Device", test_device_full.display_name);
	TEST_ASSERT_EQUAL_STRING("TestCo", test_device_full.manufacturer);
}

///////////////////////////////
// VariantType enumeration tests
///////////////////////////////

void test_VariantType_values(void) {
	TEST_ASSERT_EQUAL_INT(0, VARIANT_NONE);
	TEST_ASSERT_EQUAL_INT(1, VARIANT_STANDARD);
	TEST_ASSERT_EQUAL_INT(2, VARIANT_ALTERNATE);
	TEST_ASSERT_EQUAL_INT(100, VARIANT_PLATFORM_BASE);
}

///////////////////////////////
// Main
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// PLAT_getDeviceName
	RUN_TEST(test_getDeviceName_with_manufacturer);
	RUN_TEST(test_getDeviceName_without_manufacturer);
	RUN_TEST(test_getDeviceName_null_device);

	// PlatformVariant structure
	RUN_TEST(test_platform_variant_initial_state);
	RUN_TEST(test_platform_variant_set_fields);

	// VARIANT_IS macro
	RUN_TEST(test_VARIANT_IS_matches_correct_variant);
	RUN_TEST(test_VARIANT_IS_returns_false_for_different_variant);
	RUN_TEST(test_VARIANT_IS_with_none);

	// HAS_FEATURE macro
	RUN_TEST(test_HAS_FEATURE_single_flag);
	RUN_TEST(test_HAS_FEATURE_multiple_flags);
	RUN_TEST(test_HAS_FEATURE_no_flags);
	RUN_TEST(test_HAS_FEATURE_all_flags);

	// PLAT_detectVariant (weak fallback)
	RUN_TEST(test_PLAT_detectVariant_sets_platform);
	RUN_TEST(test_PLAT_detectVariant_sets_variant_standard);
	RUN_TEST(test_PLAT_detectVariant_sets_screen_dimensions);
	RUN_TEST(test_PLAT_detectVariant_null_device);

	// DeviceInfo structure
	RUN_TEST(test_DeviceInfo_fields);

	// VariantType enumeration
	RUN_TEST(test_VariantType_values);

	return UNITY_END();
}
