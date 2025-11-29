/**
 * Test suite for workspace/all/common/str_compare.c
 * Tests natural sorting (human-friendly alphanumeric ordering)
 */

#include "../../../../workspace/all/common/str_compare.h"
#include "../../../support/unity/unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

///////////////////////////////
// Basic comparison tests
///////////////////////////////

void test_strnatcasecmp_equal_strings(void) {
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("hello", "hello"));
}

void test_strnatcasecmp_case_insensitive(void) {
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("Hello", "hello"));
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("HELLO", "hello"));
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("HeLLo", "hEllO"));
}

void test_strnatcasecmp_alphabetic_order(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("apple", "banana") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("banana", "apple") > 0);
}

void test_strnatcasecmp_empty_strings(void) {
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("", ""));
	TEST_ASSERT_TRUE(strnatcasecmp("", "a") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("a", "") > 0);
}

void test_strnatcasecmp_null_handling(void) {
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp(NULL, NULL));
	TEST_ASSERT_TRUE(strnatcasecmp(NULL, "a") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("a", NULL) > 0);
}

///////////////////////////////
// Natural number sorting
///////////////////////////////

void test_strnatcasecmp_single_digit_numbers(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("file1", "file2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("file2", "file1") > 0);
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("file1", "file1"));
}

void test_strnatcasecmp_multi_digit_numbers(void) {
	// This is the key test - "10" should come AFTER "2"
	TEST_ASSERT_TRUE(strnatcasecmp("file2", "file10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("file10", "file2") > 0);
}

void test_strnatcasecmp_game_numbering(void) {
	// Common game naming patterns
	TEST_ASSERT_TRUE(strnatcasecmp("Game 1", "Game 2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Game 2", "Game 10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Game 9", "Game 10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Game 10", "Game 11") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Game 99", "Game 100") < 0);
}

void test_strnatcasecmp_version_numbers(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("v1.0", "v1.1") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("v1.9", "v1.10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("v2.0", "v10.0") < 0);
}

void test_strnatcasecmp_leading_zeros(void) {
	// Leading zeros should be skipped
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("file01", "file1"));
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("file001", "file1"));
	TEST_ASSERT_TRUE(strnatcasecmp("file01", "file2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("file09", "file10") < 0);
}

void test_strnatcasecmp_numbers_at_start(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("1 Game", "2 Game") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("2 Game", "10 Game") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("10 Game", "20 Game") < 0);
}

void test_strnatcasecmp_numbers_in_middle(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("a1b", "a2b") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("a2b", "a10b") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("test2test", "test10test") < 0);
}

void test_strnatcasecmp_multiple_number_sequences(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("a1b1", "a1b2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("a1b2", "a1b10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("a1b10", "a2b1") < 0);
}

///////////////////////////////
// ROM naming patterns
///////////////////////////////

void test_strnatcasecmp_mario_games(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("Super Mario Bros", "Super Mario Bros 2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Super Mario Bros 2", "Super Mario Bros 3") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Super Mario Bros 3", "Super Mario Bros 10") < 0);
}

void test_strnatcasecmp_final_fantasy(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("Final Fantasy", "Final Fantasy 2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Final Fantasy 2", "Final Fantasy 3") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Final Fantasy 9", "Final Fantasy 10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Final Fantasy 10", "Final Fantasy 12") < 0);
}

void test_strnatcasecmp_megaman(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("Mega Man", "Mega Man 2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Mega Man 2", "Mega Man 3") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Mega Man 9", "Mega Man 10") < 0);
}

void test_strnatcasecmp_zelda(void) {
	// The Legend of Zelda series (after article processing)
	TEST_ASSERT_TRUE(strnatcasecmp("Legend of Zelda", "Legend of Zelda 2") < 0);
}

///////////////////////////////
// Article skipping for sorting
///////////////////////////////

void test_strnatcasecmp_the_article_skipped(void) {
	// "The Legend of Zelda" should sort with "Legend..." not "The..."
	TEST_ASSERT_TRUE(strnatcasecmp("The Legend of Zelda", "Mario") < 0);  // L < M
	TEST_ASSERT_TRUE(strnatcasecmp("The Legend of Zelda", "Asteroids") > 0);  // L > A
}

void test_strnatcasecmp_a_article_skipped(void) {
	// "A Link to the Past" should sort with "Link..." not "A..."
	TEST_ASSERT_TRUE(strnatcasecmp("A Link to the Past", "Mario") < 0);  // L < M
	TEST_ASSERT_TRUE(strnatcasecmp("A Link to the Past", "Asteroids") > 0);  // L > A
}

void test_strnatcasecmp_an_article_skipped(void) {
	// "An American Tail" should sort with "American..." not "An..."
	TEST_ASSERT_TRUE(strnatcasecmp("An American Tail", "Batman") < 0);  // A < B
	TEST_ASSERT_TRUE(strnatcasecmp("An American Tail", "Aardvark") > 0);  // Am > Aa
}

void test_strnatcasecmp_both_have_articles(void) {
	// Both have "The" - should compare the rest
	TEST_ASSERT_TRUE(strnatcasecmp("The Addams Family", "The Legend of Zelda") < 0);  // A < L
	TEST_ASSERT_TRUE(strnatcasecmp("The Legend of Zelda", "The Addams Family") > 0);  // L > A
}

void test_strnatcasecmp_article_case_insensitive(void) {
	// Article matching should be case-insensitive
	TEST_ASSERT_TRUE(strnatcasecmp("THE Legend of Zelda", "Mario") < 0);  // L < M
	TEST_ASSERT_TRUE(strnatcasecmp("the legend of zelda", "mario") < 0);  // l < m
}

void test_strnatcasecmp_article_needs_space(void) {
	// "Theater" should NOT have "The" stripped (no space after)
	TEST_ASSERT_TRUE(strnatcasecmp("Theater", "Super Mario") > 0);  // T > S (not stripped)
	// "Ant" should NOT have "An" stripped
	TEST_ASSERT_TRUE(strnatcasecmp("Ant", "Zoo") < 0);  // A < Z (not stripped)
}

void test_strnatcasecmp_zelda_realistic(void) {
	// Realistic Zelda sorting - all should sort together under "L"
	TEST_ASSERT_TRUE(strnatcasecmp("The Legend of Zelda", "The Legend of Zelda 2") < 0);
	// "Link" > "Legend" because 'i' > 'e'
	TEST_ASSERT_TRUE(strnatcasecmp("A Link to the Past", "The Legend of Zelda") > 0);
}

///////////////////////////////
// Edge cases
///////////////////////////////

void test_strnatcasecmp_only_numbers(void) {
	TEST_ASSERT_TRUE(strnatcasecmp("1", "2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("2", "10") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("10", "100") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("100", "1000") < 0);
}

void test_strnatcasecmp_mixed_content(void) {
	// Numbers should come before letters in ASCII, preserved here
	TEST_ASSERT_TRUE(strnatcasecmp("1abc", "abc") < 0);
}

void test_strnatcasecmp_special_characters(void) {
	// Special characters compared by ASCII value
	TEST_ASSERT_TRUE(strnatcasecmp("game!", "game#") < 0);
	TEST_ASSERT_EQUAL_INT(0, strnatcasecmp("game!", "game!"));
}

void test_strnatcasecmp_prefix_sorting(void) {
	// Shorter string that's a prefix should come first
	TEST_ASSERT_TRUE(strnatcasecmp("Game", "Game 2") < 0);
	TEST_ASSERT_TRUE(strnatcasecmp("Super", "Super Mario") < 0);
}

///////////////////////////////
// Test runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Basic comparison
	RUN_TEST(test_strnatcasecmp_equal_strings);
	RUN_TEST(test_strnatcasecmp_case_insensitive);
	RUN_TEST(test_strnatcasecmp_alphabetic_order);
	RUN_TEST(test_strnatcasecmp_empty_strings);
	RUN_TEST(test_strnatcasecmp_null_handling);

	// Natural number sorting
	RUN_TEST(test_strnatcasecmp_single_digit_numbers);
	RUN_TEST(test_strnatcasecmp_multi_digit_numbers);
	RUN_TEST(test_strnatcasecmp_game_numbering);
	RUN_TEST(test_strnatcasecmp_version_numbers);
	RUN_TEST(test_strnatcasecmp_leading_zeros);
	RUN_TEST(test_strnatcasecmp_numbers_at_start);
	RUN_TEST(test_strnatcasecmp_numbers_in_middle);
	RUN_TEST(test_strnatcasecmp_multiple_number_sequences);

	// ROM naming patterns
	RUN_TEST(test_strnatcasecmp_mario_games);
	RUN_TEST(test_strnatcasecmp_final_fantasy);
	RUN_TEST(test_strnatcasecmp_megaman);
	RUN_TEST(test_strnatcasecmp_zelda);

	// Article skipping for sorting
	RUN_TEST(test_strnatcasecmp_the_article_skipped);
	RUN_TEST(test_strnatcasecmp_a_article_skipped);
	RUN_TEST(test_strnatcasecmp_an_article_skipped);
	RUN_TEST(test_strnatcasecmp_both_have_articles);
	RUN_TEST(test_strnatcasecmp_article_case_insensitive);
	RUN_TEST(test_strnatcasecmp_article_needs_space);
	RUN_TEST(test_strnatcasecmp_zelda_realistic);

	// Edge cases
	RUN_TEST(test_strnatcasecmp_only_numbers);
	RUN_TEST(test_strnatcasecmp_mixed_content);
	RUN_TEST(test_strnatcasecmp_special_characters);
	RUN_TEST(test_strnatcasecmp_prefix_sorting);

	return UNITY_END();
}
