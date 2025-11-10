# LessUI Test Suite

This directory contains the test suite for LessUI, organized to mirror the source code structure.

**Current Status: 364 tests, all passing** ✅

## Quick Start

```bash
make test   # Run all 364 tests in Docker (recommended)
```

Tests run in a Debian Buster ARM64 container that matches the platform toolchains exactly. This ensures consistency across development environments and catches platform-specific issues.

## Test Environment

**Docker Container Specifications:**
- Base: Debian Buster (10) ARM64
- Compiler: GCC 8.3.0-6 (matches platform toolchains)
- C Library: GLIBC 2.28
- Architecture: aarch64 (falls back to C implementations for 32-bit ARM assembly)
- Dockerfile: `tests/Dockerfile`

**Why Docker?**
- Eliminates macOS ARM architecture differences
- Matches the exact GCC/libc versions used by platform toolchains
- Consistent test results across all development machines
- No need to install native build tools on macOS

## Directory Structure

```
tests/
├── unit/                           # Unit tests (mirror workspace/ structure)
│   └── all/
│       └── common/
│           ├── test_utils.c              # Utils (string, file, name, date, math) - 100 tests
│           ├── test_api_pad.c            # Input state machine - 21 tests
│           ├── test_collections.c        # Array/Hash data structures - 30 tests
│           ├── test_gfx_text.c           # Text truncation/wrapping - 32 tests
│           ├── test_audio_resampler.c    # Audio resampling - 18 tests
│           ├── test_minarch_paths.c      # Save file paths - 16 tests
│           ├── test_minui_utils.c        # Launcher helpers - 17 tests
│           ├── test_m3u_parser.c         # M3U parsing - 20 tests
│           ├── test_minui_file_utils.c   # File/dir checking - 25 tests
│           ├── test_map_parser.c         # map.txt aliasing - 22 tests
│           ├── test_collection_parser.c  # Collection lists - 11 tests
│           ├── test_recent_file.c        # Recent games parsing - 13 tests
│           ├── test_recent_writer.c      # Recent games writing - 5 tests
│           ├── test_directory_utils.c    # Directory ops (→ minui_file_utils) - 7 tests
│           └── test_binary_file_utils.c  # Binary file I/O - 12 tests
├── integration/                    # Integration tests (end-to-end tests)
├── fixtures/                       # Test data, sample ROMs, configs
├── support/                        # Test infrastructure
│   ├── unity/                      # Unity test framework
│   ├── fff/                        # Fake Function Framework (SDL mocking)
│   │   └── fff.h                   # Header-only mocking library
│   ├── platform.h                  # Platform stubs for testing
│   ├── sdl_stubs.h                 # Minimal SDL type definitions
│   ├── sdl_fakes.h/c               # SDL function mocks (fff-based)
│   ├── platform_mocks.h/c          # Platform function mocks
│   └── fs_mocks.h/c                # File system mocks (--wrap-based)
├── Dockerfile                      # Test environment (Debian Buster ARM64)
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
workspace/all/minui/minui.c               →  tests/unit/all/minui/test_minui.c
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

Tests run in a Debian Buster ARM64 container that matches the platform toolchains exactly (GCC 8.3.0, GLIBC 2.28). This eliminates macOS-specific build issues and ensures consistency with the actual build environment.

```bash
# Run all tests (uses Docker automatically)
make test

# Enter Docker container for debugging
make -f makefile.qa docker-shell

# Rebuild Docker image
make -f makefile.qa docker-build
```

### Native Testing (Advanced)

Run tests directly on your host system (not recommended on macOS due to architecture differences):

```bash
# Run all tests natively
make -f makefile.qa test-native

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
make -f makefile.qa clean-tests
make test
```

## Writing New Tests

### 1. Mirror the Source Structure

If adding tests for `workspace/all/minui/minui.c`:

```bash
# Create directory
mkdir -p tests/unit/all/minui

# Create test file
touch tests/unit/all/minui/test_minui.c
```

### 2. Use Unity Framework

```c
#include "../../../../workspace/all/minui/minui.h"
#include "../../../support/unity/unity.h"

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

### 3. Update makefile.qa

Add your test to the build:

```makefile
tests/unit_tests: tests/unit/all/minui/test_minui.c ...
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

See `support/unity/unity.h` for full list.

## SDL Function Mocking

LessUI uses SDL extensively for graphics, input, and audio. Testing SDL-dependent code requires **mocking** SDL functions. We use the **Fake Function Framework (fff)** for this.

### Infrastructure Overview

```
tests/support/
├── fff/
│   └── fff.h              # Fake Function Framework (MIT licensed)
├── sdl_stubs.h            # Minimal SDL type definitions
├── sdl_fakes.h            # fff-based SDL function fakes (declarations)
├── sdl_fakes.c            # fff-based SDL function fakes (definitions)
├── platform_mocks.h       # Mock PLAT_* interface
└── platform_mocks.c       # Mock PLAT_* implementations
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
#include "../../../support/unity/unity.h"
#include "../../../support/fff/fff.h"
#include "../../../support/sdl_stubs.h"
#include "../../../support/sdl_fakes.h"

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
#include "../../../support/platform_mocks.h"

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

**Extract to separate module** (like `pad.c`, `collections.c`) when:
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
- `tests/support/fff/fff.h` (inline documentation)
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
- ✅ Docker (Debian Buster with GCC) - **This is how we test**
- ❌ macOS (uses ld64, doesn't support --wrap)

### Using File Mocking

#### Example: Testing M3U Parser

```c
#include "../../../support/unity/unity.h"
#include "../../../support/fs_mocks.h"
#include "../../../../workspace/all/common/m3u_parser.h"

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
- `tests/unit/all/common/test_m3u_parser.c` - M3U playlist parsing (read-only with mocking)
- `tests/unit/all/common/test_minui_file_utils.c` - File existence checking
- `tests/unit/all/common/test_recent_writer.c` - File writing with real temp files

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

**Total: 364 tests across 16 test suites**
- **342 unit tests** - Testing individual modules in isolation
- **22 integration tests** - Testing modules working together with real file I/O

### Extracted Modules (Testable Logic)

These modules were extracted from large files (api.c, minui.c, minarch.c) to enable comprehensive unit testing:

| Module | Lines | Tests | Extracted From | Key Functions |
|--------|-------|-------|----------------|---------------|
| utils.c | 703 | 100 | (original) | String, file, name, date, math utilities |
| pad.c | 183 | 21 | api.c | Button state machine, analog input |
| collections.c | 193 | 30 | minui.c | Array, Hash data structures |
| gfx_text.c | 170 | 32 | api.c | Text truncation, wrapping, sizing |
| audio_resampler.c | 90 | 18 | api.c | Bresenham sample rate conversion |
| minarch_paths.c | 77 | 16 | minarch.c | Save file path generation |
| minui_utils.c | 48 | 17 | minui.c | Index char, console dir detection |
| m3u_parser.c | 132 | 20 | minui.c | M3U playlist parsing (getFirstDisc + getAllDiscs) |
| minui_file_utils.c | 130 | 25 | minui.c | File/dir checking (hasEmu, hasCue, hasM3u, hasNonHiddenFiles) |
| map_parser.c | 64 | 22 | minui.c/minarch.c | ROM display name aliasing (map.txt) |
| collection_parser.c | 70 | 11 | minui.c | Custom ROM list parsing (.txt files) |
| recent_file.c | 95 | 18 | minui.c | Recent games read/write (parse + save) |
| binary_file_utils.c | 42 | 12 | minarch.c | Binary file read/write (fread/fwrite) |
| **Total** | **1,997** | **342** | | |

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

### workspace/all/common/collections.c - ✅ 30 tests
**File:** `tests/unit/all/common/test_collections.c`

- Array lifecycle (Array_new, Array_free)
- Array operations (push, pop, unshift, reverse)
- Capacity growth (doubling when full)
- StringArray operations (indexOf, StringArray_free)
- Hash map operations (Hash_new, Hash_set, Hash_get, Hash_free)
- Integration tests (ROM alias maps, recent games lists)

**Coverage:** Complete coverage of dynamic array and hash map data structures.

**Note:** Extracted from `minui.c` for reusability across all components.

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

### workspace/all/common/minarch_paths.c - ✅ 16 tests
**File:** `tests/unit/all/common/test_minarch_paths.c`

- SRAM save file path generation (.sav files)
- RTC file path generation (.rtc files)
- Save state path generation (.st0-.st9 files)
- Config file path generation (.cfg files with device tags)

**Coverage:** Complete coverage of MinArch save file path logic.

**Note:** Extracted from `minarch.c`, pure sprintf logic.

### workspace/all/common/minui_utils.c - ✅ 17 tests
**File:** `tests/unit/all/common/test_minui_utils.c`

- LessUI_getIndexChar() - Alphabetical indexing for ROM navigation (7 tests)
- LessUI_isConsoleDir() - Console directory classification (8 tests)
- Integration tests (2 tests)

**Coverage:** Complete coverage of LessUI helper utilities.

**Note:** Extracted from `minui.c`, pure string logic.

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

**Note:** Extracted from `minui.c`, uses file system mocking.

### workspace/all/common/minui_file_utils.c - ✅ 25 tests
**Files:**
- `tests/unit/all/common/test_minui_file_utils.c` (18 tests)
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

**Note:** Extracted from `minui.c` hasEmu/hasCue/hasM3u/hasCollections/hasRoms. File tests use mocking, directory tests use real temp directories with mkdtemp().

### workspace/all/common/map_parser.c - ✅ 22 tests
**File:** `tests/unit/all/common/test_map_parser.c`

- Map_getAlias() - ROM display name aliasing
- Tab-delimited format parsing (filename<TAB>display name)
- Hidden ROM detection (alias starts with '.')
- Basic parsing (with and without aliases)
- Integration tests (real-world map.txt files)
- Error handling (file not found, empty files, missing ROMs)

**Coverage:** Complete coverage of map.txt parsing logic.

**Note:** Extracted from `minui.c`'s Directory_index() and `minarch.c`'s getAlias(), uses file system mocking.

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

**Note:** Extracted from `minui.c`'s getCollection(), uses file system mocking.

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

**Note:** Extracted from `minui.c` loadRecents()/saveRecents(). Uses hybrid approach: file mocking for reads, real temp files for writes.

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

**Coverage:** Complete coverage of binary file I/O patterns used in minarch.c.

**Note:** Extracted from `minarch.c` SRAM_read()/SRAM_write() patterns. Uses real temp files with mkstemp().

## Integration Tests

**Location:** `tests/integration/`

Integration tests verify that multiple extracted modules work together correctly with real file I/O. Unlike unit tests that test individual functions in isolation, integration tests exercise realistic workflows that span multiple components.

### Approach: Component Integration

Integration tests use a **component integration** approach:
- Test extracted modules working together (M3U parser + Map parser + Recent file, etc.)
- Use real temp directories and files (created with mkdtemp/mkstemp)
- No mocking - tests real file I/O and data flow between components
- Easy to maintain - follows same pattern as existing 342 unit tests

**Why not test the full minui.c/minarch.c applications?**
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

**MinArch Save File Workflows (4 tests):**
12. `test_minarch_save_state_workflow` - Save state path + binary I/O
13. `test_minarch_sram_rtc_workflow` - SRAM + RTC file handling
14. `test_all_save_slots` - All 10 slots (0-9)
15. `test_auto_resume_slot_9` - Auto-resume workflow

**Config File Workflows (2 tests):**
16. `test_minarch_config_file_integration` - Game vs global configs
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
make -f makefile.qa test-native    # Run natively (advanced)
```

Integration tests are included in the standard test suite and run in Docker like all other tests.

### Adding New Integration Tests

Follow the same pattern as existing integration tests:

```c
// 1. Include required modules
#include "../../workspace/all/common/m3u_parser.h"
#include "../../workspace/all/common/map_parser.h"
#include "../support/unity/unity.h"
#include "integration_support.h"

// 2. Use setUp/tearDown for temp directory management
static char test_dir[256];

void setUp(void) {
    strcpy(test_dir, "/tmp/minui_integration_XXXXXX");
    create_test_minui_structure(test_dir);
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

- `create_test_minui_structure()` - Creates temp LessUI directory structure
- `create_test_rom()` - Creates placeholder ROM file
- `create_test_m3u()` - Creates M3U file with disc entries
- `create_test_map()` - Creates map.txt with ROM aliases
- `create_test_collection()` - Creates collection .txt file
- `rmdir_recursive()` - Cleans up temp directories

These utilities make it easy to set up realistic test scenarios.

### Todo
- [ ] Additional api.c GFX rendering functions (mostly SDL pixel operations)
- [x] Integration tests for LessUI/MinArch workflows (22 tests implemented, all passing)

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
- Pull/build the Debian Buster ARM64 test image
- Compile and run all tests
- Report any failures

## Debugging Test Failures

### Debug in Docker Container
```bash
# Enter the test container
make -f makefile.qa docker-shell

# Inside container, build and run tests
make -f makefile.qa clean-tests test-native

# Build with debug symbols
gcc -g -o tests/utils_test_debug tests/unit/all/common/test_utils.c \
    workspace/all/common/utils/utils.c \
    tests/support/unity/unity.c \
    -I tests/support -I tests/support/unity -I workspace/all/common \
    -std=c99

# Run with gdb (lldb not available in Debian Buster)
gdb tests/utils_test_debug
(gdb) run
(gdb) bt  # backtrace when crash occurs
```

### Debug Natively (macOS/Linux)
```bash
# Build with debug symbols
gcc -g -o tests/utils_test_debug tests/unit/all/common/test_utils.c \
    workspace/all/common/utils/utils.c \
    tests/support/unity/unity.c \
    -I tests/support -I tests/support/unity -I workspace/all/common \
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
- **C Testing Tutorial**: support/unity/README.md

## Questions?

If you're adding tests and need help:
1. Look at existing tests in `unit/all/common/test_utils.c`
2. Check Unity documentation in `support/unity/`
3. Ask in discussions or open an issue
