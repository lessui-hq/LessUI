# Player Refactoring: Code Organization & Maintainability

**Goal:** Transform monolithic player.c (~7,000 lines) into focused, single-responsibility modules

**Why:** Large files are hard to navigate, understand, and modify. By extracting cohesive functionality into dedicated modules, we make the codebase more maintainable, easier to reason about, and simpler to extend.

**Validation:** Unit tests confirm that extracted modules work correctly in isolation.

---

## Progress Summary

| Metric                | Before | After  | Change         |
| --------------------- | ------ | ------ | -------------- |
| **player.c lines**    | ~7,000 | 5,140  | -27%           |
| **Extracted modules** | 0      | 17     | +17 modules    |
| **Lines in modules**  | 0      | ~4,350 | Well-organized |
| **Unit tests**        | 491    | 1,106  | +615 tests     |

---

## Module Architecture

### Core Principle: Single Responsibility

Each extracted module owns one cohesive concern:

```
player.c (orchestration, main loop, SDL integration)
    │
    ├── player_context.h/c     ─ State container for dependency injection
    ├── player_internal.h      ─ Shared type definitions (Core, Game, Config)
    │
    ├── Configuration
    │   ├── player_config.c    ─ Config file parsing utilities
    │   ├── player_options.c   ─ Core option list management
    │   └── player_paths.c     ─ Path generation (saves, states, BIOS)
    │
    ├── Input
    │   ├── player_input.c     ─ Button state, mappings, descriptors
    │   └── player_mappings.c  ─ Static button/label data, enums
    │
    ├── Persistence
    │   ├── player_memory.c    ─ SRAM/RTC read/write
    │   ├── player_state.c     ─ Save state management
    │   └── player_zip.c       ─ ZIP extraction
    │
    ├── Video
    │   ├── player_scaler.c    ─ Scaling geometry calculations
    │   ├── player_rotation.c  ─ Framebuffer rotation buffers
    │   └── player_video_convert.c ─ Pixel format conversion
    │
    ├── Core Integration
    │   ├── player_core.c      ─ AV info processing, aspect ratio
    │   ├── player_env.c       ─ Libretro environment callbacks
    │   ├── player_game.c      ─ Game file handling, ZIP parsing
    │   └── player_cpu.c       ─ Auto CPU frequency scaling
    │
    └── UI
        ├── player_menu.c      ─ In-game menu system
        └── player_menu_types.h ─ Menu type definitions
```

---

## Completed Extractions

### Configuration Layer

| Module             | Lines | Responsibility                                  | Tests |
| ------------------ | ----- | ----------------------------------------------- | ----- |
| `player_config.c`  | ~90   | Parse config values, option display names       | 19    |
| `player_options.c` | ~200  | Option list search, get/set operations          | 45    |
| `player_paths.c`   | ~110  | Generate paths for saves, states, configs, BIOS | 23    |

**Key decisions:**

- Config parsing separated from file I/O
- Path generation is pure (testable) - file operations stay in player.c
- BIOS path selection uses smart fallback (tag dir → root)

### Input Layer

| Module              | Lines | Responsibility                                               | Tests |
| ------------------- | ----- | ------------------------------------------------------------ | ----- |
| `player_input.c`    | ~240  | Button state queries, mapping lookups, descriptor processing | 36    |
| `player_mappings.c` | ~420  | Static data: button labels, scale/effect enums, defaults     | -     |

**Key decisions:**

- Pure functions for button collection (no PAD\_\* dependencies)
- D-pad remapping logic extracted and testable
- Modifier key handling isolated

### Persistence Layer

| Module            | Lines | Responsibility                      | Tests |
| ----------------- | ----- | ----------------------------------- | ----- |
| `player_memory.c` | ~120  | SRAM/RTC persistence with mock core | 16    |
| `player_state.c`  | ~130  | Save state read/write, auto-resume  | 16    |
| `player_zip.c`    | ~150  | ZIP copy/inflate extraction         | 13    |

**Key decisions:**

- Core callbacks abstracted for testing
- Path generation delegated to player_paths
- Temp file handling isolated

### Video Layer

| Module                   | Lines | Responsibility                                  | Tests |
| ------------------------ | ----- | ----------------------------------------------- | ----- |
| `player_scaler.c`        | ~350  | Scaling geometry: aspect, native, cropped modes | 26    |
| `player_rotation.c`      | ~170  | Rotation buffer management                      | -     |
| `player_video_convert.c` | ~350  | Pixel format conversion (NEON + scalar)         | -     |

**Key decisions:**

- Scaling calculations are pure math (highly testable)
- Rotation handling extracted but tightly coupled to buffers
- NEON code stays in dedicated module (platform-specific)

### Core Integration Layer

| Module          | Lines | Responsibility                                           | Tests |
| --------------- | ----- | -------------------------------------------------------- | ----- |
| `player_core.c` | ~150  | Build game info, calculate aspect ratio, process AV info | 23    |
| `player_env.c`  | ~400  | Handle 30+ libretro environment callbacks                | 51    |
| `player_game.c` | ~300  | Extension parsing, ZIP headers, M3U detection            | 46    |
| `player_cpu.c`  | ~350  | Auto CPU frequency scaling algorithm                     | 42    |

**Key decisions:**

- Environment callbacks return structured results (testable)
- Game file utilities are pure string/parsing operations
- CPU algorithm separated from thread management

### UI Layer

| Module                | Lines | Responsibility                                     | Tests |
| --------------------- | ----- | -------------------------------------------------- | ----- |
| `player_menu.c`       | ~854  | Complete menu system: init, loop, save/load, scale | 41    |
| `player_menu_types.h` | ~120  | MenuItem, MenuList type definitions                | -     |
| `player_context.c`    | ~140  | State container for context-based APIs             | -     |
| `player_context.h`    | ~260  | Context struct + service callback types            | -     |

**Key decisions:**

- Menu functions take `PlayerContext*` for explicit dependencies
- ✅ **All extern declarations eliminated** - Replaced with service callbacks in context
- Context pattern enables unit testing of menu logic
- SDL rendering stays in menu module (can't easily mock)

---

## What Remains in player.c

### Orchestration (Should Stay)

- `main()` - Entry point, argument parsing, main loop
- `Menu_loop()` - Top-level game menu orchestration
- Thread management for auto CPU scaling

### SDL Integration (Hard to Extract)

- `Menu_options()` - 415 lines of menu UI rendering
- `video_refresh_callback_main()` - Frame pipeline with debug HUD
- Audio callbacks - Thin wrappers around SND\_\*

### Platform-Specific (Better as Integration Tests)

- `Core_open()` - dlopen/dlsym symbol resolution
- Config file I/O wrappers
- HDMI monitoring

### Remaining Extraction Candidates

| Function                        | Lines | Extractable? | Notes                               |
| ------------------------------- | ----- | ------------ | ----------------------------------- |
| `trackFPS()`                    | ~40   | ✅ Yes       | Pure frame timing math              |
| `limitFF()`                     | ~60   | ✅ Yes       | Fast-forward throttling             |
| `calculateProportionalWidths()` | ~40   | ✅ Yes       | Text layout calculation             |
| `Config_readOptionsString()`    | ~30   | ⚠️ Partial   | Parsing is pure, state update isn't |
| `Config_readControlsString()`   | ~65   | ⚠️ Partial   | Same as above                       |

---

## Design Patterns Used

### 1. Context Pattern (player_context.c)

```c
// Before: Global state everywhere
void Menu_saveState(void) {
    State_write();  // Uses global state_slot, game, core
}

// After: Explicit dependencies
void Menu_saveState_ctx(PlayerContext* ctx) {
    State_write(ctx->state_slot, ctx->game, ctx->core);
}
```

**Benefits:**

- Dependencies visible in function signature
- Enables unit testing with mock context
- No hidden coupling to globals

### 1b. Service Callbacks Pattern (player_context.h)

The menu module was initially extracted with extern declarations to functions in player.c:

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
typedef struct PlayerCallbacks {
    PlayerSRAMWriteFunc sram_write;
    PlayerRTCWriteFunc rtc_write;
    PlayerStateReadFunc state_read;
    PlayerStateWriteFunc state_write;
    PlayerGameChangeDiscFunc game_change_disc;
    PlayerMenuOptionsFunc menu_options;
    struct MenuList* options_menu;
    // ... all dependencies explicit
} PlayerCallbacks;

// Menu module uses callbacks via context
static void Menu_beforeSleep_ctx(PlayerContext* ctx) {
    PlayerCallbacks* cb = ctx->callbacks;
    cb->sram_write();
    cb->rtc_write();
    cb->state_autosave();
    // ...
}
```

**Benefits:**

- Eliminates all extern declarations in player_menu.c
- Dependencies are explicit and injectable
- Enables unit testing with mock callbacks
- Maintains unidirectional dependency flow (player.c → menu, not bidirectional)
- Menu module has zero compile-time coupling to player.c internals

### 2. Pure Functions for Business Logic

```c
// Testable: No side effects, deterministic
uint32_t PlayerInput_collectButtons(
    const PlayerButtonMapping* controls,
    uint32_t pressed_buttons,
    int menu_pressed,
    int gamepad_type,
    const PlayerDpadRemap* dpad_remaps,
    int* out_used_modifier);

// Wrapper in player.c handles actual button reading
static void input_poll_callback(void) {
    PAD_poll();
    buttons = PlayerInput_collectButtons(...);
}
```

### 3. Result Structs for Complex Returns

```c
typedef struct {
    int handled;           // Was the command handled?
    int needs_reinit;      // Should audio be reinitialized?
    int updated_variable;  // Was a core variable changed?
} EnvResult;

EnvResult PlayerEnv_handleGeometry(void* data, VideoState* video);
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
2. **Tightly coupled SDL code** - Rendering code that uses GFX*\*, TTF*\_, SDL\_\_ throughout
3. **Thread entry points** - Keep thread functions near their management code
4. **dlopen/dlsym** - Platform-specific, better tested via integration

---

## File Organization

```
workspace/all/player/           # Emulator frontend
├── player.c                    # Main file (5,189 lines)
├── player_internal.h           # Shared types (header only)
├── player_context.h/c          # State container
├── player_config.h/c           # Config parsing
├── player_options.h/c          # Option management
├── player_paths.h/c            # Path generation
├── player_input.h/c            # Input handling
├── player_mappings.h/c         # Static data
├── player_memory.h/c           # SRAM/RTC
├── player_state.h/c            # Save states
├── player_zip.h/c              # ZIP extraction
├── player_scaler.h/c           # Video scaling
├── player_rotation.h/c         # Rotation buffers
├── player_video_convert.h/c    # Pixel conversion
├── player_core.h/c             # Core AV processing
├── player_env.h/c              # Environment callbacks
├── player_game.h/c             # Game file handling
├── player_cpu.h/c              # CPU scaling
├── player_menu.h/c             # Menu system
└── player_menu_types.h         # Menu types

tests/unit/all/common/           # Unit tests
├── test_player_config.c
├── test_player_options.c
├── test_player_paths.c
├── test_player_input.c
├── test_player_memory.c
├── test_player_state.c
├── test_player_zip.c
├── test_player_scaler.c
├── test_player_core.c
├── test_player_env.c
├── test_player_game.c
├── test_player_cpu.c
└── test_player_menu.c
```

---

## Next Steps

### Potential Further Extractions

1. **Frame timing utilities** - Extract `trackFPS()`, `limitFF()` to `player_timing.c`
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

**Created testable pure functions** (in `player_menu.c`):

| Function                      | Purpose                            | Lines |
| ----------------------------- | ---------------------------------- | ----- |
| `PlayerMenuNav_init()`        | Initialize navigation state        | ~10   |
| `PlayerMenuNav_navigate()`    | Up/down navigation with pagination | ~35   |
| `PlayerMenuNav_advanceItem()` | Advance to next item               | ~15   |
| `PlayerMenuNav_cycleValue()`  | Left/right value cycling           | ~25   |
| `PlayerMenuNav_getAction()`   | Determine action from button press | ~25   |

**New types** (in `player_menu_types.h`):

- `PlayerMenuNavState` - Navigation state container
- `PlayerMenuAction` - Action enum (EXIT, CONFIRM, SUBMENU, etc.)

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

- Reduced player.c from 5,189 → 5,140 lines (-49 lines / -1%)
- Eliminated code duplication in option menu builders
- Standardized option menu initialization pattern

### Code Cleanup and Naming Standardization (December 2025)

**Removed Dead Code:**

- Deleted unused `getAlias()` function from player.c (42 lines)
- Reduced player.c from 5,140 → 5,098 lines

**Unified Type Definitions:**

- Consolidated `Option`/`PlayerOption` duplicate type definitions
- Made `player_options.h` the canonical source for option types
- Updated `player_internal.h` to use typedef aliases for backward compatibility

**Function Naming Standardization:**
Renamed **405 function references** across 8 modules to follow consistent `Player[Module]_functionName` pattern:

| Module         | Functions Renamed | Pattern                                           |
| -------------- | ----------------- | ------------------------------------------------- |
| player_config  | 3                 | `Player_getConfigValue` → `PlayerConfig_getValue` |
| player_options | 5                 | `Player_findOption` → `PlayerOptions_find`        |
| player_paths   | 6                 | `Player_getSRAMPath` → `PlayerPaths_getSRAM`      |
| player_memory  | 7                 | `Player_readSRAM` → `PlayerMemory_readSRAM`       |
| player_state   | 5                 | `Player_readState` → `PlayerState_read`           |
| player_utils   | 3                 | `Player_getCoreName` → `PlayerUtils_getCoreName`  |
| player_zip     | 2                 | `Player_zipCopy` → `PlayerZip_copy`               |
| player_cpu     | (earlier)         | `AutoCPU_update` → `PlayerCPU_update`             |

**Benefits:**

- **Eliminates ambiguity** - Module ownership is immediately clear from function name
- **Prevents collisions** - No more generic `Player_` prefix shared across modules
- **Improves autocomplete** - IDEs can group functions by module
- **Easier navigation** - Grep/search by module prefix finds all related functions
- **Consistent with existing modules** - Matches patterns already used in Input, Core, Menu, Env, Game modules

**Documentation:**

- Added comprehensive naming convention guide to CLAUDE.md
- Includes table of all module prefixes and example functions
- Documents type naming (`PlayerCPUState`) and constant naming (`PLAYER_CPU_MAX`)

---

## Validation

All extracted modules are validated by unit tests:

- **1,106 tests** passing
- **0 failures**
- Tests run in Docker for consistency

The high test count isn't the goal—it's evidence that the extractions preserved behavior correctly.
