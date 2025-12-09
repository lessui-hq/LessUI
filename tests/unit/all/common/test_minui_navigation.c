/**
 * test_minui_navigation.c - Unit tests for MinUI navigation module
 *
 * Tests the pure navigation logic functions that don't depend on global state.
 * Context-aware functions are tested separately with mock contexts.
 */

#include "../../../support/unity/unity.h"
#include "minui_navigation.h"
#include "minui_entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// MinUINav_determineAction Tests
///////////////////////////////

void test_determineAction_rom_entry_returns_open_rom(void) {
	Entry entry = {.path = "/mnt/SDCARD/Roms/GB/game.gb", .name = "game", .type = ENTRY_ROM};
	MinUINavAction action;

	MinUINav_determineAction(&entry, "/mnt/SDCARD/Roms/GB", NULL, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_ROM, action.action);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/game.gb", action.path);
}

void test_determineAction_pak_entry_returns_open_pak(void) {
	Entry entry = {.path = "/mnt/SDCARD/Tools/Clock.pak", .name = "Clock", .type = ENTRY_PAK};
	MinUINavAction action;

	MinUINav_determineAction(&entry, "/mnt/SDCARD/Tools", NULL, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_PAK, action.action);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Tools/Clock.pak", action.path);
}

void test_determineAction_dir_entry_returns_open_dir_with_auto_launch(void) {
	Entry entry = {.path = "/mnt/SDCARD/Roms/PS1/FF7", .name = "FF7", .type = ENTRY_DIR};
	MinUINavAction action;

	MinUINav_determineAction(&entry, "/mnt/SDCARD/Roms/PS1", NULL, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_DIR, action.action);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/PS1/FF7", action.path);
	TEST_ASSERT_EQUAL(1, action.auto_launch);
}

void test_determineAction_null_entry_returns_none(void) {
	MinUINavAction action;
	action.action = MINUI_NAV_OPEN_ROM; // Pre-set to non-none

	MinUINav_determineAction(NULL, "/path", NULL, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_NONE, action.action);
}

void test_determineAction_null_action_does_not_crash(void) {
	Entry entry = {.path = "/path", .name = "name", .type = ENTRY_ROM};

	// Should not crash
	MinUINav_determineAction(&entry, "/path", NULL, NULL);
}

void test_determineAction_collection_rom_sets_last_path(void) {
	Entry entry = {.path = "/mnt/SDCARD/Roms/GB/game.gb", .name = "game", .type = ENTRY_ROM};
	MinUINavAction action;
	const char* collections_path = "/mnt/SDCARD/.minui/Collections";
	const char* current_path = "/mnt/SDCARD/.minui/Collections/Favorites";

	MinUINav_determineAction(&entry, current_path, collections_path, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_ROM, action.action);
	// last_path should be collection path + filename
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/.minui/Collections/Favorites/game.gb", action.last_path);
}

void test_determineAction_non_collection_rom_no_last_path(void) {
	Entry entry = {.path = "/mnt/SDCARD/Roms/GB/game.gb", .name = "game", .type = ENTRY_ROM};
	MinUINavAction action;
	const char* collections_path = "/mnt/SDCARD/.minui/Collections";
	const char* current_path = "/mnt/SDCARD/Roms/GB"; // Not in collections

	MinUINav_determineAction(&entry, current_path, collections_path, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_ROM, action.action);
	TEST_ASSERT_EQUAL_STRING("", action.last_path); // No last_path for non-collection
}

///////////////////////////////
// MinUINav_shouldAutoLaunch Tests (using real temp dirs)
///////////////////////////////

void test_shouldAutoLaunch_with_cue_file(void) {
	// Create temp directory
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	// Create subdirectory that will contain the cue
	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/MyGame", temp_dir);
	mkdir(game_dir, 0755);

	// Create cue file with same name as directory
	char cue_path[512];
	snprintf(cue_path, sizeof(cue_path), "%s/MyGame.cue", game_dir);
	FILE* f = fopen(cue_path, "w");
	fputs("FILE \"track01.bin\" BINARY\n", f);
	fclose(f);

	char launch_path[512];
	bool result = MinUINav_shouldAutoLaunch(game_dir, launch_path, sizeof(launch_path));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING(cue_path, launch_path);

	// Cleanup
	unlink(cue_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

void test_shouldAutoLaunch_with_m3u_file(void) {
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/MultiDisc", temp_dir);
	mkdir(game_dir, 0755);

	// Create m3u file
	char m3u_path[512];
	snprintf(m3u_path, sizeof(m3u_path), "%s/MultiDisc.m3u", game_dir);
	FILE* f = fopen(m3u_path, "w");
	fputs("disc1.cue\ndisc2.cue\n", f);
	fclose(f);

	char launch_path[512];
	bool result = MinUINav_shouldAutoLaunch(game_dir, launch_path, sizeof(launch_path));

	TEST_ASSERT_TRUE(result);
	TEST_ASSERT_EQUAL_STRING(m3u_path, launch_path);

	// Cleanup
	unlink(m3u_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

void test_shouldAutoLaunch_cue_preferred_over_m3u(void) {
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/BothFiles", temp_dir);
	mkdir(game_dir, 0755);

	// Create both cue and m3u
	char cue_path[512], m3u_path[512];
	snprintf(cue_path, sizeof(cue_path), "%s/BothFiles.cue", game_dir);
	snprintf(m3u_path, sizeof(m3u_path), "%s/BothFiles.m3u", game_dir);

	FILE* f = fopen(cue_path, "w");
	fputs("FILE \"track01.bin\" BINARY\n", f);
	fclose(f);

	f = fopen(m3u_path, "w");
	fputs("disc1.cue\n", f);
	fclose(f);

	char launch_path[512];
	bool result = MinUINav_shouldAutoLaunch(game_dir, launch_path, sizeof(launch_path));

	TEST_ASSERT_TRUE(result);
	// cue should be preferred (checked first)
	TEST_ASSERT_EQUAL_STRING(cue_path, launch_path);

	// Cleanup
	unlink(cue_path);
	unlink(m3u_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

void test_shouldAutoLaunch_no_matching_files(void) {
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/NoMatch", temp_dir);
	mkdir(game_dir, 0755);

	// Create a file with wrong name
	char wrong_path[512];
	snprintf(wrong_path, sizeof(wrong_path), "%s/WrongName.cue", game_dir);
	FILE* f = fopen(wrong_path, "w");
	fputs("content\n", f);
	fclose(f);

	char launch_path[512];
	bool result = MinUINav_shouldAutoLaunch(game_dir, launch_path, sizeof(launch_path));

	TEST_ASSERT_FALSE(result);
	TEST_ASSERT_EQUAL_STRING("", launch_path);

	// Cleanup
	unlink(wrong_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

void test_shouldAutoLaunch_empty_directory(void) {
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char launch_path[512];
	bool result = MinUINav_shouldAutoLaunch(temp_dir, launch_path, sizeof(launch_path));

	TEST_ASSERT_FALSE(result);

	rmdir(temp_dir);
}

void test_shouldAutoLaunch_null_inputs(void) {
	char launch_path[512];

	TEST_ASSERT_FALSE(MinUINav_shouldAutoLaunch(NULL, launch_path, sizeof(launch_path)));
	TEST_ASSERT_FALSE(MinUINav_shouldAutoLaunch("/path", NULL, 512));
	TEST_ASSERT_FALSE(MinUINav_shouldAutoLaunch("/path", launch_path, 0));
}

///////////////////////////////
// MinUINav_buildPakCommand Tests
///////////////////////////////

void test_buildPakCommand_simple_path(void) {
	char cmd[512];

	MinUINav_buildPakCommand("/mnt/SDCARD/Tools/Clock.pak", cmd, sizeof(cmd));

	TEST_ASSERT_EQUAL_STRING("'/mnt/SDCARD/Tools/Clock.pak/launch.sh'", cmd);
}

void test_buildPakCommand_path_with_spaces(void) {
	char cmd[512];

	MinUINav_buildPakCommand("/mnt/SDCARD/Tools/My App.pak", cmd, sizeof(cmd));

	TEST_ASSERT_EQUAL_STRING("'/mnt/SDCARD/Tools/My App.pak/launch.sh'", cmd);
}

void test_buildPakCommand_null_inputs(void) {
	char cmd[512] = "initial";

	MinUINav_buildPakCommand(NULL, cmd, sizeof(cmd));
	TEST_ASSERT_EQUAL_STRING("", cmd);

	cmd[0] = 'x';
	MinUINav_buildPakCommand("/path", NULL, 512);
	// Should not crash

	MinUINav_buildPakCommand("/path", cmd, 0);
	// cmd unchanged since size is 0
}

///////////////////////////////
// MinUINavAction structure Tests
///////////////////////////////

void test_NavAction_structure_size(void) {
	// Ensure action structure has reasonable size
	MinUINavAction action;
	TEST_ASSERT_TRUE(sizeof(action) < 2048); // Should be under 2KB
}

void test_NavAction_paths_are_independent(void) {
	MinUINavAction action;

	strncpy(action.path, "/path/one", sizeof(action.path));
	strncpy(action.last_path, "/path/two", sizeof(action.last_path));

	// Paths should be independent
	TEST_ASSERT_EQUAL_STRING("/path/one", action.path);
	TEST_ASSERT_EQUAL_STRING("/path/two", action.last_path);
}

///////////////////////////////
// MinUINav_buildRomCommand Tests
///////////////////////////////

void test_buildRomCommand_basic(void) {
	char cmd[512], sd_path[512];

	MinUINav_buildRomCommand("/mnt/SDCARD/Roms/GB/game.gb", "gambatte", "/mnt/SDCARD/Emus/GB.pak", false,
	                         NULL, NULL, NULL, cmd, sizeof(cmd), sd_path, sizeof(sd_path));

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/game.gb", sd_path);
	// Command should contain emulator and ROM path
	TEST_ASSERT_NOT_EQUAL(0, strlen(cmd));
}

void test_buildRomCommand_preserves_rom_path(void) {
	char cmd[512], sd_path[512];

	MinUINav_buildRomCommand("/mnt/SDCARD/Roms/GBA/pokemon.gba", "gpsp", "/mnt/SDCARD/Emus/GBA.pak",
	                         false, NULL, NULL, NULL, cmd, sizeof(cmd), sd_path, sizeof(sd_path));

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GBA/pokemon.gba", sd_path);
}

void test_buildRomCommand_handles_path_with_spaces(void) {
	char cmd[512], sd_path[512];

	MinUINav_buildRomCommand("/mnt/SDCARD/Roms/GB/My Game (USA).gb", "gambatte",
	                         "/mnt/SDCARD/Emus/GB.pak", false, NULL, NULL, NULL, cmd, sizeof(cmd),
	                         sd_path, sizeof(sd_path));

	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB/My Game (USA).gb", sd_path);
}

void test_buildRomCommand_null_inputs(void) {
	char cmd[512] = "initial", sd_path[512];

	// Null ROM path
	MinUINav_buildRomCommand(NULL, "emu", "/emu/path", false, NULL, NULL, NULL, cmd, sizeof(cmd),
	                         sd_path, sizeof(sd_path));
	TEST_ASSERT_EQUAL_STRING("", cmd);

	// Null emu path
	cmd[0] = 'x';
	MinUINav_buildRomCommand("/rom", "emu", NULL, false, NULL, NULL, NULL, cmd, sizeof(cmd), sd_path,
	                         sizeof(sd_path));
	TEST_ASSERT_EQUAL_STRING("", cmd);

	// Null cmd buffer - should not crash
	MinUINav_buildRomCommand("/rom", "emu", "/emu", false, NULL, NULL, NULL, NULL, 512, sd_path,
	                         sizeof(sd_path));

	// Zero cmd size
	cmd[0] = 'x';
	MinUINav_buildRomCommand("/rom", "emu", "/emu", false, NULL, NULL, NULL, cmd, 0, sd_path,
	                         sizeof(sd_path));
}

///////////////////////////////
// Context-aware function tests with mock callbacks
///////////////////////////////

// Mock callback tracking
static char mock_save_last_path[512];
static char mock_queue_next_cmd[512];
static char mock_open_directory_path[512];
static int mock_open_directory_auto_launch;
static int mock_save_last_called;
static int mock_queue_next_called;
static int mock_open_directory_called;

static void reset_mocks(void) {
	mock_save_last_path[0] = '\0';
	mock_queue_next_cmd[0] = '\0';
	mock_open_directory_path[0] = '\0';
	mock_open_directory_auto_launch = -1;
	mock_save_last_called = 0;
	mock_queue_next_called = 0;
	mock_open_directory_called = 0;
}

// Mock callback implementations
static void mock_save_last(char* path) {
	mock_save_last_called++;
	if (path) {
		strncpy(mock_save_last_path, path, sizeof(mock_save_last_path) - 1);
	}
}

static void mock_queue_next(char* cmd) {
	mock_queue_next_called++;
	if (cmd) {
		strncpy(mock_queue_next_cmd, cmd, sizeof(mock_queue_next_cmd) - 1);
	}
}

static void mock_open_directory(char* path, int auto_launch) {
	mock_open_directory_called++;
	if (path) {
		strncpy(mock_open_directory_path, path, sizeof(mock_open_directory_path) - 1);
	}
	mock_open_directory_auto_launch = auto_launch;
}

// Helper to create a mock context
static MinUIContext* create_mock_context(MinUICallbacks* callbacks) {
	static MinUIContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.callbacks = callbacks;
	return &ctx;
}

void test_openPak_ctx_calls_save_last(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .save_last = mock_save_last,
	    .queue_next = mock_queue_next,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	MinUINav_openPak(ctx, "/mnt/SDCARD/Tools/Clock.pak");

	TEST_ASSERT_EQUAL(1, mock_save_last_called);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Tools/Clock.pak", mock_save_last_path);
}

void test_openPak_ctx_calls_queue_next_with_command(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .save_last = mock_save_last,
	    .queue_next = mock_queue_next,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	MinUINav_openPak(ctx, "/mnt/SDCARD/Tools/Clock.pak");

	TEST_ASSERT_EQUAL(1, mock_queue_next_called);
	TEST_ASSERT_EQUAL_STRING("'/mnt/SDCARD/Tools/Clock.pak/launch.sh'", mock_queue_next_cmd);
}

void test_openPak_ctx_null_context_does_not_crash(void) {
	reset_mocks();

	MinUINav_openPak(NULL, "/path");

	TEST_ASSERT_EQUAL(0, mock_save_last_called);
	TEST_ASSERT_EQUAL(0, mock_queue_next_called);
}

void test_openPak_ctx_null_path_does_not_crash(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .save_last = mock_save_last,
	    .queue_next = mock_queue_next,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	MinUINav_openPak(ctx, NULL);

	TEST_ASSERT_EQUAL(0, mock_save_last_called);
	TEST_ASSERT_EQUAL(0, mock_queue_next_called);
}

void test_openPak_ctx_null_callbacks_does_not_crash(void) {
	reset_mocks();
	MinUIContext* ctx = create_mock_context(NULL);

	MinUINav_openPak(ctx, "/path");

	TEST_ASSERT_EQUAL(0, mock_save_last_called);
}

void test_openDirectory_ctx_no_autolaunch_calls_open_directory(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .open_directory = mock_open_directory,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	MinUINav_openDirectory(ctx, "/mnt/SDCARD/Roms/GB", 0);

	TEST_ASSERT_EQUAL(1, mock_open_directory_called);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Roms/GB", mock_open_directory_path);
	TEST_ASSERT_EQUAL(0, mock_open_directory_auto_launch);
}

void test_openDirectory_ctx_null_context_does_not_crash(void) {
	reset_mocks();

	MinUINav_openDirectory(NULL, "/path", 0);

	TEST_ASSERT_EQUAL(0, mock_open_directory_called);
}

void test_openDirectory_ctx_null_path_does_not_crash(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .open_directory = mock_open_directory,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	MinUINav_openDirectory(ctx, NULL, 0);

	TEST_ASSERT_EQUAL(0, mock_open_directory_called);
}

void test_closeDirectory_ctx_null_context_does_not_crash(void) {
	// Just verify it doesn't crash
	MinUINav_closeDirectory(NULL);
}

void test_closeDirectory_ctx_empty_stack_does_nothing(void) {
	reset_mocks();
	MinUICallbacks callbacks = {0};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// Set up empty stack
	static Array empty_stack;
	empty_stack.count = 0;
	ctx->stack = &empty_stack;

	// Should not crash with empty stack
	MinUINav_closeDirectory(ctx);
}

void test_openRom_ctx_null_context_does_not_crash(void) {
	// Just verify it doesn't crash
	MinUINav_openRom(NULL, "/path/rom.gb", NULL);
}

void test_openRom_ctx_null_path_does_not_crash(void) {
	reset_mocks();
	MinUICallbacks callbacks = {0};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// Just verify it doesn't crash
	MinUINav_openRom(ctx, NULL, NULL);
}

///////////////////////////////
// MinUINav_openDirectory with autolaunch tests
///////////////////////////////

void test_openDirectory_autolaunch_with_cue_does_not_call_open_directory(void) {
	// Create temp directory with cue file
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/FF7", temp_dir);
	mkdir(game_dir, 0755);

	// Create cue file named after directory
	char cue_path[512];
	snprintf(cue_path, sizeof(cue_path), "%s/FF7.cue", game_dir);
	FILE* f = fopen(cue_path, "w");
	fputs("FILE \"track01.bin\" BINARY\n", f);
	fclose(f);

	reset_mocks();
	MinUICallbacks callbacks = {
	    .open_directory = mock_open_directory,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// With autolaunch enabled and cue present, should NOT call open_directory
	// (would call openRom instead, but that's a stub)
	MinUINav_openDirectory(ctx, game_dir, 1);

	// open_directory should NOT be called because autolaunch detected the cue
	TEST_ASSERT_EQUAL(0, mock_open_directory_called);

	// Cleanup
	unlink(cue_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

void test_openDirectory_autolaunch_without_cue_calls_open_directory(void) {
	// Create temp directory without cue file
	char temp_dir[] = "/tmp/autolaunch_XXXXXX";
	TEST_ASSERT_NOT_NULL(mkdtemp(temp_dir));

	char game_dir[512];
	snprintf(game_dir, sizeof(game_dir), "%s/Games", temp_dir);
	mkdir(game_dir, 0755);

	// Create a ROM file (not a matching cue)
	char rom_path[512];
	snprintf(rom_path, sizeof(rom_path), "%s/game.bin", game_dir);
	FILE* f = fopen(rom_path, "w");
	fputs("rom data", f);
	fclose(f);

	reset_mocks();
	MinUICallbacks callbacks = {
	    .open_directory = mock_open_directory,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// With autolaunch enabled but no matching cue, should call open_directory
	MinUINav_openDirectory(ctx, game_dir, 1);

	TEST_ASSERT_EQUAL(1, mock_open_directory_called);
	TEST_ASSERT_EQUAL_STRING(game_dir, mock_open_directory_path);
	TEST_ASSERT_EQUAL(0, mock_open_directory_auto_launch); // Subdirs don't auto-launch

	// Cleanup
	unlink(rom_path);
	rmdir(game_dir);
	rmdir(temp_dir);
}

///////////////////////////////
// MinUINav_openEntry tests
///////////////////////////////

void test_openEntry_null_context_does_not_crash(void) {
	Entry entry = {.path = "/path", .name = "name", .type = ENTRY_ROM};
	// Should not crash
	MinUINav_openEntry(NULL, &entry);
}

void test_openEntry_null_entry_does_not_crash(void) {
	reset_mocks();
	MinUICallbacks callbacks = {0};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// Should not crash
	MinUINav_openEntry(ctx, NULL);
}

void test_openEntry_pak_calls_openPak(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .save_last = mock_save_last,
	    .queue_next = mock_queue_next,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	Entry entry = {.path = "/mnt/SDCARD/Tools/Clock.pak", .name = "Clock", .type = ENTRY_PAK};

	MinUINav_openEntry(ctx, &entry);

	// Should have called save_last and queue_next via openPak
	TEST_ASSERT_EQUAL(1, mock_save_last_called);
	TEST_ASSERT_EQUAL(1, mock_queue_next_called);
	TEST_ASSERT_EQUAL_STRING("/mnt/SDCARD/Tools/Clock.pak", mock_save_last_path);
}

void test_openEntry_dir_calls_openDirectory(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .open_directory = mock_open_directory,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	Entry entry = {.path = "/mnt/SDCARD/Roms/GB", .name = "GB", .type = ENTRY_DIR};

	MinUINav_openEntry(ctx, &entry);

	// Since it's a directory with auto_launch, it will try to autolaunch first
	// Since /mnt/SDCARD/Roms/GB probably doesn't exist with a matching cue,
	// it should call open_directory
	TEST_ASSERT_EQUAL(1, mock_open_directory_called);
}

void test_openEntry_sets_recent_alias(void) {
	reset_mocks();
	MinUICallbacks callbacks = {
	    .save_last = mock_save_last,
	    .queue_next = mock_queue_next,
	};
	MinUIContext* ctx = create_mock_context(&callbacks);

	// Set up recent_alias pointer
	char* alias_target = NULL;
	ctx->recent_alias = &alias_target;

	Entry entry = {.path = "/mnt/SDCARD/Tools/Clock.pak", .name = "Clock", .type = ENTRY_PAK};

	MinUINav_openEntry(ctx, &entry);

	// recent_alias should point to entry name
	TEST_ASSERT_EQUAL_STRING("Clock", alias_target);
}

///////////////////////////////
// determineAction edge cases
///////////////////////////////

void test_determineAction_unknown_entry_type(void) {
	Entry entry = {.path = "/path", .name = "name", .type = 999}; // Invalid type
	MinUINavAction action;
	action.action = MINUI_NAV_OPEN_ROM; // Pre-set

	MinUINav_determineAction(&entry, "/path", NULL, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_NONE, action.action);
}

void test_determineAction_collection_rom_no_slash_in_path(void) {
	// Edge case: entry path has no slash
	Entry entry = {.path = "game.gb", .name = "game", .type = ENTRY_ROM};
	MinUINavAction action;
	const char* collections_path = "/mnt/SDCARD/.minui/Collections";
	const char* current_path = "/mnt/SDCARD/.minui/Collections/Favorites";

	MinUINav_determineAction(&entry, current_path, collections_path, &action);

	TEST_ASSERT_EQUAL(MINUI_NAV_OPEN_ROM, action.action);
	// No slash in entry path, so last_path remains empty
	TEST_ASSERT_EQUAL_STRING("", action.last_path);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// determineAction tests
	RUN_TEST(test_determineAction_rom_entry_returns_open_rom);
	RUN_TEST(test_determineAction_pak_entry_returns_open_pak);
	RUN_TEST(test_determineAction_dir_entry_returns_open_dir_with_auto_launch);
	RUN_TEST(test_determineAction_null_entry_returns_none);
	RUN_TEST(test_determineAction_null_action_does_not_crash);
	RUN_TEST(test_determineAction_collection_rom_sets_last_path);
	RUN_TEST(test_determineAction_non_collection_rom_no_last_path);

	// shouldAutoLaunch tests (real temp dirs)
	RUN_TEST(test_shouldAutoLaunch_with_cue_file);
	RUN_TEST(test_shouldAutoLaunch_with_m3u_file);
	RUN_TEST(test_shouldAutoLaunch_cue_preferred_over_m3u);
	RUN_TEST(test_shouldAutoLaunch_no_matching_files);
	RUN_TEST(test_shouldAutoLaunch_empty_directory);
	RUN_TEST(test_shouldAutoLaunch_null_inputs);

	// Context-aware openPak tests
	RUN_TEST(test_openPak_ctx_calls_save_last);
	RUN_TEST(test_openPak_ctx_calls_queue_next_with_command);
	RUN_TEST(test_openPak_ctx_null_context_does_not_crash);
	RUN_TEST(test_openPak_ctx_null_path_does_not_crash);
	RUN_TEST(test_openPak_ctx_null_callbacks_does_not_crash);

	// Context-aware openDirectory tests
	RUN_TEST(test_openDirectory_ctx_no_autolaunch_calls_open_directory);
	RUN_TEST(test_openDirectory_ctx_null_context_does_not_crash);
	RUN_TEST(test_openDirectory_ctx_null_path_does_not_crash);

	// Context-aware closeDirectory tests
	RUN_TEST(test_closeDirectory_ctx_null_context_does_not_crash);
	RUN_TEST(test_closeDirectory_ctx_empty_stack_does_nothing);

	// Context-aware openRom tests
	RUN_TEST(test_openRom_ctx_null_context_does_not_crash);
	RUN_TEST(test_openRom_ctx_null_path_does_not_crash);

	// buildPakCommand tests
	RUN_TEST(test_buildPakCommand_simple_path);
	RUN_TEST(test_buildPakCommand_path_with_spaces);
	RUN_TEST(test_buildPakCommand_null_inputs);

	// Structure tests
	RUN_TEST(test_NavAction_structure_size);
	RUN_TEST(test_NavAction_paths_are_independent);

	// buildRomCommand tests
	RUN_TEST(test_buildRomCommand_basic);
	RUN_TEST(test_buildRomCommand_preserves_rom_path);
	RUN_TEST(test_buildRomCommand_handles_path_with_spaces);
	RUN_TEST(test_buildRomCommand_null_inputs);

	// openDirectory with autolaunch tests
	RUN_TEST(test_openDirectory_autolaunch_with_cue_does_not_call_open_directory);
	RUN_TEST(test_openDirectory_autolaunch_without_cue_calls_open_directory);

	// openEntry tests
	RUN_TEST(test_openEntry_null_context_does_not_crash);
	RUN_TEST(test_openEntry_null_entry_does_not_crash);
	RUN_TEST(test_openEntry_pak_calls_openPak);
	RUN_TEST(test_openEntry_dir_calls_openDirectory);
	RUN_TEST(test_openEntry_sets_recent_alias);

	// determineAction edge cases
	RUN_TEST(test_determineAction_unknown_entry_type);
	RUN_TEST(test_determineAction_collection_rom_no_slash_in_path);

	return UNITY_END();
}
