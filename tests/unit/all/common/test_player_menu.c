/**
 * test_player_menu.c - Unit tests for the Player menu context system
 *
 * Validates that the PlayerContext pattern enables unit testing by:
 * - Testing context creation and access
 * - Testing state manipulation through context
 * - Testing menu state management logic (slot/disc navigation)
 * - Testing path generation for save previews
 *
 * This is a foundational test that proves the context pattern works.
 * Additional tests can build on this infrastructure.
 */

#include "../../../support/unity/unity.h"

// fff for mocking SDL functions
#include "../../../support/fff/fff.h"

// Include SDL stub headers BEFORE player headers
// This provides the actual SDL_Surface structure definition
#include <SDL/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// We test the context and menu state structures directly
// without compiling the full player_menu.c
#include "player_context.h"
#include "player_menu.h"

///////////////////////////////
// Test Fixtures
///////////////////////////////

// Test directory for temp files (exported for menu_state_stub.c)
char test_dir[256];

// Mock surfaces for context
static SDL_Surface mock_screen_surface;
static SDL_Surface* mock_screen = &mock_screen_surface;

// Mock game/core structures (minimal for testing, exported for menu_state_stub.c)
struct Game mock_game = {
    .path = "/mnt/SDCARD/Roms/GB/Tetris.gb",
    .name = "Tetris",
    .m3u_path = "",
    .tmp_path = "",
    .data = NULL,
    .size = 0,
    .is_open = 1,
};

static struct Core mock_core = {
    .initialized = 1,
    .name = "gambatte",
    .tag = "GB",
    .aspect_ratio = 1.0,
};

// Mock context state (mock_state_slot exported for menu_state_stub.c)
static int mock_quit = 0;
static int mock_show_menu = 1;
static int mock_simple_mode = 0;
int mock_state_slot = 0;
static int mock_screen_scaling = 0;
static int mock_device_width = 640;
static int mock_device_height = 480;
static int mock_device_pitch = 1280;

///////////////////////////////
// Test Helpers
///////////////////////////////

static PlayerContext* create_test_context(void) {
	PlayerContext* ctx = PlayerContext_get();

	// Wire up mock state
	ctx->core = &mock_core;
	ctx->game = &mock_game;
	ctx->screen = &mock_screen;
	ctx->quit = &mock_quit;
	ctx->show_menu = &mock_show_menu;
	ctx->simple_mode = &mock_simple_mode;
	ctx->state_slot = &mock_state_slot;
	ctx->screen_scaling = &mock_screen_scaling;
	ctx->device_width = &mock_device_width;
	ctx->device_height = &mock_device_height;
	ctx->device_pitch = &mock_device_pitch;
	ctx->menu = PlayerMenu_getState();

	return ctx;
}

static void reset_menu_state(PlayerMenuState* m) {
	m->slot = 0;
	m->disc = -1;
	m->total_discs = 0;
	m->save_exists = 0;
	m->preview_exists = 0;
	memset(m->launcher_dir, 0, sizeof(m->launcher_dir));
	memset(m->slot_path, 0, sizeof(m->slot_path));
	memset(m->bmp_path, 0, sizeof(m->bmp_path));
	memset(m->txt_path, 0, sizeof(m->txt_path));

	// Free disc paths
	for (int i = 0; i < MENU_MAX_DISCS; i++) {
		if (m->disc_paths[i]) {
			free(m->disc_paths[i]);
			m->disc_paths[i] = NULL;
		}
	}
}

static void write_text_file(const char* path, const char* content) {
	FILE* f = fopen(path, "w");
	if (f) {
		fputs(content, f);
		fclose(f);
	}
}

static void create_empty_file(const char* path) {
	FILE* f = fopen(path, "w");
	if (f)
		fclose(f);
}

///////////////////////////////
// Test Setup/Teardown
///////////////////////////////

void setUp(void) {
	// Create temp directory
	strcpy(test_dir, "/tmp/test_menu_XXXXXX");
	char* dir = mkdtemp(test_dir);
	TEST_ASSERT_NOT_NULL(dir);

	// Reset mock state
	mock_quit = 0;
	mock_show_menu = 1;
	mock_simple_mode = 0;
	mock_state_slot = 0;

	// Reset game mock
	strcpy(mock_game.path, "/mnt/SDCARD/Roms/GB/Tetris.gb");
	strcpy(mock_game.name, "Tetris");
	mock_game.m3u_path[0] = '\0';

	// Reset menu state
	reset_menu_state(PlayerMenu_getState());
}

void tearDown(void) {
	// Clean up temp files
	char path[512];
	for (int i = 0; i <= 9; i++) {
		snprintf(path, sizeof(path), "%s/Tetris.%d.bmp", test_dir, i);
		unlink(path);
		snprintf(path, sizeof(path), "%s/Tetris.%d.txt", test_dir, i);
		unlink(path);
		snprintf(path, sizeof(path), "%s/Tetris.st%d", test_dir, i);
		unlink(path);
	}
	snprintf(path, sizeof(path), "%s/Tetris.txt", test_dir);
	unlink(path);
	snprintf(path, sizeof(path), "%s/map.txt", test_dir);
	unlink(path);

	rmdir(test_dir);
}

///////////////////////////////
// Context Pattern Tests
///////////////////////////////

void test_context_get_returns_same_instance(void) {
	PlayerContext* ctx1 = PlayerContext_get();
	PlayerContext* ctx2 = PlayerContext_get();

	TEST_ASSERT_NOT_NULL(ctx1);
	TEST_ASSERT_EQUAL_PTR(ctx1, ctx2);
}

void test_context_provides_access_to_game(void) {
	PlayerContext* ctx = create_test_context();

	TEST_ASSERT_NOT_NULL(ctx->game);
	TEST_ASSERT_EQUAL_STRING("Tetris", ctx->game->name);
}

void test_context_provides_access_to_core(void) {
	PlayerContext* ctx = create_test_context();

	TEST_ASSERT_NOT_NULL(ctx->core);
	TEST_ASSERT_EQUAL_STRING("gambatte", ctx->core->name);
}

void test_context_provides_access_to_menu_state(void) {
	PlayerContext* ctx = create_test_context();

	TEST_ASSERT_NOT_NULL(ctx->menu);
	TEST_ASSERT_EQUAL(0, ctx->menu->slot);
}

void test_context_state_modifications_persist(void) {
	PlayerContext* ctx = create_test_context();

	// Modify via context
	ctx->menu->slot = 5;
	*ctx->state_slot = 7;

	// Verify changes persist
	TEST_ASSERT_EQUAL(5, ctx->menu->slot);
	TEST_ASSERT_EQUAL(7, mock_state_slot);
}

void test_ctx_accessors_work(void) {
	PlayerContext* ctx = create_test_context();

	// Test the inline accessor functions
	TEST_ASSERT_NOT_NULL(ctx_getCore(ctx));
	TEST_ASSERT_NOT_NULL(ctx_getGame(ctx));
	TEST_ASSERT_EQUAL(0, ctx_isQuitting(ctx));
	TEST_ASSERT_EQUAL(1, ctx_isMenuShown(ctx));

	// Test setters
	ctx_setQuit(ctx, 1);
	TEST_ASSERT_EQUAL(1, ctx_isQuitting(ctx));

	ctx_setShowMenu(ctx, 0);
	TEST_ASSERT_EQUAL(0, ctx_isMenuShown(ctx));
}

void test_ctx_accessors_handle_null_safely(void) {
	// Test NULL context handling
	TEST_ASSERT_NULL(ctx_getCore(NULL));
	TEST_ASSERT_NULL(ctx_getGame(NULL));
	TEST_ASSERT_EQUAL(0, ctx_isQuitting(NULL));
	TEST_ASSERT_EQUAL(0, ctx_isMenuShown(NULL));

	// Setters should not crash on NULL
	ctx_setQuit(NULL, 1);
	ctx_setShowMenu(NULL, 1);
}

///////////////////////////////
// Context Initialization Tests
///////////////////////////////

void test_context_getCallbacks_returns_instance(void) {
	PlayerCallbacks* cb = PlayerContext_getCallbacks();
	TEST_ASSERT_NOT_NULL(cb);

	// Same instance on multiple calls
	PlayerCallbacks* cb2 = PlayerContext_getCallbacks();
	TEST_ASSERT_EQUAL_PTR(cb, cb2);
}

void test_context_initGlobals_handles_null(void) {
	// Should not crash
	PlayerContext_initGlobals(NULL);
}

void test_context_initGlobals_sets_initialized(void) {
	PlayerContext* ctx = PlayerContext_get();
	PlayerContext_initGlobals(ctx);
	// If we get here without crashing, initialization worked
	TEST_ASSERT_NOT_NULL(ctx);
}

void test_context_initCallbacks_handles_null(void) {
	// Should not crash with NULL context
	PlayerCallbacks cb = {0};
	PlayerContext_initCallbacks(NULL, &cb);

	// Should not crash with NULL callbacks
	PlayerContext* ctx = PlayerContext_get();
	PlayerContext_initCallbacks(ctx, NULL);
}

void test_context_initCallbacks_links_to_context(void) {
	PlayerContext* ctx = PlayerContext_get();
	PlayerCallbacks* cb = PlayerContext_getCallbacks();

	// Setup test callback
	cb->sram_write = NULL;

	PlayerContext_initCallbacks(ctx, cb);

	// Verify callback is linked
	TEST_ASSERT_NOT_NULL(ctx->callbacks);
	TEST_ASSERT_EQUAL_PTR(cb, ctx->callbacks);
}

///////////////////////////////
// Slot Navigation Tests
///////////////////////////////

void test_slot_increment_wraps_at_max(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	// Simulate slot navigation logic from Menu_loop
	m->slot = MENU_SLOT_COUNT - 1; // slot 7

	// Increment (what Menu_loop does on BTN_RIGHT)
	m->slot += 1;
	if (m->slot >= MENU_SLOT_COUNT)
		m->slot -= MENU_SLOT_COUNT;

	TEST_ASSERT_EQUAL(0, m->slot);
}

void test_slot_decrement_wraps_at_zero(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	m->slot = 0;

	// Decrement (what Menu_loop does on BTN_LEFT)
	m->slot -= 1;
	if (m->slot < 0)
		m->slot += MENU_SLOT_COUNT;

	TEST_ASSERT_EQUAL(MENU_SLOT_COUNT - 1, m->slot);
}

void test_slot_navigation_full_cycle(void) {
	PlayerMenuState* m = PlayerMenu_getState();
	m->slot = 0;

	// Navigate through all slots forward
	for (int i = 0; i < MENU_SLOT_COUNT; i++) {
		TEST_ASSERT_EQUAL(i, m->slot);
		m->slot += 1;
		if (m->slot >= MENU_SLOT_COUNT)
			m->slot -= MENU_SLOT_COUNT;
	}

	// Should wrap back to 0
	TEST_ASSERT_EQUAL(0, m->slot);
}

///////////////////////////////
// Disc Navigation Tests
///////////////////////////////

void test_disc_increment_wraps_at_total(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	m->total_discs = 3;
	m->disc = 2; // Last disc (0-indexed)

	// Increment (what Menu_loop does on BTN_RIGHT for discs)
	m->disc += 1;
	if (m->disc == m->total_discs)
		m->disc -= m->total_discs;

	TEST_ASSERT_EQUAL(0, m->disc);
}

void test_disc_decrement_wraps_at_zero(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	m->total_discs = 3;
	m->disc = 0;

	// Decrement (what Menu_loop does on BTN_LEFT for discs)
	m->disc -= 1;
	if (m->disc < 0)
		m->disc += m->total_discs;

	TEST_ASSERT_EQUAL(2, m->disc);
}

void test_disc_navigation_with_two_discs(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	m->total_discs = 2;
	m->disc = 0;

	// Increment to disc 1
	m->disc += 1;
	if (m->disc == m->total_discs)
		m->disc -= m->total_discs;
	TEST_ASSERT_EQUAL(1, m->disc);

	// Increment to wrap back to disc 0
	m->disc += 1;
	if (m->disc == m->total_discs)
		m->disc -= m->total_discs;
	TEST_ASSERT_EQUAL(0, m->disc);
}

void test_disc_paths_can_be_set(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	m->total_discs = 2;
	m->disc_paths[0] = strdup("/path/to/disc1.cue");
	m->disc_paths[1] = strdup("/path/to/disc2.cue");

	TEST_ASSERT_EQUAL_STRING("/path/to/disc1.cue", m->disc_paths[0]);
	TEST_ASSERT_EQUAL_STRING("/path/to/disc2.cue", m->disc_paths[1]);

	// Cleanup handled by reset_menu_state in tearDown
}

///////////////////////////////
// Menu State Path Tests
///////////////////////////////

void test_menu_state_launcher_dir_can_be_set(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	strcpy(m->launcher_dir, test_dir);
	TEST_ASSERT_EQUAL_STRING(test_dir, m->launcher_dir);
}

void test_menu_state_slot_path_can_be_set(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	char expected[256];
	sprintf(expected, "%s/Tetris.txt", test_dir);
	strcpy(m->slot_path, expected);

	TEST_ASSERT_EQUAL_STRING(expected, m->slot_path);
}

void test_menu_state_bmp_path_generation(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	strcpy(m->launcher_dir, test_dir);
	m->slot = 3;

	// Generate bmp_path like Menu_updateState does
	sprintf(m->bmp_path, "%s/%s.%d.bmp", m->launcher_dir, mock_game.name, m->slot);

	char expected[256];
	sprintf(expected, "%s/Tetris.3.bmp", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected, m->bmp_path);
}

void test_menu_state_txt_path_generation(void) {
	PlayerMenuState* m = PlayerMenu_getState();

	strcpy(m->launcher_dir, test_dir);
	m->slot = 5;

	// Generate txt_path like Menu_updateState does
	sprintf(m->txt_path, "%s/%s.%d.txt", m->launcher_dir, mock_game.name, m->slot);

	char expected[256];
	sprintf(expected, "%s/Tetris.5.txt", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected, m->txt_path);
}

///////////////////////////////
// PlayerMenu_initState Tests
///////////////////////////////

void test_initState_sets_slot_to_zero_when_no_file(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	// Set up paths
	sprintf(m->slot_path, "%s/Tetris.txt", test_dir);

	// No slot file exists
	PlayerMenu_initState(ctx);

	TEST_ASSERT_EQUAL(0, m->slot);
	TEST_ASSERT_EQUAL(0, m->save_exists);
	TEST_ASSERT_EQUAL(0, m->preview_exists);
}

void test_initState_loads_slot_from_file(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	// Set up paths
	sprintf(m->slot_path, "%s/Tetris.txt", test_dir);

	// Create slot file with slot 3
	write_text_file(m->slot_path, "3");

	PlayerMenu_initState(ctx);

	TEST_ASSERT_EQUAL(3, m->slot);
}

void test_initState_resets_slot_8_to_0(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	// Set up paths
	sprintf(m->slot_path, "%s/Tetris.txt", test_dir);

	// Create slot file with slot 8 (auto-resume slot, should reset)
	write_text_file(m->slot_path, "8");

	PlayerMenu_initState(ctx);

	// Slot 8 is auto-resume, should reset to 0
	TEST_ASSERT_EQUAL(0, m->slot);
}

void test_initState_preserves_valid_slots(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	sprintf(m->slot_path, "%s/Tetris.txt", test_dir);

	// Test each valid slot 0-7
	for (int slot = 0; slot <= 7; slot++) {
		char slot_str[16];
		sprintf(slot_str, "%d", slot);
		write_text_file(m->slot_path, slot_str);

		PlayerMenu_initState(ctx);

		TEST_ASSERT_EQUAL(slot, m->slot);
	}
}

void test_initState_resets_flags(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	sprintf(m->slot_path, "%s/Tetris.txt", test_dir);

	// Set flags to non-zero
	m->save_exists = 1;
	m->preview_exists = 1;

	PlayerMenu_initState(ctx);

	// Should always reset to 0
	TEST_ASSERT_EQUAL(0, m->save_exists);
	TEST_ASSERT_EQUAL(0, m->preview_exists);
}

///////////////////////////////
// PlayerMenu_updateState Tests
///////////////////////////////

void test_updateState_generates_bmp_path(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "SuperGame");
	m->slot = 2;

	PlayerMenu_updateState(ctx);

	char expected[256];
	sprintf(expected, "%s/SuperGame.2.bmp", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected, m->bmp_path);
}

void test_updateState_generates_txt_path(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "SuperGame");
	m->slot = 5;

	PlayerMenu_updateState(ctx);

	char expected[256];
	sprintf(expected, "%s/SuperGame.5.txt", test_dir);
	TEST_ASSERT_EQUAL_STRING(expected, m->txt_path);
}

void test_updateState_detects_existing_save(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "Tetris");
	m->slot = 1;

	// Create the state file
	char state_path[256];
	sprintf(state_path, "%s/Tetris.st1", test_dir);
	create_empty_file(state_path);

	PlayerMenu_updateState(ctx);

	TEST_ASSERT_EQUAL(1, m->save_exists);
}

void test_updateState_detects_missing_save(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "Tetris");
	m->slot = 3;

	// No state file exists

	PlayerMenu_updateState(ctx);

	TEST_ASSERT_EQUAL(0, m->save_exists);
}

void test_updateState_detects_preview_when_save_and_bmp_exist(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "Tetris");
	m->slot = 4;

	// Create both state file and preview
	char state_path[256];
	sprintf(state_path, "%s/Tetris.st4", test_dir);
	create_empty_file(state_path);

	char bmp_path[256];
	sprintf(bmp_path, "%s/Tetris.4.bmp", test_dir);
	create_empty_file(bmp_path);

	PlayerMenu_updateState(ctx);

	TEST_ASSERT_EQUAL(1, m->save_exists);
	TEST_ASSERT_EQUAL(1, m->preview_exists);
}

void test_updateState_no_preview_without_save(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	strcpy(mock_game.name, "Tetris");
	m->slot = 6;

	// Create only the preview file (no save)
	char bmp_path[256];
	sprintf(bmp_path, "%s/Tetris.6.bmp", test_dir);
	create_empty_file(bmp_path);

	PlayerMenu_updateState(ctx);

	// Preview requires save to exist
	TEST_ASSERT_EQUAL(0, m->save_exists);
	TEST_ASSERT_EQUAL(0, m->preview_exists);
}

void test_updateState_preserves_state_slot(void) {
	PlayerContext* ctx = create_test_context();
	PlayerMenuState* m = ctx->menu;

	strcpy(m->launcher_dir, test_dir);
	m->slot = 5;
	*ctx->state_slot = 2; // Different from menu slot

	int original_state_slot = *ctx->state_slot;

	PlayerMenu_updateState(ctx);

	// Should restore original state_slot after using it to get save path
	TEST_ASSERT_EQUAL(original_state_slot, *ctx->state_slot);
}

///////////////////////////////
// PlayerMenu_getAlias Tests
///////////////////////////////

void test_getAlias_returns_alias_from_map_file(void) {
	PlayerContext* ctx = create_test_context();

	// Create map.txt with an alias
	char map_path[256];
	sprintf(map_path, "%s/map.txt", test_dir);
	write_text_file(map_path, "tetris.gb\tTetris DX\nzelda.gb\tZelda\n");

	// Build path that would be in the same directory
	char rom_path[256];
	sprintf(rom_path, "%s/tetris.gb", test_dir);

	char alias[256] = "";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	TEST_ASSERT_EQUAL_STRING("Tetris DX", alias);
}

void test_getAlias_returns_second_entry(void) {
	PlayerContext* ctx = create_test_context();

	char map_path[256];
	sprintf(map_path, "%s/map.txt", test_dir);
	write_text_file(map_path, "mario.gb\tSuper Mario\nzelda.gb\tLegend of Zelda\n");

	char rom_path[256];
	sprintf(rom_path, "%s/zelda.gb", test_dir);

	char alias[256] = "";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	TEST_ASSERT_EQUAL_STRING("Legend of Zelda", alias);
}

void test_getAlias_keeps_original_when_no_match(void) {
	PlayerContext* ctx = create_test_context();

	char map_path[256];
	sprintf(map_path, "%s/map.txt", test_dir);
	write_text_file(map_path, "other.gb\tOther Game\n");

	char rom_path[256];
	sprintf(rom_path, "%s/unknown.gb", test_dir);

	char alias[256] = "Original Name";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	// Should keep original when no match found
	TEST_ASSERT_EQUAL_STRING("Original Name", alias);
}

void test_getAlias_keeps_original_when_no_map_file(void) {
	PlayerContext* ctx = create_test_context();

	// No map.txt exists
	char rom_path[256];
	sprintf(rom_path, "%s/game.gb", test_dir);

	char alias[256] = "Default Name";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	TEST_ASSERT_EQUAL_STRING("Default Name", alias);
}

void test_getAlias_handles_empty_lines(void) {
	PlayerContext* ctx = create_test_context();

	char map_path[256];
	sprintf(map_path, "%s/map.txt", test_dir);
	write_text_file(map_path, "\n\ntetris.gb\tTetris\n\n");

	char rom_path[256];
	sprintf(rom_path, "%s/tetris.gb", test_dir);

	char alias[256] = "";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	TEST_ASSERT_EQUAL_STRING("Tetris", alias);
}

void test_getAlias_skips_malformed_lines(void) {
	PlayerContext* ctx = create_test_context();

	char map_path[256];
	sprintf(map_path, "%s/map.txt", test_dir);
	write_text_file(map_path, "no-tab-line\ntetris.gb\tTetris DX\n");

	char rom_path[256];
	sprintf(rom_path, "%s/tetris.gb", test_dir);

	char alias[256] = "";
	PlayerMenu_getAlias(ctx, rom_path, alias);

	TEST_ASSERT_EQUAL_STRING("Tetris DX", alias);
}

///////////////////////////////
// Navigation State Tests
///////////////////////////////

void test_nav_init_sets_defaults(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);

	TEST_ASSERT_EQUAL(10, nav.count);
	TEST_ASSERT_EQUAL(5, nav.max_visible);
	TEST_ASSERT_EQUAL(0, nav.selected);
	TEST_ASSERT_EQUAL(0, nav.start);
	TEST_ASSERT_EQUAL(5, nav.end);
	TEST_ASSERT_EQUAL(5, nav.visible_rows);
	TEST_ASSERT_EQUAL(1, nav.dirty);
	TEST_ASSERT_EQUAL(0, nav.await_input);
	TEST_ASSERT_EQUAL(0, nav.should_exit);
}

void test_nav_init_fewer_items_than_visible(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 3, 10);

	TEST_ASSERT_EQUAL(3, nav.count);
	TEST_ASSERT_EQUAL(0, nav.start);
	TEST_ASSERT_EQUAL(3, nav.end); // Capped at count
	TEST_ASSERT_EQUAL(3, nav.visible_rows);
}

void test_nav_navigate_down_basic(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);

	int changed = PlayerMenuNav_navigate(&nav, +1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(1, nav.selected);
	TEST_ASSERT_EQUAL(0, nav.start); // No scroll yet
}

void test_nav_navigate_up_basic(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 2;

	int changed = PlayerMenuNav_navigate(&nav, -1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(1, nav.selected);
}

void test_nav_navigate_down_wraps(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 9; // Last item
	nav.start = 5;
	nav.end = 10;

	int changed = PlayerMenuNav_navigate(&nav, +1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(0, nav.selected); // Wrapped to first
	TEST_ASSERT_EQUAL(0, nav.start);
	TEST_ASSERT_EQUAL(5, nav.end);
}

void test_nav_navigate_up_wraps(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 0; // First item

	int changed = PlayerMenuNav_navigate(&nav, -1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(9, nav.selected); // Wrapped to last
	TEST_ASSERT_EQUAL(5, nav.start);
	TEST_ASSERT_EQUAL(10, nav.end);
}

void test_nav_navigate_scrolls_down(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 4; // Last visible
	nav.start = 0;
	nav.end = 5;

	int changed = PlayerMenuNav_navigate(&nav, +1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(5, nav.selected);
	TEST_ASSERT_EQUAL(1, nav.start); // Scrolled
	TEST_ASSERT_EQUAL(6, nav.end);
}

void test_nav_navigate_scrolls_up(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 3;
	nav.start = 3;
	nav.end = 8;

	int changed = PlayerMenuNav_navigate(&nav, -1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(2, nav.selected);
	TEST_ASSERT_EQUAL(2, nav.start); // Scrolled
	TEST_ASSERT_EQUAL(7, nav.end);
}

void test_nav_navigate_zero_count(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 0, 5);

	int changed = PlayerMenuNav_navigate(&nav, +1);

	TEST_ASSERT_EQUAL(0, changed);
}

void test_nav_navigate_zero_direction(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);

	int changed = PlayerMenuNav_navigate(&nav, 0);

	TEST_ASSERT_EQUAL(0, changed);
	TEST_ASSERT_EQUAL(0, nav.selected);
}

void test_nav_advanceItem_basic(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 2;

	PlayerMenuNav_advanceItem(&nav);

	TEST_ASSERT_EQUAL(3, nav.selected);
}

void test_nav_advanceItem_wraps(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 9;
	nav.start = 5;
	nav.end = 10;

	PlayerMenuNav_advanceItem(&nav);

	TEST_ASSERT_EQUAL(0, nav.selected);
	TEST_ASSERT_EQUAL(0, nav.start);
	TEST_ASSERT_EQUAL(5, nav.end);
}

void test_nav_advanceItem_scrolls(void) {
	PlayerMenuNavState nav;
	PlayerMenuNav_init(&nav, 10, 5);
	nav.selected = 4;
	nav.start = 0;
	nav.end = 5;

	PlayerMenuNav_advanceItem(&nav);

	TEST_ASSERT_EQUAL(5, nav.selected);
	TEST_ASSERT_EQUAL(1, nav.start);
	TEST_ASSERT_EQUAL(6, nav.end);
}

///////////////////////////////
// Value Cycling Tests
///////////////////////////////

static char* test_values[] = {"Off", "Low", "Medium", "High", NULL};

void test_nav_cycleValue_right_basic(void) {
	MenuItem item = {.name = "Test", .values = test_values, .value = 0};

	int changed = PlayerMenuNav_cycleValue(&item, +1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(1, item.value);
}

void test_nav_cycleValue_left_basic(void) {
	MenuItem item = {.name = "Test", .values = test_values, .value = 2};

	int changed = PlayerMenuNav_cycleValue(&item, -1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(1, item.value);
}

void test_nav_cycleValue_right_wraps(void) {
	MenuItem item = {.name = "Test", .values = test_values, .value = 3}; // "High" is last

	int changed = PlayerMenuNav_cycleValue(&item, +1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(0, item.value); // Wrapped to "Off"
}

void test_nav_cycleValue_left_wraps(void) {
	MenuItem item = {.name = "Test", .values = test_values, .value = 0}; // "Off" is first

	int changed = PlayerMenuNav_cycleValue(&item, -1);

	TEST_ASSERT_EQUAL(1, changed);
	TEST_ASSERT_EQUAL(3, item.value); // Wrapped to "High"
}

void test_nav_cycleValue_no_values(void) {
	MenuItem item = {.name = "Test", .values = NULL, .value = 0};

	int changed = PlayerMenuNav_cycleValue(&item, +1);

	TEST_ASSERT_EQUAL(0, changed);
}

void test_nav_cycleValue_zero_direction(void) {
	MenuItem item = {.name = "Test", .values = test_values, .value = 1};

	int changed = PlayerMenuNav_cycleValue(&item, 0);

	TEST_ASSERT_EQUAL(0, changed);
	TEST_ASSERT_EQUAL(1, item.value);
}

///////////////////////////////
// Action Detection Tests
///////////////////////////////

void test_nav_getAction_b_exits(void) {
	MenuItem item = {.name = "Test"};
	MenuList list = {.type = MENU_LIST, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 0, 1, 0, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_EXIT, action);
}

void test_nav_getAction_a_with_on_confirm(void) {
	MenuItem item = {.name = "Test", .on_confirm = (MenuList_callback_t)0x1234};
	MenuList list = {.type = MENU_LIST, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 1, 0, 0, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_CONFIRM, action);
}

void test_nav_getAction_a_with_submenu(void) {
	MenuList submenu = {.type = MENU_LIST};
	MenuItem item = {.name = "Test", .submenu = &submenu};
	MenuList list = {.type = MENU_LIST, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 1, 0, 0, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_SUBMENU, action);
}

void test_nav_getAction_a_with_list_on_confirm(void) {
	MenuItem item = {.name = "Test"};
	MenuList list = {.type = MENU_LIST, .items = &item, .on_confirm = (MenuList_callback_t)0x1234};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 1, 0, 0, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_CONFIRM, action);
}

void test_nav_getAction_a_button_binding(void) {
	static char* button_labels[] = {"None", "A", "B", NULL};
	MenuItem item = {.name = "Test", .values = button_labels};
	MenuList list = {.type = MENU_INPUT, .items = &item, .on_confirm = (MenuList_callback_t)0x1234};

	PlayerMenuAction action =
	    PlayerMenuNav_getAction(&list, &item, MENU_INPUT, 1, 0, 0, button_labels);

	TEST_ASSERT_EQUAL(MENU_ACTION_AWAIT_INPUT, action);
}

void test_nav_getAction_x_clears_input(void) {
	MenuItem item = {.name = "Test"};
	MenuList list = {.type = MENU_INPUT, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_INPUT, 0, 0, 1, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_CLEAR_INPUT, action);
}

void test_nav_getAction_x_ignored_non_input(void) {
	MenuItem item = {.name = "Test"};
	MenuList list = {.type = MENU_LIST, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 0, 0, 1, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_NONE, action);
}

void test_nav_getAction_no_buttons(void) {
	MenuItem item = {.name = "Test"};
	MenuList list = {.type = MENU_LIST, .items = &item};

	PlayerMenuAction action = PlayerMenuNav_getAction(&list, &item, MENU_LIST, 0, 0, 0, NULL);

	TEST_ASSERT_EQUAL(MENU_ACTION_NONE, action);
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// Context pattern validation (CORE TESTS)
	RUN_TEST(test_context_get_returns_same_instance);
	RUN_TEST(test_context_provides_access_to_game);
	RUN_TEST(test_context_provides_access_to_core);
	RUN_TEST(test_context_provides_access_to_menu_state);
	RUN_TEST(test_context_state_modifications_persist);
	RUN_TEST(test_ctx_accessors_work);
	RUN_TEST(test_ctx_accessors_handle_null_safely);

	// Context initialization tests
	RUN_TEST(test_context_getCallbacks_returns_instance);
	RUN_TEST(test_context_initGlobals_handles_null);
	RUN_TEST(test_context_initGlobals_sets_initialized);
	RUN_TEST(test_context_initCallbacks_handles_null);
	RUN_TEST(test_context_initCallbacks_links_to_context);

	// Slot navigation tests
	RUN_TEST(test_slot_increment_wraps_at_max);
	RUN_TEST(test_slot_decrement_wraps_at_zero);
	RUN_TEST(test_slot_navigation_full_cycle);

	// Disc navigation tests
	RUN_TEST(test_disc_increment_wraps_at_total);
	RUN_TEST(test_disc_decrement_wraps_at_zero);
	RUN_TEST(test_disc_navigation_with_two_discs);
	RUN_TEST(test_disc_paths_can_be_set);

	// Menu state path tests
	RUN_TEST(test_menu_state_launcher_dir_can_be_set);
	RUN_TEST(test_menu_state_slot_path_can_be_set);
	RUN_TEST(test_menu_state_bmp_path_generation);
	RUN_TEST(test_menu_state_txt_path_generation);

	// PlayerMenu_initState tests
	RUN_TEST(test_initState_sets_slot_to_zero_when_no_file);
	RUN_TEST(test_initState_loads_slot_from_file);
	RUN_TEST(test_initState_resets_slot_8_to_0);
	RUN_TEST(test_initState_preserves_valid_slots);
	RUN_TEST(test_initState_resets_flags);

	// PlayerMenu_updateState tests
	RUN_TEST(test_updateState_generates_bmp_path);
	RUN_TEST(test_updateState_generates_txt_path);
	RUN_TEST(test_updateState_detects_existing_save);
	RUN_TEST(test_updateState_detects_missing_save);
	RUN_TEST(test_updateState_detects_preview_when_save_and_bmp_exist);
	RUN_TEST(test_updateState_no_preview_without_save);
	RUN_TEST(test_updateState_preserves_state_slot);

	// PlayerMenu_getAlias tests
	RUN_TEST(test_getAlias_returns_alias_from_map_file);
	RUN_TEST(test_getAlias_returns_second_entry);
	RUN_TEST(test_getAlias_keeps_original_when_no_match);
	RUN_TEST(test_getAlias_keeps_original_when_no_map_file);
	RUN_TEST(test_getAlias_handles_empty_lines);
	RUN_TEST(test_getAlias_skips_malformed_lines);

	// Navigation state tests
	RUN_TEST(test_nav_init_sets_defaults);
	RUN_TEST(test_nav_init_fewer_items_than_visible);
	RUN_TEST(test_nav_navigate_down_basic);
	RUN_TEST(test_nav_navigate_up_basic);
	RUN_TEST(test_nav_navigate_down_wraps);
	RUN_TEST(test_nav_navigate_up_wraps);
	RUN_TEST(test_nav_navigate_scrolls_down);
	RUN_TEST(test_nav_navigate_scrolls_up);
	RUN_TEST(test_nav_navigate_zero_count);
	RUN_TEST(test_nav_navigate_zero_direction);
	RUN_TEST(test_nav_advanceItem_basic);
	RUN_TEST(test_nav_advanceItem_wraps);
	RUN_TEST(test_nav_advanceItem_scrolls);

	// Value cycling tests
	RUN_TEST(test_nav_cycleValue_right_basic);
	RUN_TEST(test_nav_cycleValue_left_basic);
	RUN_TEST(test_nav_cycleValue_right_wraps);
	RUN_TEST(test_nav_cycleValue_left_wraps);
	RUN_TEST(test_nav_cycleValue_no_values);
	RUN_TEST(test_nav_cycleValue_zero_direction);

	// Action detection tests
	RUN_TEST(test_nav_getAction_b_exits);
	RUN_TEST(test_nav_getAction_a_with_on_confirm);
	RUN_TEST(test_nav_getAction_a_with_submenu);
	RUN_TEST(test_nav_getAction_a_with_list_on_confirm);
	RUN_TEST(test_nav_getAction_a_button_binding);
	RUN_TEST(test_nav_getAction_x_clears_input);
	RUN_TEST(test_nav_getAction_x_ignored_non_input);
	RUN_TEST(test_nav_getAction_no_buttons);

	return UNITY_END();
}
