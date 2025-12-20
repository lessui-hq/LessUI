# LessUI Test Suite

This directory contains the test suite for LessUI, organized to mirror the source code structure.

**Current Status: 1470 tests, all passing** ✅

## Quick Start

```bash
make test        # Run all 1470 tests in Docker (recommended)
make test-asan   # Run with AddressSanitizer + UBSan (catches memory bugs)
```

Tests run in an Ubuntu 24.04 Docker container. This ensures consistency across development environments and catches platform-specific issues.

## Test Environment

**Docker Container Specifications:**
- Base: Ubuntu 24.04 LTS
- Compiler: Modern GCC
- Complete QA toolchain (clang-tidy, clang-format, shellcheck)
- SDL2 development libraries included
- Dockerfile: `Dockerfile`

**Why Docker?**
- Consistent environment across all development machines
- Matches GitHub Actions CI environment
- Eliminates platform-specific differences
- No need to install native build tools on macOS

## Directory Structure

```
tests/
├── unit/                           # Unit tests (mirror workspace/ structure)
│   └── all/
│       ├── common/                 # Tests for workspace/all/common/
│       │   ├── test_utils.c              # Utils (string, file, name, date, math)
│       │   ├── test_api_pad.c            # Input state machine
│       │   ├── test_gfx_text.c           # Text truncation/wrapping
│       │   ├── test_audio_resampler.c    # Audio resampling
│       │   ├── test_binary_file_utils.c  # Binary file I/O
│       │   ├── test_effect_system.c      # Visual effect state
│       │   ├── test_platform_variant.c   # Platform detection
│       │   └── ...                       # Other common module tests
│       ├── launcher/               # Tests for workspace/all/launcher/
│       │   ├── test_launcher_utils.c     # Launcher helpers
│       │   ├── test_launcher_entry.c     # Entry type, sorting
│       │   ├── test_launcher_state.c     # State persistence
│       │   ├── test_m3u_parser.c         # M3U parsing
│       │   ├── test_map_parser.c         # map.txt aliasing
│       │   ├── test_collection_parser.c  # Collection lists
│       │   ├── test_recent_*.c           # Recent games (read/write/runtime)
│       │   ├── test_directory_index.c    # Directory indexing
│       │   └── ...                       # Other launcher module tests
│       └── player/                 # Tests for workspace/all/player/
│           ├── test_player_paths.c       # Save file paths
│           ├── test_player_utils.c       # Player utilities
│           ├── test_player_config.c      # Config path generation
│           ├── test_player_options.c     # Option management
│           ├── test_player_cpu.c         # CPU scaling algorithm
│           ├── test_player_menu.c        # Menu system
│           ├── test_player_env.c         # Environment callbacks
│           └── ...                       # Other player module tests
├── integration/                    # Integration tests (end-to-end tests)
├── fixtures/                       # Test data, sample ROMs, configs
├── vendor/                         # Third-party test libraries
│   ├── unity/                      # Unity test framework
│   └── fff/                        # Fake Function Framework (SDL mocking)
│       └── fff.h                   # Header-only mocking library
├── support/                        # Test infrastructure
│   ├── platform.h                  # Platform stubs for testing
│   ├── sdl_stubs.h                 # Minimal SDL type definitions
│   ├── sdl_fakes.h/c               # SDL function mocks (fff-based)
│   ├── platform_mocks.h/c          # Platform function mocks
│   ├── fs_mocks.h/c                # File system mocks (--wrap-based)
│   ├── test_helpers.h/c            # Setup/teardown utilities
│   └── test_temp.h/c               # Temp file/directory management
└── README.md                       # This file
```

## Organization Philosophy

### Mirror Structure
Tests mirror the source code structure under `workspace/`:

```
workspace/all/common/utils/utils.c        →  tests/unit/all/common/test_utils.c
workspace/all/common/utils/string_utils.c →  tests/unit/all/common/test_string_utils.c
workspace/all/common/utils/file_utils.c   →  tests/unit/all/common/test_file_utils.c
workspace/all/common/utils/name_utils.c   →  tests/unit/all/common/test_name_utils.c
workspace/all/common/utils/date_utils.c   →  tests/unit/all/common/test_date_utils.c
workspace/all/common/utils/math_utils.c   →  tests/unit/all/common/test_math_utils.c
workspace/all/launcher/launcher.c               →  tests/unit/all/launcher/test_launcher.c
```

This makes it easy to:
- Find tests for any source file
- Maintain consistency as the codebase grows
- Understand test coverage at a glance

### Test Types

**Unit Tests** (`unit/`)
- Test individual functions in isolation
- Fast execution
- Mock external dependencies
- Example: Testing string manipulation in `utils.c`

**Integration Tests** (`integration/`)
- Test multiple components working together
- Test real workflows (launch a game, save state, etc.)
- May be slower to execute

**Fixtures** (`fixtures/`)
- Sample ROM files
- Test configuration files
- Expected output data

**Support** (`support/`)
- Test frameworks (Unity)
- Shared test utilities
- Platform stubs

## Running Tests

### Docker-Based Testing (Default)

Tests run in an Ubuntu 24.04 container. This eliminates macOS-specific build issues and ensures consistency across all development environments.

```bash
# Run all tests (uses Docker automatically)
make test

# Enter Docker container for debugging
make -f Makefile.qa docker-shell

# Rebuild Docker image
make -f Makefile.qa docker-build
```

### Native Testing (Advanced)

Run tests directly on your host system (not recommended on macOS due to architecture differences):

```bash
# Run all tests natively
make -f Makefile.qa test-native

# Run individual test executables
./tests/utils_test         # Timing tests (2 tests)
./tests/string_utils_test  # String tests (35 tests)
./tests/file_utils_test    # File I/O tests (10 tests)
./tests/name_utils_test    # Name processing tests (10 tests)
./tests/date_utils_test    # Date/time tests (30 tests)
./tests/math_utils_test    # Math tests (13 tests)

# Run with verbose output
./tests/string_utils_test -v

# Run specific test
./tests/utils_test -n test_getMicroseconds_non_zero
./tests/string_utils_test -n test_prefixMatch_exact
./tests/file_utils_test -n test_exists_file_exists
./tests/name_utils_test -n test_getDisplayName_simple
./tests/date_utils_test -n test_isLeapYear_divisible_by_4
./tests/math_utils_test -n test_gcd_common_divisor
```

### Clean and Rebuild
```bash
make -f Makefile.qa clean-tests
make test
```

## Writing New Tests

### 1. Mirror the Source Structure

If adding tests for `workspace/all/launcher/launcher.c`:

```bash
# Create directory
mkdir -p tests/unit/all/launcher

# Create test file
touch tests/unit/all/launcher/test_launcher.c
```

### 2. Use Unity Framework

```c
#include "../../../../workspace/all/launcher/launcher.h"
#include "unity.h"  // Via -I tests/vendor/unity include path

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

void test_something(void) {
    TEST_ASSERT_EQUAL_INT(42, my_function());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_something);
    return UNITY_END();
}
```

### 3. Update Makefile.qa

Add your test to the build:

```makefile
tests/unit_tests: tests/unit/all/launcher/test_launcher.c ...
    @$(CC) -o $@ $^ ...
```

### 4. Common Assertions

```c
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_STRING("expected", actual)
TEST_ASSERT_NOT_NULL(pointer)
TEST_ASSERT_NULL(pointer)
```

See `vendor/unity/unity.h` for full list.

## SDL Function Mocking

LessUI uses SDL extensively for graphics, input, and audio. Testing SDL-dependent code requires **mocking** SDL functions. We use the **Fake Function Framework (fff)** for this.

### Infrastructure Overview

```
tests/
├── vendor/
│   └── fff/
│       └── fff.h          # Fake Function Framework (MIT licensed)
└── support/
    ├── sdl_stubs.h        # Minimal SDL type definitions
    ├── sdl_fakes.h        # fff-based SDL function fakes (declarations)
    ├── sdl_fakes.c        # fff-based SDL function fakes (definitions)
    ├── platform_mocks.h   # Mock PLAT_* interface
    └── platform_mocks.c   # Mock PLAT_* implementations
```

### What is fff?

The **Fake Function Framework** is a header-only C mocking library that generates mockable versions of functions using macros. It provides:

- **Call tracking** - Counts how many times a function was called
- **Argument history** - Records arguments from the last 50 calls
- **Return value control** - Set return values or sequences
- **Custom implementations** - Provide custom fake behavior

### Using fff for SDL Mocking

#### Example: Mocking SDL_PollEvent

```c
#include "unity.h"      // Via -I tests/vendor/unity
#include "fff.h"        // Via -I tests/vendor/fff
#include "sdl_stubs.h"  // Via -I tests/support
#include "sdl_fakes.h"

DEFINE_FFF_GLOBALS;

void setUp(void) {
    // Reset fakes before each test
    RESET_FAKE(SDL_PollEvent);
    FFF_RESET_HISTORY();
}

void test_event_handling(void) {
    // Configure mock to return "has event"
    SDL_PollEvent_fake.return_val = 1;

    SDL_Event event;
    int result = SDL_PollEvent(&event);

    // Verify behavior
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_INT(1, SDL_PollEvent_fake.call_count);
}
```

#### Example: Custom Fake Implementation

```c
// Custom implementation that calculates text width
int mock_TTF_SizeUTF8(TTF_Font* font, const char* text, int* w, int* h) {
    if (w) *w = strlen(text) * 10;  // 10 pixels per character
    if (h) *h = font->point_size;
    return 0;
}

void test_text_width_calculation(void) {
    // Use custom fake
    TTF_SizeUTF8_fake.custom_fake = mock_TTF_SizeUTF8;

    TTF_Font font = {.point_size = 16};
    int width, height;

    TTF_SizeUTF8(&font, "Hello", &width, &height);

    TEST_ASSERT_EQUAL_INT(50, width);   // 5 chars * 10px
    TEST_ASSERT_EQUAL_INT(16, height);  // Font size
}
```

#### Example: Return Value Sequences

```c
void test_multiple_events(void) {
    // Configure sequence: event, event, no more events
    static int return_sequence[] = {1, 1, 0};
    SET_RETURN_SEQ(SDL_PollEvent, return_sequence, 3);

    SDL_Event event;
    TEST_ASSERT_EQUAL_INT(1, SDL_PollEvent(&event));  // First event
    TEST_ASSERT_EQUAL_INT(1, SDL_PollEvent(&event));  // Second event
    TEST_ASSERT_EQUAL_INT(0, SDL_PollEvent(&event));  // No more
}
```

### Available SDL Fakes

**Currently defined:**
- `SDL_PollEvent` - Event polling
- `TTF_SizeUTF8` - Text size calculation

**To add more fakes:**

1. Add declaration in `tests/support/sdl_fakes.h`:
```c
DECLARE_FAKE_VALUE_FUNC(int, SDL_BlitSurface,
                        SDL_Surface*, SDL_Rect*, SDL_Surface*, SDL_Rect*);
```

2. Add definition in `tests/support/sdl_fakes.c`:
```c
DEFINE_FAKE_VALUE_FUNC(int, SDL_BlitSurface,
                       SDL_Surface*, SDL_Rect*, SDL_Surface*, SDL_Rect*);
```

3. Reset in your test's `setUp()`:
```c
RESET_FAKE(SDL_BlitSurface);
```

### Platform Mocks

For mocking platform-specific `PLAT_*` functions, use `platform_mocks.c`:

```c
#include "platform_mocks.h"  // Via -I tests/support

void test_battery_status(void) {
    // Configure mock battery state
    mock_set_battery(75, 1);  // 75% charge, charging

    // Call code that uses PLAT_getBatteryStatus()
    update_battery_display();

    // Verify behavior
    // ...
}
```

### Complete Example

See `tests/unit/all/common/test_gfx_text.c` for a comprehensive demonstration of fff usage with custom fake implementations.

### When to Extract vs. Mock

**Extract to separate module** (like `pad.c`, `gfx_text.c`) when:
- Logic is pure (no SDL dependencies)
- Functions can be reused across components
- Code is tightly coupled to complex global state

**Use SDL mocking** when:
- Testing functions that directly call SDL
- Verifying interaction with SDL APIs
- Testing error handling (simulating SDL failures)

**Skip testing** when:
- Function is trivial (simple getter/setter)
- Logic is entirely SDL rendering (test visually instead)
- Would require massive extraction effort for minimal value

### fff Documentation

For more fff features, see:
- [fff GitHub](https://github.com/meekrosoft/fff)
- `tests/vendor/fff/fff.h` (inline documentation)
- `tests/unit/all/common/test_gfx_text.c` (real-world usage examples)

## File System Function Mocking

Testing file I/O code requires mocking file system operations. We use **GCC's --wrap linker flag** for this.

### Infrastructure Overview

```
tests/support/
├── fs_mocks.h             # File mocking API
└── fs_mocks.c             # File mocking implementation
```

### What is --wrap?

GCC's linker supports `--wrap=symbol` which intercepts function calls:

```bash
gcc ... -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fgets
```

**How it works:**
- Calls to `fopen()` are redirected to `__wrap_fopen()` (your mock)
- Your mock can call `__real_fopen()` for the real implementation
- No code changes needed - pure link-time substitution

**Platform support:**
- ✅ Linux (GCC/GNU ld)
- ✅ Docker (Ubuntu 24.04 with GCC) - **This is how we test**
- ❌ macOS (uses ld64, doesn't support --wrap)

### Using File Mocking

#### Example: Testing M3U Parser

```c
#include "unity.h"      // Via -I tests/vendor/unity
#include "fs_mocks.h"   // Via -I tests/support
#include "../../../../workspace/all/launcher/launcher_m3u.h"

void setUp(void) {
    // Reset mock file system before each test
    mock_fs_reset();
}

void test_parse_m3u_file(void) {
    // Create mock files
    mock_fs_add_file("/Roms/PS1/FF7.m3u",
        "FF7 (Disc 1).bin\nFF7 (Disc 2).bin\n");
    mock_fs_add_file("/Roms/PS1/FF7 (Disc 1).bin", "disc data");

    // Call production code - it uses real fopen/fgets
    char disc_path[256];
    int found = M3U_getFirstDisc("/Roms/PS1/FF7.m3u", disc_path);

    // But it's actually reading from our mock!
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("/Roms/PS1/FF7 (Disc 1).bin", disc_path);
}
```

#### Available Mock Functions

**Currently wrapped:**
- `exists(char* path)` - LessUI's file existence check
- `fopen(const char* path, const char* mode)` - Open files (read mode only)
- `fclose(FILE* stream)` - Close files
- `fgets(char* s, int size, FILE* stream)` - Read lines

**Mock API:**
```c
void mock_fs_reset(void);                           // Clear all mock files
void mock_fs_add_file(const char* path, const char* content);  // Add mock file
int mock_fs_exists(const char* path);               // Check if mock file exists
```

### Complete Examples

**Real-world usage:**
- `tests/unit/all/launcher/test_m3u_parser.c` - M3U playlist parsing (read-only with mocking)
- `tests/unit/all/launcher/test_launcher_file_utils.c` - File existence checking
- `tests/unit/all/launcher/test_recent_writer.c` - File writing with real temp files

### Compilation Requirements

Tests using file mocking must be compiled with --wrap flags:

```makefile
tests/my_test: test_my_test.c my_code.c utils.c fs_mocks.c $(TEST_UNITY)
	@$(CC) -o $@ $^ $(TEST_INCLUDES) $(TEST_CFLAGS) \
		-Wl,--wrap=exists \
		-Wl,--wrap=fopen \
		-Wl,--wrap=fclose \
		-Wl,--wrap=fgets
```

### When to Use File Mocking

**Use file mocking when:**
- Testing file parsing logic (map.txt, .m3u, .cue files)
- Testing path construction with existence checks
- Testing functions that read configuration files
- Verifying error handling (file not found, empty files)

**Don't use file mocking when:**
- Function only manipulates paths (use pure string functions instead)
- Testing actual file I/O performance
- Integration testing (use real temp files)

### Limitations

**Current implementation:**
- **Read mode only** - Supports exists(), fopen("r"), fgets(), fclose()
- Files stored as strings in memory (max 8KB per file, 100 files total)
- **Docker-only** - Requires GCC --wrap (won't compile on macOS with clang/ld64)
- Comprehensive test coverage for all text file parsers (map.txt, M3U, collections, recent.txt)

**Limitations and alternatives:**
- **Write operations** (fputs, fprintf, fwrite) - Use real temp files instead
- **Directory operations** (opendir, readdir) - Use real temp directories instead
- **Binary I/O** (fread, fwrite with binary data) - Use real temp files instead

Using real temp files for these operations is more reliable, works cross-platform,
and avoids the complexity of mocking glibc's internal FILE structure validation.

### Example: Testing with Temp Files

For write operations or directory testing, use mktemp and cleanup:

```c
void test_saveRecents(void) {
    // Create temp file
    char temp_path[] = "/tmp/recent_test_XXXXXX";
    int fd = mkstemp(temp_path);
    TEST_ASSERT_TRUE(fd >= 0);
    close(fd);

    // Test the write operation
    saveRecents(temp_path);

    // Read back and verify
    FILE* f = fopen(temp_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    // ... verify content ...
    fclose(f);

    // Cleanup
    unlink(temp_path);
}
```

## Test Helpers (Setup/Teardown)

The test suite provides centralized helper functions to prevent test pollution and ensure proper cleanup.

### Infrastructure Overview

```
tests/support/
├── test_helpers.h         # Central reset utilities
├── test_helpers.c         # Reset function implementations
├── test_temp.h            # Temp file/directory management
└── test_temp.c            # Temp file implementations
```

### Why Use Test Helpers?

Test pollution occurs when state from one test leaks into another, causing:
- Intermittent failures ("works on my machine")
- Order-dependent test results
- Difficult-to-debug failures

The helper functions ensure tests are isolated by resetting all mock state and cleaning up resources.

### Basic Usage

```c
#include "unity.h"
#include "test_helpers.h"

void setUp(void) {
    test_reset_all();  // Reset SDL fakes, FS mocks, fff history
}

void tearDown(void) {
    test_cleanup_all();  // Clean up temp files
}

void test_example(void) {
    // Your test code here
}
```

### Selective Reset Functions

If you only need specific resets:

```c
void setUp(void) {
    test_reset_sdl_fakes();   // Only reset SDL mocks
    test_reset_fs_mocks();    // Only reset file system mocks
    test_reset_fff_history(); // Only reset fff call history
}
```

### Available Functions

| Function | Purpose |
|----------|---------|
| `test_reset_all()` | Reset all mock state (SDL fakes, FS mocks, fff history) |
| `test_cleanup_all()` | Clean up all temp files/directories |
| `test_reset_sdl_fakes()` | Reset SDL function mocks |
| `test_reset_fs_mocks()` | Reset file system mocks |
| `test_reset_fff_history()` | Reset fff call sequence history |

## Temp File Management

The `test_temp.h` module provides safe, auto-cleaned temp file creation.

### Benefits

- **No fixed paths** - Unique paths prevent collisions in parallel test runs
- **Automatic cleanup** - Files cleaned even when tests fail
- **Cross-platform** - Works on Linux and macOS

### Creating Temp Files

```c
#include "test_temp.h"

void tearDown(void) {
    test_temp_cleanup();  // Required: clean up temp files
}

void test_file_operations(void) {
    // Create empty temp file
    const char* path = test_temp_file(".txt");

    // Create temp file with content
    const char* config = test_temp_file_with_content(".cfg", "option=value\n");

    // Create temp file with binary data
    uint8_t sram[8192] = {0};
    const char* save = test_temp_file_with_binary(".sav", sram, sizeof(sram));

    // Files are automatically cleaned in tearDown()
}
```

### Creating Temp Directories

```c
void test_directory_operations(void) {
    // Create temp directory
    const char* dir = test_temp_dir();

    // Create nested subdirectory
    const char* roms_dir = test_temp_subdir(dir, "Roms/GB");
    // roms_dir is now "/tmp/test_dir_XXXXXX/Roms/GB"

    // Create file inside temp directory
    const char* rom = test_temp_create_file(dir, "Roms/GB/game.gb", "ROM DATA");

    // All cleaned automatically in tearDown()
}
```

### Available Functions

| Function | Purpose |
|----------|---------|
| `test_temp_file(suffix)` | Create empty temp file with optional suffix |
| `test_temp_file_with_content(suffix, content)` | Create temp file with text content |
| `test_temp_file_with_binary(suffix, data, size)` | Create temp file with binary data |
| `test_temp_dir()` | Create empty temp directory |
| `test_temp_subdir(base, subpath)` | Create nested directory under temp dir |
| `test_temp_create_file(dir, filename, content)` | Create file inside temp directory |
| `test_temp_cleanup()` | Remove all tracked temp resources |
| `test_temp_count()` | Get count of tracked resources (for debugging) |

## Memory Sanitizer Testing

The test suite supports AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) to catch memory bugs.

### What Sanitizers Detect

**AddressSanitizer (ASan):**
- Buffer overflows (heap, stack, global)
- Use-after-free
- Use-after-return
- Memory leaks
- Double-free

**UndefinedBehaviorSanitizer (UBSan):**
- Integer overflow
- Null pointer dereference
- Misaligned memory access
- Invalid shift operations

### Running Tests with Sanitizers

```bash
# Run all tests with ASan + UBSan (Docker, recommended)
make test-asan

# Or explicitly via Docker
make docker-test-asan

# Native (Linux hosts with GCC only)
make -f Makefile.qa test-asan-native
```

### Example Output

When ASan detects an issue, you'll see detailed output:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in my_function /path/to/file.c:42
    #1 0x... in test_my_function /path/to/test.c:15
    ...
```

### When to Use Sanitizers

- **During development** - Catch bugs early
- **Before releases** - Comprehensive memory checking
- **Investigating crashes** - Get detailed stack traces
- **CI integration** - Add `make test-asan` to your workflow

### Notes

- Sanitizers add ~2x slowdown but catch critical bugs
- Requires GCC (runs in Docker for consistency)
- Leak detection is enabled by default
- Tests continue on errors (doesn't halt at first issue)

## Test Guidelines

### Good Test Characteristics
1. **Fast** - Unit tests should run in milliseconds
2. **Isolated** - No dependencies on other tests
3. **Repeatable** - Same result every time
4. **Self-checking** - Asserts verify correctness
5. **Timely** - Written alongside the code

### What to Test
- **Happy paths** - Normal, expected usage
- **Edge cases** - Boundary conditions (empty strings, NULL, max values)
- **Error cases** - Invalid input, file not found, etc.
- **Regression tests** - Known bugs that were fixed

### Example Test Coverage

From `test_utils.c`:
```c
// Happy path
void test_prefixMatch_exact(void) {
    TEST_ASSERT_TRUE(prefixMatch("hello", "hello"));
}

// Edge case
void test_prefixMatch_empty(void) {
    TEST_ASSERT_TRUE(prefixMatch("", "anything"));
}

// Error case
void test_exactMatch_null_strings(void) {
    TEST_ASSERT_FALSE(exactMatch(NULL, "hello"));
}

// Regression test
void test_getEmuName_with_parens(void) {
    // This previously crashed due to overlapping memory
    char out[512];
    getEmuName("test (GB).gb", out);
    TEST_ASSERT_EQUAL_STRING("GB", out);
}
```

## Test Summary

**Total: 1470 tests across 47 test suites**

### Extracted Modules (Testable Logic)

These modules were extracted from large files (api.c, launcher.c, player.c) to enable comprehensive unit testing:

| Module | Lines | Tests | Extracted From | Key Functions |
|--------|-------|-------|----------------|---------------|
| utils.c | 703 | 123 | (original) | String, file, name, date, math utilities |
| ui_layout.c | ~100 | 20 | api.c | UI layout calculations (DP scaling, pill heights) |
| str_compare.c | 119 | 28 | (original) | Natural string sorting, article handling |
| nointro_parser.c | 266 | 39 | (original) | No-Intro ROM naming conventions |
| effect_system.c | 106 | 43 | platform files | Visual effect state management |
| player_utils.c | ~120 | 41 | player.c | Core name extraction, option search, string utils |
| player_config.c | ~50 | 19 | player.c | Config path generation, option name mapping |
| player_options.c | ~70 | 36 | player.c | Option list search and manipulation |
| pad.c | 183 | 36 | api.c | Button state machine, analog input |
| gfx_text.c | 170 | 32 | api.c | Text truncation, wrapping, sizing |
| launcher_file_utils.c | 130 | 25 | launcher.c | File/dir checking (hasEmu, hasCue, hasM3u) |
| map_parser.c | 64 | 22 | launcher.c/player.c | ROM display name aliasing (map.txt) |
| m3u_parser.c | 132 | 20 | launcher.c | M3U playlist parsing |
| audio_resampler.c | 162 | 20 | api.c | Sample rate conversion |
| recent_file.c | 95 | 18 | launcher.c | Recent games read/write |
| launcher_utils.c | 48 | 17 | launcher.c | Index char, console dir detection |
| player_paths.c | 77 | 16 | player.c | Save file path generation |
| platform_variant.c | 67 | 14 | (original) | Platform variant detection |
| binary_file_utils.c | 42 | 12 | player.c | Binary file read/write |
| collection_parser.c | 70 | 11 | launcher.c | Custom ROM list parsing |
| **Total** | **~2,870** | **622** | | |

### Testing Technologies

**Mocking Frameworks:**
- **fff (Fake Function Framework)** - SDL function mocking (header-only)
- **GCC --wrap** - File system function mocking for reads (link-time substitution)

**Testing Approaches:**
- **File mocking (--wrap)**: Read-only text file operations (exists, fopen("r"), fgets)
- **Real temp files**: Write operations (mkstemp + fopen("w"), fputs, fwrite)
- **Real temp directories**: Directory operations (mkdtemp + opendir, readdir)

**What We Can Test:**
- SDL functions (SDL_PollEvent, TTF_SizeUTF8, etc.) - fff mocks
- Platform functions (PLAT_getBatteryStatus, PLAT_pollInput, etc.) - fff mocks
- Text file parsing (map.txt, M3U, collections, recent.txt) - file mocking
- File writing (Recent_save) - real temp files
- Binary file I/O (fread/fwrite) - real temp files
- Directory checking (hasNonHiddenFiles) - real temp directories

## Current Test Coverage

### workspace/all/common/utils/utils.c - ✅ 2 tests
**File:** `tests/unit/all/common/test_utils.c`

- Timing (getMicroseconds)

**Coverage:** Timing functions tested for non-zero values and monotonicity.

### workspace/all/common/utils/string_utils.c - ✅ 35 tests
**File:** `tests/unit/all/common/test_string_utils.c`

- String matching (prefixMatch, suffixMatch, exactMatch, containsString, hide)
- String manipulation (normalizeNewline, trimTrailingNewlines, trimSortingMeta)
- Text parsing (splitTextLines)

**Coverage:** All functions tested with happy paths, edge cases, and error conditions.

### workspace/all/common/utils/file_utils.c - ✅ 10 tests
**File:** `tests/unit/all/common/test_file_utils.c`

- File existence checking (exists)
- File creation (touch)
- File I/O (putFile, getFile, allocFile)
- Integer file I/O (putInt, getInt)

**Coverage:** All file I/O functions tested including edge cases and error conditions.

### workspace/all/common/utils/name_utils.c - ✅ 10 tests
**File:** `tests/unit/all/common/test_name_utils.c`

- Display name processing (getDisplayName) - strips paths, extensions, region codes
- Emulator name extraction (getEmuName) - extracts from ROM paths

**Coverage:** All name processing functions tested with various input formats.

### workspace/all/common/utils/date_utils.c - ✅ 30 tests
**File:** `tests/unit/all/common/test_date_utils.c`

- Leap year calculation (isLeapYear)
- Days in month logic with leap year support (getDaysInMonth)
- Date/time validation and normalization (validateDateTime)
  - Month wrapping (1-12)
  - Year clamping (1970-2100)
  - Day validation (handles varying month lengths and leap years)
  - Time wrapping (hours, minutes, seconds)
- 12-hour time conversion (convertTo12Hour)

**Coverage:** Complete coverage of date/time validation logic.

**Note:** Logic was extracted from `clock.c` into a proper utility library.

### workspace/all/common/utils/math_utils.c - ✅ 13 tests
**File:** `tests/unit/all/common/test_math_utils.c`

- Greatest common divisor (gcd) - Euclidean algorithm
- 16-bit color averaging (average16) - RGB565 pixel blending
- 32-bit color averaging (average32) - RGBA8888 pixel blending with overflow handling

**Coverage:** Pure math functions with edge cases and real-world scenarios.

**Note:** Extracted from `api.c` for reusability and testability.

### workspace/all/common/pad.c - ✅ 21 tests
**File:** `tests/unit/all/common/test_api_pad.c`

- Button state management (PAD_reset, PAD_setAnalog)
- Analog stick deadzone handling
- Opposite direction cancellation (left/right, up/down)
- Button repeat timing
- Query functions (anyJustPressed, justPressed, isPressed, justReleased, justRepeated)
- Menu tap detection (PAD_tappedMenu)

**Coverage:** Complete coverage of input state machine logic.

**Note:** Extracted from `api.c` for testability without SDL dependencies.

### workspace/all/common/gfx_text.c - ✅ 32 tests
**File:** `tests/unit/all/common/test_gfx_text.c`

- GFX_truncateText() - Text truncation with ellipsis (8 tests)
- GFX_wrapText() - Multi-line text wrapping (16 tests)
- GFX_sizeText() - Multi-line text bounding box calculation (8 tests)

**Coverage:** Complete coverage of text manipulation algorithms.

**Note:** Extracted from `api.c`, uses fff to mock TTF_SizeUTF8.

### workspace/all/common/audio_resampler.c - ✅ 18 tests
**File:** `tests/unit/all/common/test_audio_resampler.c`

- Nearest-neighbor sample rate conversion
- Bresenham-like algorithm for frame duplication/skipping
- Upsampling (44100 -> 48000 Hz)
- Downsampling (48000 -> 44100 Hz)
- Realistic scenarios (1 second of audio)
- Ring buffer integration

**Coverage:** Complete coverage of audio resampling algorithm.

**Note:** Extracted from `api.c`'s SND_resampleNear(), pure algorithm with no SDL dependencies.

### workspace/all/common/player_paths.c - ✅ 16 tests
**File:** `tests/unit/all/common/test_player_paths.c`

- SRAM save file path generation (.sav files)
- RTC file path generation (.rtc files)
- Save state path generation (.st0-.st9 files)
- Config file path generation (.cfg files with device tags)

**Coverage:** Complete coverage of Player save file path logic.

**Note:** Extracted from `player.c`, pure sprintf logic.

### workspace/all/common/launcher_utils.c - ✅ 17 tests
**File:** `tests/unit/all/common/test_launcher_utils.c`

- LessUI_getIndexChar() - Alphabetical indexing for ROM navigation (7 tests)
- LessUI_isConsoleDir() - Console directory classification (8 tests)
- Integration tests (2 tests)

**Coverage:** Complete coverage of LessUI helper utilities.

**Note:** Extracted from `launcher.c`, pure string logic.

### workspace/all/common/m3u_parser.c - ✅ 20 tests
**File:** `tests/unit/all/common/test_m3u_parser.c`

- M3U_getFirstDisc() - First disc extraction (8 tests)
- M3U_getAllDiscs() - Full disc list parsing (12 tests)
- Empty line handling
- Windows newline support
- Path construction (relative to M3U location)
- Error handling (missing files, empty playlists)
- Disc naming and numbering

**Coverage:** Complete coverage of M3U parsing logic (getFirstDisc + getAllDiscs).

**Note:** Extracted from `launcher.c`, uses file system mocking.

### workspace/all/common/launcher_file_utils.c - ✅ 25 tests
**Files:**
- `tests/unit/all/common/test_launcher_file_utils.c` (18 tests)
- `tests/unit/all/common/test_directory_utils.c` (7 tests)

**File existence checking (18 tests):**
- LessUI_hasEmu() - Emulator availability checking (5 tests)
- LessUI_hasCue() - CUE file detection for disc games (4 tests)
- LessUI_hasM3u() - M3U playlist detection (5 tests)
- Integration tests (multi-disc workflow) (4 tests)

**Directory content checking (7 tests):**
- LessUI_hasNonHiddenFiles() - Directory content checking
- Empty directory detection
- Hidden file filtering (.dotfiles, .DS_Store, etc.)
- Mixed content (hidden + visible files)
- Subdirectory handling
- Nonexistent directory error handling

**Coverage:** Complete coverage of file and directory checking utilities.

**Note:** Extracted from `launcher.c` hasEmu/hasCue/hasM3u/hasCollections/hasRoms. File tests use mocking, directory tests use real temp directories with mkdtemp().

### workspace/all/common/map_parser.c - ✅ 22 tests
**File:** `tests/unit/all/common/test_map_parser.c`

- Map_getAlias() - ROM display name aliasing
- Tab-delimited format parsing (filename<TAB>display name)
- Hidden ROM detection (alias starts with '.')
- Basic parsing (with and without aliases)
- Integration tests (real-world map.txt files)
- Error handling (file not found, empty files, missing ROMs)

**Coverage:** Complete coverage of map.txt parsing logic.

**Note:** Extracted from `launcher.c`'s Directory_index() and `player.c`'s getAlias(), uses file system mocking.

### workspace/all/common/collection_parser.c - ✅ 11 tests
**File:** `tests/unit/all/common/test_collection_parser.c`

- Collection_parse() - Custom ROM list parsing
- Plain text format (one ROM path per line)
- ROM validation (only includes existing files)
- PAK detection (identifies .pak files)
- Empty line handling
- Integration tests (multi-platform collections)
- Error handling (file not found, all ROMs missing)

**Coverage:** Complete coverage of collection .txt parsing logic.

**Note:** Extracted from `launcher.c`'s getCollection(), uses file system mocking.

### workspace/all/common/recent_file.c - ✅ 18 tests (13 read + 5 write)
**Files:**
- `tests/unit/all/common/test_recent_file.c` (13 tests) - Recent_parse()
- `tests/unit/all/common/test_recent_writer.c` (5 tests) - Recent_save()

**Read operations (uses file mocking):**
- Recent_parse() - Tab-delimited format parsing
- ROM validation (only includes existing files)
- Order preservation (newest first)
- Format handling (Windows newlines, special characters)
- Error handling (file not found, empty files)

**Write operations (uses real temp files):**
- Recent_save() - Writes entries to recent.txt
- Single/multiple entries with/without aliases
- Empty array handling
- File creation error handling

**Coverage:** Complete coverage of recent.txt read/write operations.

**Note:** Extracted from `launcher.c` loadRecents()/saveRecents(). Uses hybrid approach: file mocking for reads, real temp files for writes.

### workspace/all/common/binary_file_utils.c - ✅ 12 tests
**File:** `tests/unit/all/common/test_binary_file_utils.c`

- BinaryFile_read() - Binary file reading with fread()
- BinaryFile_write() - Binary file writing with fwrite()
- Small buffers (5 bytes)
- Large buffers (1KB)
- SRAM-like data (32KB, like Game Boy saves)
- RTC-like data (8 bytes, like Game Boy RTC)
- Error handling (null buffers, zero size, invalid paths)
- Partial reads
- File overwriting

**Coverage:** Complete coverage of binary file I/O patterns used in player.c.

**Note:** Extracted from `player.c` SRAM_read()/SRAM_write() patterns. Uses real temp files with mkstemp().

### workspace/all/common/effect_system.c - ✅ 30 tests
**File:** `tests/unit/all/common/test_effect_system.c`

- EFFECT_init() - State initialization
- EFFECT_setType/setScale/setColor() - Pending state setters
- EFFECT_applyPending() - Apply pending changes
- EFFECT_needsUpdate() - Change detection
- EFFECT_markLive() - Mark as rendered
- EFFECT_getOpacity() - Global opacity (constant 128)
- EFFECT_usesGeneration() - Procedural generation check
- Full workflow integration tests

**Coverage:** Complete coverage of visual effect state management system.

**Note:** Extracted from platform-specific files. Pure state management with no SDL dependencies.

### workspace/all/common/effect_generate.c - ✅ 24 tests
**File:** `tests/unit/all/common/test_effect_generate.c`

- EFFECT_generateLine() - Horizontal scanline pattern generation
- EFFECT_generateGrille() - Aperture grille pattern with RGB phosphor tints
- EFFECT_generateGrid() - LCD pixel grid pattern
- EFFECT_generateGridWithColor() - Grid with color tinting (for Game Boy DMG)
- EFFECT_generateSlot() - Staggered slot mask pattern
- Edge cases: null buffers, invalid dimensions, scale 1, large scale

**Coverage:** Complete coverage of procedural effect pattern generation.

**Note:** Pure C pattern generation with no SDL dependencies. All patterns write directly to ARGB8888 buffers.

### workspace/all/common/player_utils.c - ✅ 41 tests
**File:** `tests/unit/all/common/test_player_utils.c`

- Player_getCoreName() - Extract core name from .so filename
- Player_getOptionValueIndex() - Search option value arrays
- Player_findNearestFrequency() - CPU frequency matching algorithm
- Player_replaceString() - In-place string replacement
- Player_escapeSingleQuotes() - Shell quote escaping

**Coverage:** Complete coverage of pure utility functions extracted from player.c.

**Note:** Zero external dependencies. Includes shell safety utilities and CPU scaling helpers.

### workspace/all/common/player_config.c - ✅ 19 tests
**File:** `tests/unit/all/common/test_player_config.c`

- Player_getConfigPath() - Config file path generation with device tags
- Player_getOptionDisplayName() - Option key to display name mapping
- Game-specific vs default configs
- Device-specific config paths
- Edge cases (empty strings, special characters, long paths)

**Coverage:** Complete coverage of config path utilities.

**Note:** Extracted from `player.c` Config_getPath() and getOptionNameFromKey().

### workspace/all/common/player_options.c - ✅ 36 tests
**File:** `tests/unit/all/common/test_player_options.c`

- Player_findOption() - Search option list by key
- Player_getOptionValue() - Get current value string
- Player_setOptionValue() - Set value by string matching
- Player_setOptionRawValue() - Set value by index
- Change tracking
- Bounds checking
- Edge cases (empty lists, NULL handling, invalid values)

**Coverage:** Complete coverage of option list management operations.

**Note:** Extracted from `player.c` OptionList_getOption/getOptionValue/setOptionValue functions.

### workspace/all/common/platform_variant.c - ✅ 14 tests
**File:** `tests/unit/all/common/test_platform_variant.c`

- PLAT_getDeviceName() - Device name formatting
- PlatformVariant structure field access
- VARIANT_IS() macro - Variant checking
- HAS_FEATURE() macro - Feature flag checking
- DeviceInfo structure
- Hardware feature flags (NEON, LID, RUMBLE, etc.)

**Coverage:** Complete coverage of platform detection system.

**Note:** Tests the unified platform variant system that supports multi-device platforms.

## Integration Tests

**Location:** `tests/integration/`

Integration tests verify that multiple extracted modules work together correctly with real file I/O. Unlike unit tests that test individual functions in isolation, integration tests exercise realistic workflows that span multiple components.

### Approach: Component Integration

Integration tests use a **component integration** approach:
- Test extracted modules working together (M3U parser + Map parser + Recent file, etc.)
- Use real temp directories and files (created with mkdtemp/mkstemp)
- No mocking - tests real file I/O and data flow between components
- Easy to maintain - follows same pattern as existing 342 unit tests

**Why not test the full launcher.c/player.c applications?**
The main launcher and frontend code is tightly coupled to SDL rendering and event loops. Testing these would require complex subprocess management or invasive code changes. Instead, we test the extracted business logic modules working together - this provides excellent coverage with minimal complexity.

### Available Integration Tests

**File:** `tests/integration/test_workflows.c` - **22 comprehensive tests**

**Multi-Disc Workflows (5 tests):**
1. `test_multi_disc_game_complete_workflow` - M3U + Map + Recent end-to-end
2. `test_multi_disc_detection` - hasM3u + hasCue together
3. `test_m3u_first_vs_all_consistency` - getFirstDisc/getAllDiscs match
4. `test_m3u_with_multiple_cues` - M3U + individual CUE files
5. `test_nested_directories` - Deep directory structures

**Collection Workflows (4 tests):**
6. `test_collection_with_aliases` - Multi-system with map.txt
7. `test_collection_with_m3u_games` - Collection of multi-disc games
8. `test_multi_system_collection_workflow` - GB + NES + PS1 mixed features
9. `test_empty_directory_collection` - Empty directory handling

**Recent Games Workflows (2 tests):**
10. `test_recent_games_roundtrip` - Save/load/modify persistence
11. `test_recent_with_save_states` - Recent + save state integration

**Player Save File Workflows (4 tests):**
12. `test_player_save_state_workflow` - Save state path + binary I/O
13. `test_player_sram_rtc_workflow` - SRAM + RTC file handling
14. `test_all_save_slots` - All 10 slots (0-9)
15. `test_auto_resume_slot_9` - Auto-resume workflow

**Config File Workflows (2 tests):**
16. `test_player_config_file_integration` - Game vs global configs
17. `test_config_device_tags` - Device-specific config isolation

**Advanced Integration (5 tests):**
18. `test_file_detection_integration` - All file detection utilities together
19. `test_hidden_roms_in_map` - Hidden ROM handling (alias starts with '.')
20. `test_rom_with_all_features` - M3U + CUE + alias + save + recent
21. `test_multi_platform_save_isolation` - Cross-platform userdata
22. `test_error_handling_integration` - Missing files, empty collections

### Running Integration Tests

Integration tests run automatically with `make test` (Docker-based):

```bash
make test                          # Run all tests (unit + integration)
make -f Makefile.qa test-native    # Run natively (advanced)
```

Integration tests are included in the standard test suite and run in Docker like all other tests.

### Adding New Integration Tests

Follow the same pattern as existing integration tests:

```c
// 1. Include required modules
#include "../../workspace/all/launcher/launcher_m3u.h"
#include "../../workspace/all/launcher/launcher_map.h"
#include "unity.h"  // Via -I tests/vendor/unity
#include "integration_support.h"

// 2. Use setUp/tearDown for temp directory management
static char test_dir[256];

void setUp(void) {
    strcpy(test_dir, "/tmp/launcher_integration_XXXXXX");
    create_test_launcher_structure(test_dir);
}

void tearDown(void) {
    rmdir_recursive(test_dir);
}

// 3. Write integration test
void test_my_workflow(void) {
    // Setup: Create real files
    char path[512];
    snprintf(path, sizeof(path), "%s/Roms/GB/game.gb", test_dir);
    create_test_rom(path);

    // Test: Call multiple modules
    // Assert: Verify integration works

    // Cleanup: Handled by tearDown()
}
```

### Integration Test Support Utilities

**File:** `tests/integration/integration_support.h/c`

Helper functions for creating test data structures:

- `create_test_launcher_structure()` - Creates temp LessUI directory structure
- `create_test_rom()` - Creates placeholder ROM file
- `create_test_m3u()` - Creates M3U file with disc entries
- `create_test_map()` - Creates map.txt with ROM aliases
- `create_test_collection()` - Creates collection .txt file
- `rmdir_recursive()` - Cleans up temp directories

These utilities make it easy to set up realistic test scenarios.

### Todo
- [ ] Additional api.c GFX rendering functions (mostly SDL pixel operations)
- [x] Integration tests for LessUI/Player workflows (22 tests implemented, all passing)

## Continuous Integration

Tests run automatically on:
- Pre-commit hooks (optional)
- Pull request validation
- Release builds

Configure CI to run:
```bash
make lint   # Static analysis
make test   # All tests in Docker
```

CI systems should have Docker available. The test environment will automatically:
- Pull/build the Ubuntu 24.04 test image
- Compile and run all tests
- Report any failures

## Debugging Test Failures

### Debug in Docker Container
```bash
# Enter the test container
make -f Makefile.qa docker-shell

# Inside container, build and run tests
make -f Makefile.qa clean-tests test-native

# Build with debug symbols
gcc -g -o tests/utils_test_debug tests/unit/all/common/test_utils.c \
    workspace/all/common/utils.c \
    tests/vendor/unity/unity.c \
    -I tests/support -I tests/vendor/unity -I workspace/all/common \
    -std=c99

# Run with gdb
gdb tests/utils_test_debug
(gdb) run
(gdb) bt  # backtrace when crash occurs
```

### Debug Natively (macOS/Linux)
```bash
# Build with debug symbols
gcc -g -o tests/utils_test_debug tests/unit/all/common/test_utils.c \
    workspace/all/common/utils.c \
    tests/vendor/unity/unity.c \
    -I tests/support -I tests/vendor/unity -I workspace/all/common \
    -std=c99

# macOS
lldb tests/utils_test_debug
(lldb) run
(lldb) bt

# Linux
gdb tests/utils_test_debug
(gdb) run
(gdb) bt
```

### Verbose Output
```bash
# Run individual test with verbose mode
./tests/utils_test -v
./tests/string_utils_test -v
```

### Run Single Test
```bash
# Filter by test name
./tests/utils_test -n test_getMicroseconds_non_zero
./tests/string_utils_test -n test_prefixMatch_exact
```

## References

- **Unity Framework**: https://github.com/ThrowTheSwitch/Unity
- **Test Organization Best Practices**: See this README's structure section
- **C Testing Tutorial**: vendor/unity/README.md

## Questions?

If you're adding tests and need help:
1. Look at existing tests in `unit/all/common/test_utils.c`
2. Check Unity documentation in `vendor/unity/`
3. Ask in discussions or open an issue
