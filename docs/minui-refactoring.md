# MinUI Refactoring: Code Organization & Maintainability

**Goal:** Transform minui.c into focused, single-responsibility modules following the same patterns as minarch refactoring.

**Why:** Smaller, focused modules are easier to test, understand, and modify. By extracting cohesive functionality, we enable unit testing of business logic without SDL dependencies.

**Validation:** Unit tests confirm that extracted modules work correctly in isolation.

---

## Progress Summary

| Metric | Before | Current | Target |
|--------|--------|---------|--------|
| **minui.c lines** | ~3,500+ | ~2,450 | ~1,500 |
| **Extracted modules** | 0 | 15 | 15-18 ✅ |
| **Lines in modules** | 0 | ~2,800 | ~2,500 ✅ |
| **Unit tests** | 0 | 448 | 300+ ✅ |

*Note: minui.c decreased after thumbnail + type extraction.*
*Test target exceeded! 448 minui-specific tests across 18 test files.*
*State is now fully typed - MinUIContext uses Directory** and Array of Recent* instead of void*.*

---

## Module Architecture

### Core Principle: Single Responsibility

Each extracted module owns one cohesive concern:

```
minui.c (orchestration, main loop, SDL integration)
    │
    ├── Data Types
    │   ├── minui_entry.c         ─ Entry type, arrays, alphabetical indexing
    │   └── minui_state.c         ─ Path decomposition, state persistence
    │
    ├── File Operations
    │   ├── minui_file_utils.c    ─ Emulator detection, CUE/M3U/thumbnail checking
    │   ├── minui_m3u.c           ─ M3U playlist parsing
    │   ├── minui_map.c           ─ Display name aliasing via map.txt
    │   ├── collection_parser.c   ─ Custom ROM list parsing
    │   └── recent_file.c         ─ Recent games read/write
    │
    ├── String Processing
    │   ├── minui_str_compare.c   ─ Natural sorting (articles, numbers)
    │   └── minui_utils.c         ─ Console directory detection, misc
    │
    ├── Directory Building
    │   ├── directory_index.c     ─ Alias application, hidden filtering, duplicates
    │   └── minui_directory.c     ─ Console detection, entry types, collation, scanning
    │
    ├── Navigation
    │   ├── minui_context.c       ─ Centralized state management (context pattern)
    │   └── minui_navigation.c    ─ Navigation logic, entry dispatch, auto-launch
    │
    ├── UI State
    │   └── minui_thumbnail.c     ─ Thumbnail cache, fade animation, preload hints
    │
    └── Launch
        └── minui_launcher.c      ─ ROM/PAK command construction, quoting
```

---

## Completed Extractions

### Data Types Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minui_entry.c` | ~204 | Entry type management, IntArray, alphabetical indexing | 25 |
| `minui_state.c` | ~269 | Path decomposition, collation, resume state | 27 |

**Key decisions:**
- Entry type is foundational - used throughout UI system
- State persistence separated from file I/O
- IntArray provides efficient index storage

### File Operations Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minui_file_utils.c` | ~169 | Emulator detection, file type checking | 18 |
| `minui_m3u.c` | ~165 | M3U playlist parsing for multi-disc games | 20 |
| `minui_map.c` | ~86 | Display name aliasing from map.txt | 22 |
| `collection_parser.c` | ~92 | Custom ROM list parsing | 11 |
| `recent_file.c` | ~190 | Recent games file I/O + Recent struct operations | 31 |

**Key decisions:**
- File checking is pure (testable without real files)
- Parsing logic separated from file I/O where possible
- Map aliasing uses simple key=value format

### String Processing Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minui_str_compare.c` | ~119 | Natural string sorting (articles, numeric) | 28 |
| `minui_utils.c` | ~51 | Console directory detection, misc utilities | 17 |

**Key decisions:**
- Sorting handles "The", "A", "An" prefixes
- Numeric sorting: "Game 2" before "Game 10"
- Utilities are pure functions

### Directory Building Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `directory_index.c` | ~206 | Alias application, hidden filtering, duplicates, alpha index | 38 |
| `minui_directory.c` | ~360 | Console detection, entry types, collation, scanning, Directory type | 32 |

**Key decisions:**
- Index building is deterministic and testable
- Hidden entries (starting with `.`) filtered
- Duplicate detection for multi-file games
- Console directory detection uses pure path comparisons
- Entry type determination (ROM, PAK, DIR) extracted for testability
- Collation prefix extraction for multi-region systems
- `Directory` struct now in minui_directory.h (properly typed state)
- `Directory_free` and array helpers are pure functions in minui_directory.c
- `Directory_new` stays in minui.c (depends on global state)

### UI State Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minui_thumbnail.c` | ~190 | Thumbnail cache, fade animation, preload hints | 72 |

**Key decisions:**
- Cache uses opaque `void*` data pointer for SDL independence
- FIFO eviction policy (oldest entry removed when full)
- Smoothstep easing for smooth fade-in animation
- SDL surface allocation/freeing handled by minui.c wrappers

### Launch Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minui_launcher.c` | ~118 | ROM/PAK command construction, shell quoting | 32 |

**Key decisions:**
- Command strings properly escaped for shell
- Path construction is pure (testable)
- Separates command building from execution

---

## What Remains in minui.c

### Orchestration (Should Stay)
- `main()` - Entry point, argument parsing, main loop
- Event loop - Input handling, state machine
- SDL initialization and cleanup

### Thumbnail System (~200 lines) - Partially Extracted
**Extracted to `minui_thumbnail.c`:**
- Cache data structure and operations (find, add, evict, clear)
- Fade animation state and smoothstep calculation
- Preload hint index calculation

**Still in minui.c (SDL/threading dependent):**
- `thumbnail_thread()` - Async background loading
- `ThumbLoader_*()` - Thread management and synchronization
- SDL wrapper functions for surface allocation/freeing

### Directory Structures (~150 lines) - ✅ Extracted
**Extracted to module headers:**
- `Directory` struct → `minui_directory.h`
- `Recent` struct → `recent_file.h`
- Pure functions → `minui_directory.c` and `recent_file.c`

**Still in minui.c (global dependent):**
- `Directory_new()` - Uses globals (simple_mode, recents, get* functions)
- `Directory_index()` - Uses globals (COLLECTIONS_PATH, map loading)

### Directory Building (~400 lines) - Partially Extracted
**Extracted to `minui_directory.c`:**
- `isConsoleDir()` - Console directory detection
- `hasRoms()` - Check if directory has ROMs (wrapper)
- `hasCollections()` - Check if collections exist (wrapper)
- Entry type determination in `addEntries()`

**Still in minui.c (global state dependent):**
- `hasRecents()` - Modifies global recents array
- `getRoot()` - Uses global simple_mode
- `getRecents()` - Uses global recents array
- `getCollection()` - Load custom ROM collection
- `getDiscs()` - Get multi-disc game files
- `getEntries()` - Main entry point for directory loading

### Navigation Logic (~200 lines) - Candidate for Extraction
- `queueNext()` - Queue command for execution
- `readyResumePath()` / `readyResume()` / `autoResume()` - Auto-resume
- `openPak()` - Launch application packages
- `openRom()` - Launch ROM with emulator
- `openDirectory()` - Open folder for browsing
- `closeDirectory()` - Navigate back
- `Entry_open()` - Dispatch to appropriate handler
- `saveLast()` / `loadLast()` - State persistence

### Menu/UI System (~100 lines) - Hard to Extract
- `Menu_init()` - Initialize UI resources
- `Menu_quit()` - Cleanup
- Text cache management
- List rendering

### Main Event Loop (~700 lines) - Hard to Extract
- Input handling
- Thumbnail rendering
- Text truncation and caching
- Overlay rendering (brightness, volume)
- HDMI detection

---

## Remaining Extraction Candidates

### Completed (High Priority)

| Function Group | Lines | Status | Notes |
|----------------|-------|--------|-------|
| Directory building | ~400 | ✅ Done | Core helpers extracted (minui_directory.c) |
| Navigation dispatch | ~200 | ✅ Done | Context pattern (minui_navigation.c) |
| Thumbnail cache | ~150 | ✅ Done | Cache + fade logic (minui_thumbnail.c) |

### Medium Priority

| Function Group | Lines | Extractable? | Notes |
|----------------|-------|--------------|-------|
| Directory struct | ~150 | ✅ Yes | Data structures, no SDL dependency |
| Recent array ops | ~50 | ✅ Yes | Already simple, could consolidate |

### Low Priority (SDL-Dependent)

| Function Group | Lines | Extractable? | Notes |
|----------------|-------|--------------|-------|
| Text cache | ~100 | ⚠️ Partial | Cache logic maybe, rendering no |
| Main event loop | ~700 | ❌ No | Tightly coupled to SDL/input |
| Menu init/quit | ~100 | ❌ No | SDL resource management |

---

## Design Patterns Used

### 1. Pure Functions for Business Logic

```c
// Testable: No side effects, deterministic
int MinUIStrCompare_compare(const char* a, const char* b);

// Wrapper in minui.c handles actual sorting
static int Entry_sort(const void* a, const void* b) {
    return MinUIStrCompare_compare(
        ((Entry*)a)->name,
        ((Entry*)b)->name
    );
}
```

### 2. Filesystem Abstraction for Testing

```c
// In tests, use --wrap to mock file operations
int __wrap_exists(const char* path) {
    // Return test fixture data
}

// Production code uses real filesystem
int exists(const char* path);
```

### 3. Result Structs for Complex Returns

```c
typedef struct {
    char path[MAX_PATH];
    char filename[MAX_PATH];
    int disc_number;
} M3UEntry;

int MinUIM3U_parse(const char* content, M3UEntry* entries, int max_entries);
```

---

## Next Steps

### Phase 1: Directory Building Extraction - ✅ COMPLETE

**Completed:** Created `minui_directory.c` with ~335 lines, 32 tests

**Extracted functions:**
- `MinUIDir_isConsoleDir()` - Console directory detection
- `MinUIDir_determineEntryType()` - Entry type determination
- `MinUIDir_hasRoms()` - ROM system availability check (wrapper)
- `MinUIDir_buildCollationPrefix()` - Collation prefix extraction
- `MinUIDir_matchesCollation()` - Collation matching
- `MinUIDirScanResult` - Directory scanning utilities
- `MinUIDir_scan()` - Single directory scanning
- `MinUIDir_scanCollated()` - Multi-directory collated scanning

**Integrated into minui.c:**
- `getEntries()` now uses `MinUIDir_buildCollationPrefix()` and `MinUIDir_matchesCollation()`
- `isConsoleDir()` is a thin wrapper around `MinUIDir_isConsoleDir()`
- `addEntries()` uses `MinUIDir_determineEntryType()`

**Remaining (global state dependent):**
- `getRoot()`, `getRecents()`, `getCollection()` - Use global arrays
- Full context pattern would be needed for complete extraction

### Phase 2: Navigation Extraction - ✅ COMPLETE

**Completed:** Created `minui_context.c` (~50 lines) and `minui_navigation.c` (~200 lines), 30 tests

**New modules:**
- `minui_context.h/c` - Context pattern for testability (same as MinArch)
  - `MinUIContext` structure with pointers to globals
  - `MinUICallbacks` for service function pointers
  - Enables dependency injection for testing
- `minui_navigation.h/c` - Pure navigation logic
  - `MinUINav_determineAction()` - Determines action for entry type
  - `MinUINav_shouldAutoLaunch()` - Checks for cue/m3u auto-launch
  - `MinUINav_buildPakCommand()` - Pak launch command building
  - `MinUINav_buildRomCommand()` - ROM launch command building

**Context wiring complete:**
- `MinUIContext_setup()` added to minui.c
- All globals wired: `top`, `stack`, `recents`, `quit`, `can_resume`, `should_resume`, etc.
- Callbacks initialized: `addRecent`, `saveRecents`, `queueNext`, `saveLast`, etc.
- `MinUIRestoreState` struct replaces individual restore_* globals (with macros for compatibility)

**Navigation functions migrated to context pattern:**
- `openPak_ctx()` / `openPak()` - Pak application launching
- `openRom_ctx()` / `openRom()` - ROM launching with multi-disc/resume support
- `openDirectory_ctx()` / `openDirectory()` - Directory browsing with auto-launch
- `closeDirectory_ctx()` / `closeDirectory()` - Directory stack navigation
- `Entry_open_ctx()` / `Entry_open()` - Entry dispatch to appropriate handler

All functions now have `_ctx` versions that take `MinUIContext*` as first parameter.
Legacy wrappers call the `_ctx` versions with `MinUIContext_get()` for backward compatibility.

**Tests added for context-aware functions:**
- Mock callback infrastructure for testing navigation in isolation
- `MinUINav_openPak` tests: callback invocation, command building, null safety
- `MinUINav_openDirectory` tests: callback invocation, null safety
- `MinUINav_closeDirectory` tests: null safety, empty stack handling
- `MinUINav_openRom` tests: null safety

### Phase 3: Thumbnail System - ✅ COMPLETE

**Completed:** Created `minui_thumbnail.c` (~175 lines), 72 tests

**New module:**
- `minui_thumbnail.h/c` - Thumbnail cache and fade animation
  - `ThumbCache` - FIFO cache with fixed capacity (3 slots)
  - `ThumbCacheSlot` - Opaque data pointer for SDL_Surface abstraction
  - `ThumbFadeState` - Fade animation state tracking
  - `ThumbPreload_getHintIndex()` - Scroll direction preload calculation

**Extracted functions:**
- `ThumbCache_init/find/add/evict/clear()` - Pure cache operations
- `ThumbCache_isFull/getEvictSlot/getData/getSlot()` - Cache queries
- `ThumbFade_init/start/reset/update/isActive()` - Fade state management
- `ThumbFade_calculateAlpha()` - Smoothstep easing math

**Integrated into minui.c:**
- `thumb_cache_push()` - SDL wrapper that handles eviction and surface freeing
- `thumb_cache_clear()` - SDL wrapper that frees all cached surfaces
- `ThumbCache` replaces old `CacheItem[]` array
- `ThumbFadeState` replaces `thumb_alpha` and `thumb_fade_start_ms` variables

**Design decisions:**
- Cache uses `void*` data pointer for SDL independence and testability
- SDL_Surface allocation/freeing stays in minui.c wrappers
- Async thumbnail loader thread management unchanged (stays in minui.c)
- Fade animation uses smoothstep easing: f(t) = t² × (3 - 2t)

### Phase 4: Type Safety & State Organization - ✅ COMPLETE

**Completed:** Extracted Directory and Recent types to proper headers, fully typed MinUIContext

**Type extractions:**
- `Directory` struct moved to `minui_directory.h`
  - `Directory_free()` moved to minui_directory.c (pure cleanup)
  - `DirectoryArray_pop/free()` moved to minui_directory.c (pure operations)
  - `Directory_new/index()` stay in minui.c (depend on globals)
- `Recent` struct moved to `recent_file.h`
  - `Recent_new()` moved to recent_file.c (now takes hasEmu callback for testability)
  - `Recent_free()` moved to recent_file.c
  - `RecentArray_indexOf/free()` moved to recent_file.c (pure array operations)
  - `Recent_new_local()` wrapper in minui.c for convenience

**MinUIContext improvements:**
- Changed from `void** top` to `Directory** top` (properly typed)
- `Array** recents` now properly documented as Array of Recent*
- `ctx_getTop()` now returns `Directory*` instead of `void*`
- Added `ctx_getRecents()` accessor
- Removed all `void*` casts from context initialization

**Benefits:**
- State structure is now self-documenting (types show intent)
- Can create mock contexts with proper types for testing
- Compiler enforces type safety instead of runtime casts
- Matches MinArch pattern (fully typed context)

**Tests added:**
- 13 new tests for Recent runtime operations (Recent_new, array ops)
- Tests use mock hasEmu callback to verify emulator availability checking

---

## File Organization

```
workspace/all/minui/             # Launcher
├── minui.c                      # Main file (~2,500 lines)
├── minui_entry.h/c              # Entry type, IntArray
├── minui_state.h/c              # State persistence
├── minui_file_utils.h/c         # File checking
├── minui_m3u.h/c                # M3U parsing
├── minui_map.h/c                # Display name aliasing
├── minui_str_compare.h/c        # Natural sorting
├── minui_launcher.h/c           # Command construction
├── minui_utils.h/c              # Misc utilities
├── minui_directory.h/c          # Console detection, entry types, collation
├── minui_thumbnail.h/c          # Thumbnail cache, fade animation
├── directory_index.h/c          # Index building
├── collection_parser.h/c        # Collection parsing
├── recent_file.h/c              # Recent games
├── minui_context.h/c            # Context pattern (state management)
└── minui_navigation.h/c         # Navigation logic

tests/unit/all/common/           # Unit tests
├── test_minui_entry.c
├── test_minui_state.c
├── test_minui_file_utils.c
├── test_minui_launcher.c
├── test_minui_utils.c
├── test_minui_directory.c       # 32 tests
├── test_minui_navigation.c      # 30 tests
├── test_minui_thumbnail.c       # 72 tests
├── test_m3u_parser.c
├── test_map_parser.c
├── test_str_compare.c
├── test_directory_index.c
├── test_collection_parser.c
├── test_recent_parser.c         # Recent file I/O parsing
├── test_recent_writer.c         # Recent file I/O writing
└── test_recent_runtime.c        # 13 tests - Recent struct operations
```

---

## Comparison with MinArch Refactoring

| Aspect | MinArch | MinUI |
|--------|---------|-------|
| Original size | ~7,000 lines | ~3,500 lines |
| Current size | 5,098 lines | ~2,544 lines |
| Reduction | 27% | 27% |
| Extracted modules | 18 | 14 |
| Unit tests | 1,106 | 363 |
| Complexity | Higher (libretro, video, audio) | Lower (file browser, UI) |

**Key Differences:**
- MinArch has more complex domain (emulator frontend)
- MinUI has simpler I/O patterns (file browsing)
- MinArch needed more extensive callback abstractions
- MinUI can use simpler filesystem mocking

---

## Validation

All extracted modules are validated by unit tests:

- **363 tests** passing (minui-specific modules)
- **1,168 total tests** in test suite
- Tests run in Docker for consistency

The test count reflects comprehensive coverage of extracted functionality.

---

## Lessons Learned from MinArch

### Applied to MinUI ✅

1. **Context pattern** - `MinUIContext` implemented for dependency injection
2. **Service callbacks** - `MinUICallbacks` abstracts operations for mocking
3. **Pure functions** - Business logic extracted from I/O throughout
4. **Result structs** - Used for complex return values (e.g., `MinUINavAction`)

### What Worked Well

1. **Incremental extraction** - One module at a time
2. **Test-first** - Write tests for extracted code
3. **Preserve behavior** - No functional changes during extraction
4. **Clear boundaries** - Each module owns one concern

### What to Avoid

1. **Over-extraction** - Don't extract thin wrappers
2. **Breaking SDL coupling** - Some code must stay coupled
3. **Premature abstraction** - Extract when testing demands it
