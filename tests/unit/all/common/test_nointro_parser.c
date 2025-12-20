/**
 * test_nointro_parser.c - Tests for No-Intro ROM name parser
 *
 * Tests the parsing of No-Intro naming convention into structured data.
 * https://wiki.no-intro.org/index.php?title=Naming_Convention
 */

#include "../../../../workspace/all/common/nointro_parser.h"
#include "../../../../workspace/all/common/utils.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// Basic Parsing Tests
///////////////////////////////

void test_parseNoIntroName_simple_no_tags(void) {
	NoIntroName info;
	parseNoIntroName("Super Mario Bros.nes", &info);

	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", info.title);
	TEST_ASSERT_EQUAL_STRING("Super Mario Bros", info.display_name);
	TEST_ASSERT_EQUAL_STRING("", info.region);
	TEST_ASSERT_EQUAL_STRING("", info.language);
	TEST_ASSERT_FALSE(info.has_tags);
}

void test_parseNoIntroName_with_extension(void) {
	NoIntroName info;
	parseNoIntroName("Tetris.gb", &info);

	TEST_ASSERT_EQUAL_STRING("Tetris", info.title);
	TEST_ASSERT_EQUAL_STRING("Tetris", info.display_name);
}

void test_parseNoIntroName_multipart_extension(void) {
	NoIntroName info;
	parseNoIntroName("Celeste.p8.png", &info);

	TEST_ASSERT_EQUAL_STRING("Celeste", info.title);
	TEST_ASSERT_EQUAL_STRING("Celeste", info.display_name);
}

///////////////////////////////
// Region Parsing Tests
///////////////////////////////

void test_parseNoIntroName_single_region(void) {
	NoIntroName info;
	parseNoIntroName("Super Metroid (USA).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("Super Metroid", info.title);
	TEST_ASSERT_EQUAL_STRING("Super Metroid", info.display_name);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_TRUE(info.has_tags);
}

void test_parseNoIntroName_multi_region(void) {
	NoIntroName info;
	parseNoIntroName("Game (Japan, USA).gb", &info);

	TEST_ASSERT_EQUAL_STRING("Game", info.title);
	TEST_ASSERT_EQUAL_STRING("Japan, USA", info.region);
}

void test_parseNoIntroName_world_region(void) {
	NoIntroName info;
	parseNoIntroName("Tetris (World).gb", &info);

	TEST_ASSERT_EQUAL_STRING("Tetris", info.title);
	TEST_ASSERT_EQUAL_STRING("World", info.region);
}

void test_parseNoIntroName_europe_region(void) {
	NoIntroName info;
	parseNoIntroName("Sonic (Europe).md", &info);

	TEST_ASSERT_EQUAL_STRING("Sonic", info.title);
	TEST_ASSERT_EQUAL_STRING("Europe", info.region);
}

///////////////////////////////
// Language Parsing Tests
///////////////////////////////

void test_parseNoIntroName_single_language(void) {
	NoIntroName info;
	parseNoIntroName("Game (Europe) (En).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Game", info.title);
	TEST_ASSERT_EQUAL_STRING("Europe", info.region);
	TEST_ASSERT_EQUAL_STRING("En", info.language);
}

void test_parseNoIntroName_multi_language(void) {
	NoIntroName info;
	parseNoIntroName("Super Metroid (Japan, USA) (En,Ja).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("Super Metroid", info.title);
	TEST_ASSERT_EQUAL_STRING("Japan, USA", info.region);
	TEST_ASSERT_EQUAL_STRING("En,Ja", info.language);
}

void test_parseNoIntroName_three_languages(void) {
	NoIntroName info;
	parseNoIntroName("Game (Europe) (En,Fr,De).nes", &info);

	TEST_ASSERT_EQUAL_STRING("En,Fr,De", info.language);
}

///////////////////////////////
// Version Parsing Tests
///////////////////////////////

void test_parseNoIntroName_version(void) {
	NoIntroName info;
	parseNoIntroName("Mario Kart (USA) (v1.2).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("Mario Kart", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	// Debug: check if version went to additional
	if (info.version[0] == '\0' && info.additional[0] != '\0') {
		printf("\nDEBUG: Version tag '%s' went to additional instead of version\n", info.additional);
	}
	TEST_ASSERT_EQUAL_STRING("v1.2", info.version);
}

void test_parseNoIntroName_revision(void) {
	NoIntroName info;
	parseNoIntroName("Pokemon Red (USA) (Rev A).gb", &info);

	TEST_ASSERT_EQUAL_STRING("Pokemon Red", info.title);
	TEST_ASSERT_EQUAL_STRING("Rev A", info.version);
}

void test_parseNoIntroName_revision_number(void) {
	NoIntroName info;
	parseNoIntroName("Zelda (USA) (Rev 1).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Rev 1", info.version);
}

///////////////////////////////
// Development Status Tests
///////////////////////////////

void test_parseNoIntroName_beta(void) {
	NoIntroName info;
	parseNoIntroName("StarFox (USA) (Beta).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("StarFox", info.title);
	TEST_ASSERT_EQUAL_STRING("Beta", info.dev_status);
}

void test_parseNoIntroName_beta_numbered(void) {
	NoIntroName info;
	parseNoIntroName("Game (USA) (Beta 2).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Beta 2", info.dev_status);
}

void test_parseNoIntroName_proto(void) {
	NoIntroName info;
	parseNoIntroName("Resident Evil (USA) (Proto).psx", &info);

	TEST_ASSERT_EQUAL_STRING("Resident Evil", info.title);
	TEST_ASSERT_EQUAL_STRING("Proto", info.dev_status);
}

void test_parseNoIntroName_sample(void) {
	NoIntroName info;
	parseNoIntroName("Demo Game (USA) (Sample).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("Sample", info.dev_status);
}

///////////////////////////////
// Status Flag Tests
///////////////////////////////

void test_parseNoIntroName_bad_dump(void) {
	NoIntroName info;
	parseNoIntroName("Rare Game (USA) [b].nes", &info);

	TEST_ASSERT_EQUAL_STRING("Rare Game", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("b", info.status);
}

void test_parseNoIntroName_verified(void) {
	NoIntroName info;
	parseNoIntroName("Perfect Dump (USA) [!].nes", &info);

	TEST_ASSERT_EQUAL_STRING("!", info.status);
}

///////////////////////////////
// Article Handling Tests
///////////////////////////////

void test_parseNoIntroName_article_the(void) {
	NoIntroName info;
	parseNoIntroName("Legend of Zelda, The (USA).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Legend of Zelda, The", info.title);
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", info.display_name);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
}

void test_parseNoIntroName_article_a(void) {
	NoIntroName info;
	parseNoIntroName("Man Born in Hell, A (USA).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Man Born in Hell, A", info.title);
	TEST_ASSERT_EQUAL_STRING("A Man Born in Hell", info.display_name);
}

void test_parseNoIntroName_article_an(void) {
	NoIntroName info;
	parseNoIntroName("Angry Bird, An (USA).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Angry Bird, An", info.title);
	TEST_ASSERT_EQUAL_STRING("An Angry Bird", info.display_name);
}

void test_parseNoIntroName_article_already_front(void) {
	NoIntroName info;
	parseNoIntroName("The Legend of Zelda (USA).nes", &info);

	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", info.title);
	TEST_ASSERT_EQUAL_STRING("The Legend of Zelda", info.display_name);
}

///////////////////////////////
// Complex Multi-Tag Tests
///////////////////////////////

void test_parseNoIntroName_all_tags(void) {
	NoIntroName info;
	parseNoIntroName("Final Fantasy, The (Japan, USA) (En,Ja) (v1.1) (Proto).sfc", &info);

	TEST_ASSERT_EQUAL_STRING("Final Fantasy, The", info.title);
	TEST_ASSERT_EQUAL_STRING("The Final Fantasy", info.display_name);
	TEST_ASSERT_EQUAL_STRING("Japan, USA", info.region);
	TEST_ASSERT_EQUAL_STRING("En,Ja", info.language);
	TEST_ASSERT_EQUAL_STRING("v1.1", info.version);
	TEST_ASSERT_EQUAL_STRING("Proto", info.dev_status);
}

void test_parseNoIntroName_complex_with_brackets(void) {
	NoIntroName info;
	parseNoIntroName("Adventure (USA) (v1.0) [!].nes", &info);

	TEST_ASSERT_EQUAL_STRING("Adventure", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("v1.0", info.version);
	TEST_ASSERT_EQUAL_STRING("!", info.status);
}

void test_parseNoIntroName_disc_number(void) {
	NoIntroName info;
	parseNoIntroName("Legend of Dragoon, The (USA) (Disc 1).bin", &info);

	TEST_ASSERT_EQUAL_STRING("Legend of Dragoon, The", info.title);
	TEST_ASSERT_EQUAL_STRING("The Legend of Dragoon", info.display_name);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("Disc 1", info.additional);
}

void test_parseNoIntroName_unlicensed(void) {
	NoIntroName info;
	parseNoIntroName("Homebrew Game (World) (Unl).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Homebrew Game", info.title);
	TEST_ASSERT_EQUAL_STRING("Unl", info.license);
}

///////////////////////////////
// Tag Order Independence Tests
///////////////////////////////

void test_parseNoIntroName_tags_different_order_1(void) {
	NoIntroName info;
	parseNoIntroName("Game (USA) (En) (v1.0).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Game", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("En", info.language);
	TEST_ASSERT_EQUAL_STRING("v1.0", info.version);
}

void test_parseNoIntroName_tags_different_order_2(void) {
	NoIntroName info;
	parseNoIntroName("Game (v1.0) (USA) (En).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Game", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("En", info.language);
	TEST_ASSERT_EQUAL_STRING("v1.0", info.version);
}

void test_parseNoIntroName_tags_different_order_3(void) {
	NoIntroName info;
	parseNoIntroName("Game (En) (v1.0) (USA).nes", &info);

	TEST_ASSERT_EQUAL_STRING("Game", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("En", info.language);
	TEST_ASSERT_EQUAL_STRING("v1.0", info.version);
}

///////////////////////////////
// Real ROM Names from FAKESD
///////////////////////////////

void test_parseNoIntroName_real_zip_file(void) {
	NoIntroName info;
	parseNoIntroName("Wario Land 3 (World) (En,Ja).zip", &info);

	TEST_ASSERT_EQUAL_STRING("Wario Land 3", info.title);
	TEST_ASSERT_EQUAL_STRING("Wario Land 3", info.display_name);
	TEST_ASSERT_EQUAL_STRING("World", info.region);
	TEST_ASSERT_EQUAL_STRING("En,Ja", info.language);
}

void test_parseNoIntroName_real_gb_compatible(void) {
	NoIntroName info;
	parseNoIntroName("Babe and Friends (USA) (GB Compatible).zip", &info);

	TEST_ASSERT_EQUAL_STRING("Babe and Friends", info.title);
	TEST_ASSERT_EQUAL_STRING("USA", info.region);
	TEST_ASSERT_EQUAL_STRING("GB Compatible", info.additional);
}

void test_parseNoIntroName_real_multi_region_zip(void) {
	NoIntroName info;
	parseNoIntroName("Star Wars - Yoda Stories (USA, Europe) (GB Compatible).zip", &info);

	TEST_ASSERT_EQUAL_STRING("Star Wars - Yoda Stories", info.title);
	TEST_ASSERT_EQUAL_STRING("USA, Europe", info.region);
}

void test_parseNoIntroName_real_five_languages(void) {
	NoIntroName info;
	parseNoIntroName("Toki Tori (USA, Europe) (En,Ja,Fr,De,Es).zip", &info);

	TEST_ASSERT_EQUAL_STRING("Toki Tori", info.title);
	TEST_ASSERT_EQUAL_STRING("USA, Europe", info.region);
	TEST_ASSERT_EQUAL_STRING("En,Ja,Fr,De,Es", info.language);
}

void test_parseNoIntroName_adventure_island(void) {
	NoIntroName info;
	parseNoIntroName("Adventure Island (USA, Europe).zip", &info);

	TEST_ASSERT_EQUAL_STRING("Adventure Island", info.title);
	TEST_ASSERT_EQUAL_STRING("Adventure Island", info.display_name);
	TEST_ASSERT_EQUAL_STRING("USA, Europe", info.region);
	TEST_ASSERT_EQUAL_STRING("", info.language);
}

///////////////////////////////
// Edge Cases
///////////////////////////////

void test_parseNoIntroName_empty_string(void) {
	NoIntroName info;
	parseNoIntroName("", &info);

	TEST_ASSERT_EQUAL_STRING("", info.title);
	TEST_ASSERT_EQUAL_STRING("", info.display_name);
}

void test_parseNoIntroName_only_extension(void) {
	NoIntroName info;
	parseNoIntroName(".nes", &info);

	// Should handle gracefully
	TEST_ASSERT_FALSE(info.has_tags);
}

void test_parseNoIntroName_with_path(void) {
	NoIntroName info;
	parseNoIntroName("/mnt/SDCARD/Roms/GB/Tetris (World).gb", &info);

	TEST_ASSERT_EQUAL_STRING("Tetris", info.title);
	TEST_ASSERT_EQUAL_STRING("World", info.region);
}

void test_parseNoIntroName_unmatched_brackets(void) {
	NoIntroName info;
	parseNoIntroName("Game with (Paren.nes", &info);

	// Should handle gracefully without crashing
	TEST_ASSERT_EQUAL_STRING("Game with (Paren", info.title);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic parsing
	RUN_TEST(test_parseNoIntroName_simple_no_tags);
	RUN_TEST(test_parseNoIntroName_with_extension);
	RUN_TEST(test_parseNoIntroName_multipart_extension);

	// Regions
	RUN_TEST(test_parseNoIntroName_single_region);
	RUN_TEST(test_parseNoIntroName_multi_region);
	RUN_TEST(test_parseNoIntroName_world_region);
	RUN_TEST(test_parseNoIntroName_europe_region);

	// Languages
	RUN_TEST(test_parseNoIntroName_single_language);
	RUN_TEST(test_parseNoIntroName_multi_language);
	RUN_TEST(test_parseNoIntroName_three_languages);

	// Versions
	RUN_TEST(test_parseNoIntroName_version);
	RUN_TEST(test_parseNoIntroName_revision);
	RUN_TEST(test_parseNoIntroName_revision_number);

	// Development status
	RUN_TEST(test_parseNoIntroName_beta);
	RUN_TEST(test_parseNoIntroName_beta_numbered);
	RUN_TEST(test_parseNoIntroName_proto);
	RUN_TEST(test_parseNoIntroName_sample);

	// Status flags
	RUN_TEST(test_parseNoIntroName_bad_dump);
	RUN_TEST(test_parseNoIntroName_verified);

	// Articles
	RUN_TEST(test_parseNoIntroName_article_the);
	RUN_TEST(test_parseNoIntroName_article_a);
	RUN_TEST(test_parseNoIntroName_article_an);
	RUN_TEST(test_parseNoIntroName_article_already_front);

	// Complex cases
	RUN_TEST(test_parseNoIntroName_all_tags);
	RUN_TEST(test_parseNoIntroName_complex_with_brackets);
	RUN_TEST(test_parseNoIntroName_disc_number);
	RUN_TEST(test_parseNoIntroName_unlicensed);

	// Tag order independence
	RUN_TEST(test_parseNoIntroName_tags_different_order_1);
	RUN_TEST(test_parseNoIntroName_tags_different_order_2);
	RUN_TEST(test_parseNoIntroName_tags_different_order_3);

	// Real ROM names
	RUN_TEST(test_parseNoIntroName_real_zip_file);
	RUN_TEST(test_parseNoIntroName_real_gb_compatible);
	RUN_TEST(test_parseNoIntroName_real_multi_region_zip);
	RUN_TEST(test_parseNoIntroName_real_five_languages);
	RUN_TEST(test_parseNoIntroName_adventure_island);

	// Edge cases
	RUN_TEST(test_parseNoIntroName_empty_string);
	RUN_TEST(test_parseNoIntroName_only_extension);
	RUN_TEST(test_parseNoIntroName_with_path);
	RUN_TEST(test_parseNoIntroName_unmatched_brackets);

	return UNITY_END();
}
