/**
 * test_directory_utils.c - Tests for directory utility functions
 *
 * Tests directory operations using real temp directories.
 * These tests are OS-portable and work in both Docker and native environments.
 */

#include "../../../support/unity/unity.h"
#include "../../../../workspace/all/common/minui_file_utils.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

void setUp(void) {
	// Nothing to set up
}

void tearDown(void) {
	// Nothing to clean up
}

///////////////////////////////
// hasNonHiddenFiles Tests
///////////////////////////////

void test_MinUI_hasNonHiddenFiles_empty_directory(void) {
	// Create temp directory
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Empty directory should return 0
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_FALSE(result);

	// Cleanup
	rmdir(temp_dir);
}

void test_MinUI_hasNonHiddenFiles_with_visible_file(void) {
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create a visible file
	char file_path[512];
	sprintf(file_path, "%s/visible.txt", temp_dir);
	FILE* f = fopen(file_path, "w");
	fputs("test", f);
	fclose(f);

	// Should return 1
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_TRUE(result);

	// Cleanup
	unlink(file_path);
	rmdir(temp_dir);
}

void test_MinUI_hasNonHiddenFiles_only_hidden_files(void) {
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create hidden files
	char file1[512], file2[512];
	sprintf(file1, "%s/.hidden1", temp_dir);
	sprintf(file2, "%s/.hidden2", temp_dir);

	FILE* f1 = fopen(file1, "w");
	fputs("test", f1);
	fclose(f1);

	FILE* f2 = fopen(file2, "w");
	fputs("test", f2);
	fclose(f2);

	// Should return 0 (only hidden files)
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_FALSE(result);

	// Cleanup
	unlink(file1);
	unlink(file2);
	rmdir(temp_dir);
}

void test_MinUI_hasNonHiddenFiles_mixed_files(void) {
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create both hidden and visible files
	char hidden[512], visible[512];
	sprintf(hidden, "%s/.hidden", temp_dir);
	sprintf(visible, "%s/visible.txt", temp_dir);

	FILE* f1 = fopen(hidden, "w");
	fputs("test", f1);
	fclose(f1);

	FILE* f2 = fopen(visible, "w");
	fputs("test", f2);
	fclose(f2);

	// Should return 1 (has at least one visible file)
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_TRUE(result);

	// Cleanup
	unlink(hidden);
	unlink(visible);
	rmdir(temp_dir);
}

void test_MinUI_hasNonHiddenFiles_nonexistent_directory(void) {
	// Should return 0 for nonexistent directory
	int result = MinUI_hasNonHiddenFiles("/nonexistent/path/to/dir");
	TEST_ASSERT_FALSE(result);
}

void test_MinUI_hasNonHiddenFiles_with_subdirectories(void) {
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create a subdirectory (not hidden)
	char subdir[512];
	sprintf(subdir, "%s/subdir", temp_dir);
	mkdir(subdir, 0755);

	// Should return 1 (subdirectory counts as non-hidden entry)
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_TRUE(result);

	// Cleanup
	rmdir(subdir);
	rmdir(temp_dir);
}

void test_MinUI_hasNonHiddenFiles_with_dotfiles(void) {
	char temp_dir[] = "/tmp/dirtest_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create dotfiles that should be hidden
	char file1[512], file2[512], file3[512];
	sprintf(file1, "%s/.DS_Store", temp_dir);
	sprintf(file2, "%s/.gitignore", temp_dir);
	sprintf(file3, "%s/.config", temp_dir);

	FILE* f1 = fopen(file1, "w");
	fclose(f1);
	FILE* f2 = fopen(file2, "w");
	fclose(f2);
	FILE* f3 = fopen(file3, "w");
	fclose(f3);

	// Should return 0 (all hidden)
	int result = MinUI_hasNonHiddenFiles(temp_dir);
	TEST_ASSERT_FALSE(result);

	// Cleanup
	unlink(file1);
	unlink(file2);
	unlink(file3);
	rmdir(temp_dir);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_MinUI_hasNonHiddenFiles_empty_directory);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_with_visible_file);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_only_hidden_files);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_mixed_files);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_nonexistent_directory);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_with_subdirectories);
	RUN_TEST(test_MinUI_hasNonHiddenFiles_with_dotfiles);

	return UNITY_END();
}
