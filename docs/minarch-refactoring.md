# MinArch Refactoring: Code Organization & Maintainability

**Goal:** Transform monolithic minarch.c (~7,000 lines) into focused, single-responsibility modules

**Why:** Large files are hard to navigate, understand, and modify. By extracting cohesive functionality into dedicated modules, we make the codebase more maintainable, easier to reason about, and simpler to extend.

**Validation:** Unit tests confirm that extracted modules work correctly in isolation.

---

## Progress Summary

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| **minarch.c lines** | ~7,000 | 5,140 | -27% |
| **Extracted modules** | 0 | 17 | +17 modules |
| **Lines in modules** | 0 | ~4,350 | Well-organized |
| **Unit tests** | 491 | 1,106 | +615 tests |

---

## Module Architecture

### Core Principle: Single Responsibility

Each extracted module owns one cohesive concern:

```
minarch.c (orchestration, main loop, SDL integration)
    │
    ├── minarch_context.h/c     ─ State container for dependency injection
    ├── minarch_internal.h      ─ Shared type definitions (Core, Game, Config)
    │
    ├── Configuration
    │   ├── minarch_config.c    ─ Config file parsing utilities
    │   ├── minarch_options.c   ─ Core option list management
    │   └── minarch_paths.c     ─ Path generation (saves, states, BIOS)
    │
    ├── Input
    │   ├── minarch_input.c     ─ Button state, mappings, descriptors
    │   └── minarch_mappings.c  ─ Static button/label data, enums
    │
    ├── Persistence
    │   ├── minarch_memory.c    ─ SRAM/RTC read/write
    │   ├── minarch_state.c     ─ Save state management
    │   └── minarch_zip.c       ─ ZIP extraction
    │
    ├── Video
    │   ├── minarch_scaler.c    ─ Scaling geometry calculations
    │   ├── minarch_rotation.c  ─ Framebuffer rotation buffers
    │   └── minarch_video_convert.c ─ Pixel format conversion
    │
    ├── Core Integration
    │   ├── minarch_core.c      ─ AV info processing, aspect ratio
    │   ├── minarch_env.c       ─ Libretro environment callbacks
    │   ├── minarch_game.c      ─ Game file handling, ZIP parsing
    │   └── minarch_cpu.c       ─ Auto CPU frequency scaling
    │
    └── UI
        ├── minarch_menu.c      ─ In-game menu system
        └── minarch_menu_types.h ─ Menu type definitions
```

---

## Completed Extractions

### Configuration Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_config.c` | ~90 | Parse config values, option display names | 19 |
| `minarch_options.c` | ~200 | Option list search, get/set operations | 45 |
| `minarch_paths.c` | ~110 | Generate paths for saves, states, configs, BIOS | 23 |

**Key decisions:**
- Config parsing separated from file I/O
- Path generation is pure (testable) - file operations stay in minarch.c
- BIOS path selection uses smart fallback (tag dir → root)

### Input Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_input.c` | ~240 | Button state queries, mapping lookups, descriptor processing | 36 |
| `minarch_mappings.c` | ~420 | Static data: button labels, scale/effect enums, defaults | - |

**Key decisions:**
- Pure functions for button collection (no PAD_* dependencies)
- D-pad remapping logic extracted and testable
- Modifier key handling isolated

### Persistence Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_memory.c` | ~120 | SRAM/RTC persistence with mock core | 16 |
| `minarch_state.c` | ~130 | Save state read/write, auto-resume | 16 |
| `minarch_zip.c` | ~150 | ZIP copy/inflate extraction | 13 |

**Key decisions:**
- Core callbacks abstracted for testing
- Path generation delegated to minarch_paths
- Temp file handling isolated

### Video Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_scaler.c` | ~350 | Scaling geometry: aspect, native, cropped modes | 26 |
| `minarch_rotation.c` | ~170 | Rotation buffer management | - |
| `minarch_video_convert.c` | ~350 | Pixel format conversion (NEON + scalar) | - |

**Key decisions:**
- Scaling calculations are pure math (highly testable)
- Rotation handling extracted but tightly coupled to buffers
- NEON code stays in dedicated module (platform-specific)

### Core Integration Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_core.c` | ~150 | Build game info, calculate aspect ratio, process AV info | 23 |
| `minarch_env.c` | ~400 | Handle 30+ libretro environment callbacks | 51 |
| `minarch_game.c` | ~300 | Extension parsing, ZIP headers, M3U detection | 46 |
| `minarch_cpu.c` | ~350 | Auto CPU frequency scaling algorithm | 42 |

**Key decisions:**
- Environment callbacks return structured results (testable)
- Game file utilities are pure string/parsing operations
- CPU algorithm separated from thread management

### UI Layer

| Module | Lines | Responsibility | Tests |
|--------|-------|----------------|-------|
| `minarch_menu.c` | ~854 | Complete menu system: init, loop, save/load, scale | 41 |
| `minarch_menu_types.h` | ~120 | MenuItem, MenuList type definitions | - |
| `minarch_context.c` | ~140 | State container for context-based APIs | - |
| `minarch_context.h` | ~260 | Context struct + service callback types | - |

**Key decisions:**
- Menu functions take `MinArchContext*` for explicit dependencies
- ✅ **All extern declarations eliminated** - Replaced with service callbacks in context
- Context pattern enables unit testing of menu logic
- SDL rendering stays in menu module (can't easily mock)

---

## What Remains in minarch.c

### Orchestration (Should Stay)
- `main()` - Entry point, argument parsing, main loop
- `Menu_loop()` - Top-level game menu orchestration
- Thread management for auto CPU scaling

### SDL Integration (Hard to Extract)
- `Menu_options()` - 415 lines of menu UI rendering
- `video_refresh_callback_main()` - Frame pipeline with debug HUD
- Audio callbacks - Thin wrappers around SND_*

### Platform-Specific (Better as Integration Tests)
- `Core_open()` - dlopen/dlsym symbol resolution
- Config file I/O wrappers
- HDMI monitoring

### Remaining Extraction Candidates

| Function | Lines | Extractable? | Notes |
|----------|-------|--------------|-------|
| `trackFPS()` | ~40 | ✅ Yes | Pure frame timing math |
| `limitFF()` | ~60 | ✅ Yes | Fast-forward throttling |
| `calculateProportionalWidths()` | ~40 | ✅ Yes | Text layout calculation |
| `Config_readOptionsString()` | ~30 | ⚠️ Partial | Parsing is pure, state update isn't |
| `Config_readControlsString()` | ~65 | ⚠️ Partial | Same as above |

---

## Design Patterns Used

### 1. Context Pattern (minarch_context.c)

```c
// Before: Global state everywhere
void Menu_saveState(void) {
    State_write();  // Uses global state_slot, game, core
}

// After: Explicit dependencies
void Menu_saveState_ctx(MinArchContext* ctx) {
    State_write(ctx->state_slot, ctx->game, ctx->core);
}
```

**Benefits:**
- Dependencies visible in function signature
- Enables unit testing with mock context
- No hidden coupling to globals

### 1b. Service Callbacks Pattern (minarch_context.h)

The menu module was initially extracted with extern declarations to functions in minarch.c:

```c
// BEFORE: Extern declarations (bad - bidirectional coupling)
extern void SRAM_write(void);
extern void RTC_write(void);
extern void State_read(void);
extern void State_write(void);
extern void Game_changeDisc(char* path);
extern int Menu_options(MenuList* list);
extern MenuList options_menu;
// ... 10+ more externs
```

This was replaced with a callback interface in the context:

```c
// AFTER: Service callbacks (good - unidirectional dependency)
typedef struct MinArchCallbacks {
    MinArchSRAMWriteFunc sram_write;
    MinArchRTCWriteFunc rtc_write;
    MinArchStateReadFunc state_read;
    MinArchStateWriteFunc state_write;
    MinArchGameChangeDiscFunc game_change_disc;
    MinArchMenuOptionsFunc menu_options;
    struct MenuList* options_menu;
    // ... all dependencies explicit
} MinArchCallbacks;

// Menu module uses callbacks via context
static void Menu_beforeSleep_ctx(MinArchContext* ctx) {
    MinArchCallbacks* cb = ctx->callbacks;
    cb->sram_write();
    cb->rtc_write();
    cb->state_autosave();
    // ...
}
```

**Benefits:**
- Eliminates all extern declarations in minarch_menu.c
- Dependencies are explicit and injectable
- Enables unit testing with mock callbacks
- Maintains unidirectional dependency flow (minarch.c → menu, not bidirectional)
- Menu module has zero compile-time coupling to minarch.c internals

### 2. Pure Functions for Business Logic

```c
// Testable: No side effects, deterministic
uint32_t MinArchInput_collectButtons(
    const MinArchButtonMapping* controls,
    uint32_t pressed_buttons,
    int menu_pressed,
    int gamepad_type,
    const MinArchDpadRemap* dpad_remaps,
    int* out_used_modifier);

// Wrapper in minarch.c handles actual button reading
static void input_poll_callback(void) {
    PAD_poll();
    buttons = MinArchInput_collectButtons(...);
}
```

### 3. Result Structs for Complex Returns

```c
typedef struct {
    int handled;           // Was the command handled?
    int needs_reinit;      // Should audio be reinitialized?
    int updated_variable;  // Was a core variable changed?
} EnvResult;

EnvResult MinArchEnv_handleGeometry(void* data, VideoState* video);
```

---

## Lessons Learned

### Cross-Platform SDL Header Management

When extracting modules, a critical SDL header conflict emerged:

**Problem:** Hardcoded `#include <SDL/SDL.h>` in extracted modules conflicts with SDL2 platforms.

**Solution:** Always use `#include "sdl.h"` (the compatibility layer) in production code. Use forward declarations in headers.

```c
// CORRECT - In .c files
#include "sdl.h"

// CORRECT - In .h files
struct SDL_Surface;  // Forward declaration

// WRONG - Hardcoded version
#include <SDL/SDL.h>
```

### When NOT to Extract

1. **Thin wrappers** - If a function just calls one other function, don't extract
2. **Tightly coupled SDL code** - Rendering code that uses GFX_*, TTF_*, SDL_* throughout
3. **Thread entry points** - Keep thread functions near their management code
4. **dlopen/dlsym** - Platform-specific, better tested via integration

---

## File Organization

```
workspace/all/minarch/           # Emulator frontend
├── minarch.c                    # Main file (5,189 lines)
├── minarch_internal.h           # Shared types (header only)
├── minarch_context.h/c          # State container
├── minarch_config.h/c           # Config parsing
├── minarch_options.h/c          # Option management
├── minarch_paths.h/c            # Path generation
├── minarch_input.h/c            # Input handling
├── minarch_mappings.h/c         # Static data
├── minarch_memory.h/c           # SRAM/RTC
├── minarch_state.h/c            # Save states
├── minarch_zip.h/c              # ZIP extraction
├── minarch_scaler.h/c           # Video scaling
├── minarch_rotation.h/c         # Rotation buffers
├── minarch_video_convert.h/c    # Pixel conversion
├── minarch_core.h/c             # Core AV processing
├── minarch_env.h/c              # Environment callbacks
├── minarch_game.h/c             # Game file handling
├── minarch_cpu.h/c              # CPU scaling
├── minarch_menu.h/c             # Menu system
└── minarch_menu_types.h         # Menu types

tests/unit/all/common/           # Unit tests
├── test_minarch_config.c
├── test_minarch_options.c
├── test_minarch_paths.c
├── test_minarch_input.c
├── test_minarch_memory.c
├── test_minarch_state.c
├── test_minarch_zip.c
├── test_minarch_scaler.c
├── test_minarch_core.c
├── test_minarch_env.c
├── test_minarch_game.c
├── test_minarch_cpu.c
└── test_minarch_menu.c
```

---

## Next Steps

### Potential Further Extractions

1. **Frame timing utilities** - Extract `trackFPS()`, `limitFF()` to `minarch_timing.c`
2. **Text layout helpers** - Extract `calculateProportionalWidths()` to shared utility
3. **Config string parsing** - Move parsing logic from `Config_read*String()` functions

### Architectural Improvements

1. **Reduce global state** - More functions could take context parameter
2. ✅ **Event-based menu** - Separate input handling from rendering in Menu_options (DONE)
3. **Mock SDL layer** - Enable testing of rendering code with fake SDL

### Documentation

1. Update CLAUDE.md with new module locations
2. Add inline documentation for complex algorithms
3. Document the context pattern for future contributors

---

## Recent Improvements (December 2025)

### Menu_options() Refactoring

The ~410-line `Menu_options()` function was refactored to improve testability and reduce code duplication.

#### Phase A: Input Handling Extraction

**Created testable pure functions** (in `minarch_menu.c`):

| Function | Purpose | Lines |
|----------|---------|-------|
| `MinArchMenuNav_init()` | Initialize navigation state | ~10 |
| `MinArchMenuNav_navigate()` | Up/down navigation with pagination | ~35 |
| `MinArchMenuNav_advanceItem()` | Advance to next item | ~15 |
| `MinArchMenuNav_cycleValue()` | Left/right value cycling | ~25 |
| `MinArchMenuNav_getAction()` | Determine action from button press | ~25 |

**New types** (in `minarch_menu_types.h`):
- `MinArchMenuNavState` - Navigation state container
- `MinArchMenuAction` - Action enum (EXIT, CONFIRM, SUBMENU, etc.)

**Benefits:**
- Input logic now testable without SDL/PAD mocking
- Navigation behavior validated by 27 new unit tests
- Clearer separation of concerns (input vs. rendering)
- Menu_options() reduced from 410 to 370 lines

#### Phase B: Callback Consolidation

**Created generic helper** (`OptionsMenu_buildAndShow()`):
- Handles lazy initialization of option menus
- Builds/updates MenuItems from OptionList
- Eliminates ~80 lines of duplicate code

**Before:**
```c
static int OptionFrontend_openMenu(MenuList* list, int i) {
    // 40 lines of menu building logic
}
static int OptionEmulator_openMenu(MenuList* list, int i) {
    // 48 lines of nearly identical code
}
```

**After:**
```c
static int OptionFrontend_openMenu(MenuList* list, int i) {
    return OptionsMenu_buildAndShow(&config.frontend, &OptionFrontend_menu, NULL);
}
static int OptionEmulator_openMenu(MenuList* list, int i) {
    return OptionsMenu_buildAndShow(&config.core, &OptionEmulator_menu,
                                    "This core has no options.");
}
```

**Impact:**
- Reduced minarch.c from 5,189 → 5,140 lines (-49 lines / -1%)
- Eliminated code duplication in option menu builders
- Standardized option menu initialization pattern

### Code Cleanup and Naming Standardization (December 2025)

**Removed Dead Code:**
- Deleted unused `getAlias()` function from minarch.c (42 lines)
- Reduced minarch.c from 5,140 → 5,098 lines

**Unified Type Definitions:**
- Consolidated `Option`/`MinArchOption` duplicate type definitions
- Made `minarch_options.h` the canonical source for option types
- Updated `minarch_internal.h` to use typedef aliases for backward compatibility

**Function Naming Standardization:**
Renamed **405 function references** across 8 modules to follow consistent `MinArch[Module]_functionName` pattern:

| Module | Functions Renamed | Pattern |
|--------|-------------------|---------|
| minarch_config | 3 | `MinArch_getConfigValue` → `MinArchConfig_getValue` |
| minarch_options | 5 | `MinArch_findOption` → `MinArchOptions_find` |
| minarch_paths | 6 | `MinArch_getSRAMPath` → `MinArchPaths_getSRAM` |
| minarch_memory | 7 | `MinArch_readSRAM` → `MinArchMemory_readSRAM` |
| minarch_state | 5 | `MinArch_readState` → `MinArchState_read` |
| minarch_utils | 3 | `MinArch_getCoreName` → `MinArchUtils_getCoreName` |
| minarch_zip | 2 | `MinArch_zipCopy` → `MinArchZip_copy` |
| minarch_cpu | (earlier) | `AutoCPU_update` → `MinArchCPU_update` |

**Benefits:**
- **Eliminates ambiguity** - Module ownership is immediately clear from function name
- **Prevents collisions** - No more generic `MinArch_` prefix shared across modules
- **Improves autocomplete** - IDEs can group functions by module
- **Easier navigation** - Grep/search by module prefix finds all related functions
- **Consistent with existing modules** - Matches patterns already used in Input, Core, Menu, Env, Game modules

**Documentation:**
- Added comprehensive naming convention guide to CLAUDE.md
- Includes table of all module prefixes and example functions
- Documents type naming (`MinArchCPUState`) and constant naming (`MINARCH_CPU_MAX`)

---

## Validation

All extracted modules are validated by unit tests:

- **1,106 tests** passing
- **0 failures**
- Tests run in Docker for consistency

The high test count isn't the goal—it's evidence that the extractions preserved behavior correctly.
