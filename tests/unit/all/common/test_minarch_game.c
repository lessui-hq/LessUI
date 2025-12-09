/**
 * test_minarch_game.c - Unit tests for game file handling utilities
 *
 * Tests the game file loading functions extracted from minarch.c:
 * - MinArchGame_parseExtensions - Parse pipe-delimited extension list
 * - MinArchGame_matchesExtension - Match filename against extension list
 * - MinArchGame_parseZipHeader - Parse ZIP local file header
 * - MinArchGame_buildM3uPath - Build M3U path from ROM path
 *
 * These are pure functions that can be tested without file I/O mocking.
 */

// Note: MINARCH_GAME_TEST is defined via compiler flag (-DMINARCH_GAME_TEST)
// This skips the MinArchGame_detectM3uPath() function which depends on exists()

#include "../../support/unity/unity.h"
#include "minarch_game.h"

#include <string.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// MinArchGame_parseExtensions tests
///////////////////////////////

void test_parseExtensions_single_extension(void) {
	char exts[] = "gb";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_EQUAL_STRING("gb", out[0]);
	TEST_ASSERT_NULL(out[1]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_multiple_extensions(void) {
	char exts[] = "gb|gbc|dmg";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_EQUAL_STRING("gb", out[0]);
	TEST_ASSERT_EQUAL_STRING("gbc", out[1]);
	TEST_ASSERT_EQUAL_STRING("dmg", out[2]);
	TEST_ASSERT_NULL(out[3]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_with_zip_support(void) {
	char exts[] = "nes|fds|zip";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_zip_in_middle(void) {
	char exts[] = "nes|zip|fds";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(3, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_zip_only(void) {
	char exts[] = "zip";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(1, count);
	TEST_ASSERT_TRUE(supports_zip);
}

void test_parseExtensions_empty_string(void) {
	char exts[] = "";
	char* out[32];
	bool supports_zip = true; // Start true to verify it gets set false

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_string(void) {
	char* out[32];
	bool supports_zip = true;

	int count = MinArchGame_parseExtensions(NULL, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_output(void) {
	char exts[] = "gb|gbc";
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, NULL, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(0, count);
}

void test_parseExtensions_respects_max(void) {
	char exts[] = "a|b|c|d|e|f|g|h|i|j";
	char* out[5];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 5, &supports_zip);

	TEST_ASSERT_EQUAL_INT(5, count);
	TEST_ASSERT_EQUAL_STRING("a", out[0]);
	TEST_ASSERT_EQUAL_STRING("e", out[4]);
}

void test_parseExtensions_typical_core(void) {
	// Typical SNES core extensions
	char exts[] = "smc|sfc|swc|fig|bs|st|bin";
	char* out[32];
	bool supports_zip = false;

	int count = MinArchGame_parseExtensions(exts, out, 32, &supports_zip);

	TEST_ASSERT_EQUAL_INT(7, count);
	TEST_ASSERT_EQUAL_STRING("smc", out[0]);
	TEST_ASSERT_EQUAL_STRING("bin", out[6]);
	TEST_ASSERT_FALSE(supports_zip);
}

void test_parseExtensions_null_supports_zip_pointer(void) {
	char exts[] = "gb|zip";
	char* out[32];

	// Should not crash when supports_zip is NULL
	int count = MinArchGame_parseExtensions(exts, out, 32, NULL);

	TEST_ASSERT_EQUAL_INT(2, count);
}

///////////////////////////////
// MinArchGame_matchesExtension tests
///////////////////////////////

void test_matchesExtension_exact_match(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.gb", extensions));
}

void test_matchesExtension_second_extension(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.gbc", extensions));
}

void test_matchesExtension_last_extension(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.dmg", extensions));
}

void test_matchesExtension_no_match(void) {
	char* extensions[] = {"gb", "gbc", "dmg", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.nes", extensions));
}

void test_matchesExtension_case_insensitive(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.GB", extensions));
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.GBC", extensions));
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.Gb", extensions));
}

void test_matchesExtension_no_extension(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game", extensions));
}

void test_matchesExtension_dot_only(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension(".", extensions));
}

void test_matchesExtension_hidden_file_with_ext(void) {
	char* extensions[] = {"gb", "gbc", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension(".hidden.gb", extensions));
}

void test_matchesExtension_empty_extensions(void) {
	char* extensions[] = {NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.gb", extensions));
}

void test_matchesExtension_null_filename(void) {
	char* extensions[] = {"gb", NULL};
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension(NULL, extensions));
}

void test_matchesExtension_null_extensions(void) {
	TEST_ASSERT_FALSE(MinArchGame_matchesExtension("game.gb", NULL));
}

void test_matchesExtension_path_with_extension(void) {
	char* extensions[] = {"cue", "bin", NULL};
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("/path/to/game/disc.cue", extensions));
}

void test_matchesExtension_double_extension(void) {
	// p8.png is a special PICO-8 format
	char* extensions[] = {"png", "p8", NULL};
	// Should match "png" (the actual extension)
	TEST_ASSERT_TRUE(MinArchGame_matchesExtension("game.p8.png", extensions));
}

///////////////////////////////
// MinArchGame_parseZipHeader tests
///////////////////////////////

void test_parseZipHeader_stored_file(void) {
	// ZIP local file header for a stored (uncompressed) file
	// Signature: PK\x03\x04
	// Version needed: 10
	// General purpose flag: 0
	// Compression method: 0 (store)
	// ... other fields ...
	// Compressed size: 0x1234 (4660)
	// Filename length: 8
	// Extra field length: 0
	uint8_t header[MINARCH_ZIP_HEADER_SIZE] = {
	    0x50, 0x4b, 0x03, 0x04, // signature
	    0x0a, 0x00,             // version needed
	    0x00, 0x00,             // flags (no data descriptor)
	    0x00, 0x00,             // compression method: stored
	    0x00, 0x00,             // mod time
	    0x00, 0x00,             // mod date
	    0x00, 0x00, 0x00, 0x00, // CRC-32
	    0x34, 0x12, 0x00, 0x00, // compressed size: 0x1234
	    0x34, 0x12, 0x00, 0x00, // uncompressed size
	    0x08, 0x00,             // filename length: 8
	    0x00, 0x00              // extra field length: 0
	};

	uint16_t compression, filename_len, extra_len;
	uint32_t compressed_size;

	bool result =
	    MinArchGame_parseZipHeader(header, &compression, &filename_len, &compressed_size, &extra_len);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_UINT16(0, compression);       // stored
	TEST_ASSERT_EQUAL_UINT16(8, filename_len);      // filename length
	TEST_ASSERT_EQUAL_UINT32(0x1234, compressed_size); // 4660 bytes
	TEST_ASSERT_EQUAL_UINT16(0, extra_len);
}

void test_parseZipHeader_deflate_file(void) {
	uint8_t header[MINARCH_ZIP_HEADER_SIZE] = {
	    0x50, 0x4b, 0x03, 0x04, // signature
	    0x14, 0x00,             // version needed
	    0x00, 0x00,             // flags
	    0x08, 0x00,             // compression method: deflate
	    0x00, 0x00,             // mod time
	    0x00, 0x00,             // mod date
	    0x00, 0x00, 0x00, 0x00, // CRC-32
	    0xff, 0x00, 0x01, 0x00, // compressed size: 0x100ff
	    0x00, 0x02, 0x00, 0x00, // uncompressed size
	    0x0c, 0x00,             // filename length: 12
	    0x10, 0x00              // extra field length: 16
	};

	uint16_t compression, filename_len, extra_len;
	uint32_t compressed_size;

	bool result =
	    MinArchGame_parseZipHeader(header, &compression, &filename_len, &compressed_size, &extra_len);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_UINT16(8, compression);           // deflate
	TEST_ASSERT_EQUAL_UINT16(12, filename_len);         // filename length
	TEST_ASSERT_EQUAL_UINT32(0x100ff, compressed_size); // compressed size
	TEST_ASSERT_EQUAL_UINT16(16, extra_len);            // extra field length
}

void test_parseZipHeader_data_descriptor_flag(void) {
	// Header with data descriptor flag set (bit 3 of flags)
	uint8_t header[MINARCH_ZIP_HEADER_SIZE] = {
	    0x50, 0x4b, 0x03, 0x04, // signature
	    0x14, 0x00,             // version needed
	    0x08, 0x00,             // flags: bit 3 set (data descriptor)
	    0x08, 0x00,             // compression method
	    0x00, 0x00,             // mod time
	    0x00, 0x00,             // mod date
	    0x00, 0x00, 0x00, 0x00, // CRC-32 (invalid - in data descriptor)
	    0x00, 0x00, 0x00, 0x00, // compressed size (invalid)
	    0x00, 0x00, 0x00, 0x00, // uncompressed size (invalid)
	    0x08, 0x00,             // filename length
	    0x00, 0x00              // extra field length
	};

	uint16_t compression, filename_len, extra_len;
	uint32_t compressed_size;

	bool result =
	    MinArchGame_parseZipHeader(header, &compression, &filename_len, &compressed_size, &extra_len);

	TEST_ASSERT_FALSE(result); // Should reject headers with data descriptor
}

void test_parseZipHeader_null_header(void) {
	uint16_t compression;
	bool result = MinArchGame_parseZipHeader(NULL, &compression, NULL, NULL, NULL);
	TEST_ASSERT_FALSE(result);
}

void test_parseZipHeader_null_outputs(void) {
	uint8_t header[MINARCH_ZIP_HEADER_SIZE] = {
	    0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00};

	// Should not crash with NULL output pointers
	bool result = MinArchGame_parseZipHeader(header, NULL, NULL, NULL, NULL);
	TEST_ASSERT_TRUE(result);
}

void test_parseZipHeader_large_sizes(void) {
	// Header with large file sizes (close to 4GB limit)
	uint8_t header[MINARCH_ZIP_HEADER_SIZE] = {
	    0x50, 0x4b, 0x03, 0x04, // signature
	    0x14, 0x00,             // version needed
	    0x00, 0x00,             // flags
	    0x00, 0x00,             // compression: stored
	    0x00, 0x00,             // mod time
	    0x00, 0x00,             // mod date
	    0x00, 0x00, 0x00, 0x00, // CRC-32
	    0xff, 0xff, 0xff, 0x7f, // compressed size: 0x7fffffff (2GB)
	    0xff, 0xff, 0xff, 0x7f, // uncompressed size
	    0xff, 0x00,             // filename length: 255
	    0xff, 0x7f              // extra field length: 32767
	};

	uint16_t compression, filename_len, extra_len;
	uint32_t compressed_size;

	bool result =
	    MinArchGame_parseZipHeader(header, &compression, &filename_len, &compressed_size, &extra_len);

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_UINT32(0x7fffffff, compressed_size);
	TEST_ASSERT_EQUAL_UINT16(255, filename_len);
	TEST_ASSERT_EQUAL_UINT16(0x7fff, extra_len);
}

///////////////////////////////
// MinArchGame_buildM3uPath tests
///////////////////////////////

void test_buildM3uPath_typical_disc(void) {
	char out[256];
	bool result =
	    MinArchGame_buildM3uPath("/Roms/PS/Game (Disc 1)/image.cue", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS/Game (Disc 1).m3u", out);
}

void test_buildM3uPath_simple_path(void) {
	char out[256];
	bool result =
	    MinArchGame_buildM3uPath("/path/to/folder/file.bin", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/path/to/folder.m3u", out);
}

void test_buildM3uPath_deep_path(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(
	    "/mnt/SDCARD/Roms/PlayStation/Game Name (USA) (Disc 1)/disc.cue", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/PlayStation/Game Name (USA) (Disc 1).m3u", out);
}

void test_buildM3uPath_special_chars(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(
	    "/Roms/PS/Game - Title (USA) [Rev 1]/track01.bin", out, sizeof(out));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING("/Roms/PS/Game - Title (USA) [Rev 1].m3u", out);
}

void test_buildM3uPath_null_rom_path(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath(NULL, out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_null_output(void) {
	bool result = MinArchGame_buildM3uPath("/path/to/file.bin", NULL, 256);
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_zero_size(void) {
	char out[256];
	bool result = MinArchGame_buildM3uPath("/path/to/file.bin", out, 0);
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_buffer_too_small(void) {
	char out[10]; // Too small for the result
	bool result = MinArchGame_buildM3uPath("/path/to/folder/file.bin", out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_single_component(void) {
	char out[256];
	// Path with only one component - should fail (no parent directory)
	bool result = MinArchGame_buildM3uPath("/file.bin", out, sizeof(out));
	TEST_ASSERT_FALSE(result);
}

void test_buildM3uPath_root_dir(void) {
	char out[256];
	// ROM in root directory
	bool result = MinArchGame_buildM3uPath("/folder/file.bin", out, sizeof(out));
	// Should fail - can't go above root
	TEST_ASSERT_FALSE(result);
}

///////////////////////////////
// ZIP LE macros tests
///////////////////////////////

void test_zip_le_read16_typical(void) {
	uint8_t buf[] = {0x34, 0x12}; // Little-endian 0x1234
	TEST_ASSERT_EQUAL_UINT16(0x1234, MINARCH_ZIP_LE_READ16(buf));
}

void test_zip_le_read16_zero(void) {
	uint8_t buf[] = {0x00, 0x00};
	TEST_ASSERT_EQUAL_UINT16(0, MINARCH_ZIP_LE_READ16(buf));
}

void test_zip_le_read16_max(void) {
	uint8_t buf[] = {0xff, 0xff};
	TEST_ASSERT_EQUAL_UINT16(0xffff, MINARCH_ZIP_LE_READ16(buf));
}

void test_zip_le_read32_typical(void) {
	uint8_t buf[] = {0x78, 0x56, 0x34, 0x12}; // Little-endian 0x12345678
	TEST_ASSERT_EQUAL_UINT32(0x12345678, MINARCH_ZIP_LE_READ32(buf));
}

void test_zip_le_read32_zero(void) {
	uint8_t buf[] = {0x00, 0x00, 0x00, 0x00};
	TEST_ASSERT_EQUAL_UINT32(0, MINARCH_ZIP_LE_READ32(buf));
}

void test_zip_le_read32_max(void) {
	uint8_t buf[] = {0xff, 0xff, 0xff, 0xff};
	TEST_ASSERT_EQUAL_UINT32(0xffffffff, MINARCH_ZIP_LE_READ32(buf));
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// parseExtensions tests
	RUN_TEST(test_parseExtensions_single_extension);
	RUN_TEST(test_parseExtensions_multiple_extensions);
	RUN_TEST(test_parseExtensions_with_zip_support);
	RUN_TEST(test_parseExtensions_zip_in_middle);
	RUN_TEST(test_parseExtensions_zip_only);
	RUN_TEST(test_parseExtensions_empty_string);
	RUN_TEST(test_parseExtensions_null_string);
	RUN_TEST(test_parseExtensions_null_output);
	RUN_TEST(test_parseExtensions_respects_max);
	RUN_TEST(test_parseExtensions_typical_core);
	RUN_TEST(test_parseExtensions_null_supports_zip_pointer);

	// matchesExtension tests
	RUN_TEST(test_matchesExtension_exact_match);
	RUN_TEST(test_matchesExtension_second_extension);
	RUN_TEST(test_matchesExtension_last_extension);
	RUN_TEST(test_matchesExtension_no_match);
	RUN_TEST(test_matchesExtension_case_insensitive);
	RUN_TEST(test_matchesExtension_no_extension);
	RUN_TEST(test_matchesExtension_dot_only);
	RUN_TEST(test_matchesExtension_hidden_file_with_ext);
	RUN_TEST(test_matchesExtension_empty_extensions);
	RUN_TEST(test_matchesExtension_null_filename);
	RUN_TEST(test_matchesExtension_null_extensions);
	RUN_TEST(test_matchesExtension_path_with_extension);
	RUN_TEST(test_matchesExtension_double_extension);

	// parseZipHeader tests
	RUN_TEST(test_parseZipHeader_stored_file);
	RUN_TEST(test_parseZipHeader_deflate_file);
	RUN_TEST(test_parseZipHeader_data_descriptor_flag);
	RUN_TEST(test_parseZipHeader_null_header);
	RUN_TEST(test_parseZipHeader_null_outputs);
	RUN_TEST(test_parseZipHeader_large_sizes);

	// buildM3uPath tests
	RUN_TEST(test_buildM3uPath_typical_disc);
	RUN_TEST(test_buildM3uPath_simple_path);
	RUN_TEST(test_buildM3uPath_deep_path);
	RUN_TEST(test_buildM3uPath_special_chars);
	RUN_TEST(test_buildM3uPath_null_rom_path);
	RUN_TEST(test_buildM3uPath_null_output);
	RUN_TEST(test_buildM3uPath_zero_size);
	RUN_TEST(test_buildM3uPath_buffer_too_small);
	RUN_TEST(test_buildM3uPath_single_component);
	RUN_TEST(test_buildM3uPath_root_dir);

	// ZIP LE macro tests
	RUN_TEST(test_zip_le_read16_typical);
	RUN_TEST(test_zip_le_read16_zero);
	RUN_TEST(test_zip_le_read16_max);
	RUN_TEST(test_zip_le_read32_typical);
	RUN_TEST(test_zip_le_read32_zero);
	RUN_TEST(test_zip_le_read32_max);

	return UNITY_END();
}
