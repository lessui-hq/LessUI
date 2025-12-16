/**
 * test_launcher_context.c - Unit tests for Launcher context module
 *
 * Tests the context lifecycle and accessor functions.
 */

#include "../../../support/unity/unity.h"
#include "launcher_context.h"

#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

///////////////////////////////
// LauncherContext_get Tests
///////////////////////////////

void test_LauncherContext_get_returns_non_null(void) {
	LauncherContext* ctx = LauncherContext_get();
	TEST_ASSERT_NOT_NULL(ctx);
}

void test_LauncherContext_get_returns_same_instance(void) {
	LauncherContext* ctx1 = LauncherContext_get();
	LauncherContext* ctx2 = LauncherContext_get();
	TEST_ASSERT_EQUAL_PTR(ctx1, ctx2);
}

void test_LauncherContext_get_initializes_callbacks_pointer(void) {
	LauncherContext* ctx = LauncherContext_get();
	TEST_ASSERT_NOT_NULL(ctx->callbacks);
}

///////////////////////////////
// LauncherContext_getCallbacks Tests
///////////////////////////////

void test_LauncherContext_getCallbacks_returns_non_null(void) {
	LauncherCallbacks* callbacks = LauncherContext_getCallbacks();
	TEST_ASSERT_NOT_NULL(callbacks);
}

void test_LauncherContext_getCallbacks_matches_context_callbacks(void) {
	LauncherContext* ctx = LauncherContext_get();
	LauncherCallbacks* callbacks = LauncherContext_getCallbacks();
	// After initCallbacks, they may differ, but initially they match
	TEST_ASSERT_NOT_NULL(callbacks);
}

///////////////////////////////
// LauncherContext_initCallbacks Tests
///////////////////////////////

void test_LauncherContext_initCallbacks_sets_callbacks(void) {
	LauncherContext* ctx = LauncherContext_get();
	LauncherCallbacks my_callbacks = {0};
	my_callbacks.queue_next = (LauncherQueueNextFunc)0x12345678; // Dummy pointer

	LauncherContext_initCallbacks(ctx, &my_callbacks);

	TEST_ASSERT_EQUAL_PTR(&my_callbacks, ctx->callbacks);
	TEST_ASSERT_EQUAL_PTR((void*)0x12345678, ctx->callbacks->queue_next);
}

void test_LauncherContext_initCallbacks_null_ctx_does_not_crash(void) {
	LauncherCallbacks callbacks = {0};
	LauncherContext_initCallbacks(NULL, &callbacks);
	// Should not crash
}

void test_LauncherContext_initCallbacks_null_callbacks_does_not_crash(void) {
	LauncherContext* ctx = LauncherContext_get();
	LauncherCallbacks* original = ctx->callbacks;

	LauncherContext_initCallbacks(ctx, NULL);

	// callbacks should remain unchanged
	TEST_ASSERT_EQUAL_PTR(original, ctx->callbacks);
}

///////////////////////////////
// LauncherContext_initGlobals Tests
///////////////////////////////

void test_LauncherContext_initGlobals_does_not_crash(void) {
	LauncherContext* ctx = LauncherContext_get();
	LauncherContext_initGlobals(ctx);
	// Should not crash - it's a stub
}

void test_LauncherContext_initGlobals_null_does_not_crash(void) {
	LauncherContext_initGlobals(NULL);
	// Should not crash
}

///////////////////////////////
// ctx_getTop Tests
///////////////////////////////

void test_ctx_getTop_returns_null_when_top_null(void) {
	LauncherContext ctx = {0};
	ctx.top = NULL;

	TEST_ASSERT_NULL(ctx_getTop(&ctx));
}

void test_ctx_getTop_returns_null_when_ctx_null(void) {
	TEST_ASSERT_NULL(ctx_getTop(NULL));
}

void test_ctx_getTop_returns_directory(void) {
	Directory* dir = (Directory*)0xABCD1234;
	Directory* top = dir;
	LauncherContext ctx = {0};
	ctx.top = &top;

	TEST_ASSERT_EQUAL_PTR(dir, ctx_getTop(&ctx));
}

///////////////////////////////
// ctx_getStack Tests
///////////////////////////////

void test_ctx_getStack_returns_null_when_stack_null(void) {
	LauncherContext ctx = {0};
	ctx.stack = NULL;

	TEST_ASSERT_NULL(ctx_getStack(&ctx));
}

void test_ctx_getStack_returns_null_when_ctx_null(void) {
	TEST_ASSERT_NULL(ctx_getStack(NULL));
}

void test_ctx_getStack_returns_array(void) {
	Array arr = {0};
	Array* stack = &arr;
	LauncherContext ctx = {0};
	ctx.stack = &stack;

	TEST_ASSERT_EQUAL_PTR(&arr, ctx_getStack(&ctx));
}

///////////////////////////////
// ctx_getRecents Tests
///////////////////////////////

void test_ctx_getRecents_returns_null_when_recents_null(void) {
	LauncherContext ctx = {0};
	ctx.recents = NULL;

	TEST_ASSERT_NULL(ctx_getRecents(&ctx));
}

void test_ctx_getRecents_returns_null_when_ctx_null(void) {
	TEST_ASSERT_NULL(ctx_getRecents(NULL));
}

void test_ctx_getRecents_returns_array(void) {
	Array arr = {0};
	Array* recents = &arr;
	LauncherContext ctx = {0};
	ctx.recents = &recents;

	TEST_ASSERT_EQUAL_PTR(&arr, ctx_getRecents(&ctx));
}

///////////////////////////////
// ctx_isQuitting Tests
///////////////////////////////

void test_ctx_isQuitting_returns_0_when_quit_null(void) {
	LauncherContext ctx = {0};
	ctx.quit = NULL;

	TEST_ASSERT_EQUAL(0, ctx_isQuitting(&ctx));
}

void test_ctx_isQuitting_returns_0_when_ctx_null(void) {
	TEST_ASSERT_EQUAL(0, ctx_isQuitting(NULL));
}

void test_ctx_isQuitting_returns_quit_value(void) {
	int quit = 1;
	LauncherContext ctx = {0};
	ctx.quit = &quit;

	TEST_ASSERT_EQUAL(1, ctx_isQuitting(&ctx));
}

///////////////////////////////
// ctx_setQuit Tests
///////////////////////////////

void test_ctx_setQuit_sets_value(void) {
	int quit = 0;
	LauncherContext ctx = {0};
	ctx.quit = &quit;

	ctx_setQuit(&ctx, 1);

	TEST_ASSERT_EQUAL(1, quit);
}

void test_ctx_setQuit_null_ctx_does_not_crash(void) {
	ctx_setQuit(NULL, 1);
	// Should not crash
}

void test_ctx_setQuit_null_quit_does_not_crash(void) {
	LauncherContext ctx = {0};
	ctx.quit = NULL;

	ctx_setQuit(&ctx, 1);
	// Should not crash
}

///////////////////////////////
// ctx_canResume Tests
///////////////////////////////

void test_ctx_canResume_returns_0_when_null(void) {
	LauncherContext ctx = {0};
	ctx.can_resume = NULL;

	TEST_ASSERT_EQUAL(0, ctx_canResume(&ctx));
}

void test_ctx_canResume_returns_value(void) {
	int can_resume = 1;
	LauncherContext ctx = {0};
	ctx.can_resume = &can_resume;

	TEST_ASSERT_EQUAL(1, ctx_canResume(&ctx));
}

///////////////////////////////
// ctx_setCanResume Tests
///////////////////////////////

void test_ctx_setCanResume_sets_value(void) {
	int can_resume = 0;
	LauncherContext ctx = {0};
	ctx.can_resume = &can_resume;

	ctx_setCanResume(&ctx, 1);

	TEST_ASSERT_EQUAL(1, can_resume);
}

void test_ctx_setCanResume_null_does_not_crash(void) {
	ctx_setCanResume(NULL, 1);
	// Should not crash
}

///////////////////////////////
// ctx_shouldResume Tests
///////////////////////////////

void test_ctx_shouldResume_returns_0_when_null(void) {
	LauncherContext ctx = {0};
	ctx.should_resume = NULL;

	TEST_ASSERT_EQUAL(0, ctx_shouldResume(&ctx));
}

void test_ctx_shouldResume_returns_value(void) {
	int should_resume = 1;
	LauncherContext ctx = {0};
	ctx.should_resume = &should_resume;

	TEST_ASSERT_EQUAL(1, ctx_shouldResume(&ctx));
}

///////////////////////////////
// ctx_setShouldResume Tests
///////////////////////////////

void test_ctx_setShouldResume_sets_value(void) {
	int should_resume = 0;
	LauncherContext ctx = {0};
	ctx.should_resume = &should_resume;

	ctx_setShouldResume(&ctx, 1);

	TEST_ASSERT_EQUAL(1, should_resume);
}

void test_ctx_setShouldResume_null_does_not_crash(void) {
	ctx_setShouldResume(NULL, 1);
	// Should not crash
}

///////////////////////////////
// Test Runner
///////////////////////////////

int main(void) {
	UNITY_BEGIN();

	// LauncherContext_get tests
	RUN_TEST(test_LauncherContext_get_returns_non_null);
	RUN_TEST(test_LauncherContext_get_returns_same_instance);
	RUN_TEST(test_LauncherContext_get_initializes_callbacks_pointer);

	// LauncherContext_getCallbacks tests
	RUN_TEST(test_LauncherContext_getCallbacks_returns_non_null);
	RUN_TEST(test_LauncherContext_getCallbacks_matches_context_callbacks);

	// LauncherContext_initCallbacks tests
	RUN_TEST(test_LauncherContext_initCallbacks_sets_callbacks);
	RUN_TEST(test_LauncherContext_initCallbacks_null_ctx_does_not_crash);
	RUN_TEST(test_LauncherContext_initCallbacks_null_callbacks_does_not_crash);

	// LauncherContext_initGlobals tests
	RUN_TEST(test_LauncherContext_initGlobals_does_not_crash);
	RUN_TEST(test_LauncherContext_initGlobals_null_does_not_crash);

	// ctx_getTop tests
	RUN_TEST(test_ctx_getTop_returns_null_when_top_null);
	RUN_TEST(test_ctx_getTop_returns_null_when_ctx_null);
	RUN_TEST(test_ctx_getTop_returns_directory);

	// ctx_getStack tests
	RUN_TEST(test_ctx_getStack_returns_null_when_stack_null);
	RUN_TEST(test_ctx_getStack_returns_null_when_ctx_null);
	RUN_TEST(test_ctx_getStack_returns_array);

	// ctx_getRecents tests
	RUN_TEST(test_ctx_getRecents_returns_null_when_recents_null);
	RUN_TEST(test_ctx_getRecents_returns_null_when_ctx_null);
	RUN_TEST(test_ctx_getRecents_returns_array);

	// ctx_isQuitting tests
	RUN_TEST(test_ctx_isQuitting_returns_0_when_quit_null);
	RUN_TEST(test_ctx_isQuitting_returns_0_when_ctx_null);
	RUN_TEST(test_ctx_isQuitting_returns_quit_value);

	// ctx_setQuit tests
	RUN_TEST(test_ctx_setQuit_sets_value);
	RUN_TEST(test_ctx_setQuit_null_ctx_does_not_crash);
	RUN_TEST(test_ctx_setQuit_null_quit_does_not_crash);

	// ctx_canResume tests
	RUN_TEST(test_ctx_canResume_returns_0_when_null);
	RUN_TEST(test_ctx_canResume_returns_value);

	// ctx_setCanResume tests
	RUN_TEST(test_ctx_setCanResume_sets_value);
	RUN_TEST(test_ctx_setCanResume_null_does_not_crash);

	// ctx_shouldResume tests
	RUN_TEST(test_ctx_shouldResume_returns_0_when_null);
	RUN_TEST(test_ctx_shouldResume_returns_value);

	// ctx_setShouldResume tests
	RUN_TEST(test_ctx_setShouldResume_sets_value);
	RUN_TEST(test_ctx_setShouldResume_null_does_not_crash);

	return UNITY_END();
}
