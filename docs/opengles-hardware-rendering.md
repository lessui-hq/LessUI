# OpenGL ES Hardware Rendering Implementation

## Overview

LessUI now supports OpenGL ES hardware-accelerated rendering for libretro cores that require GPU rendering (Flycast, PPSSPP, Beetle PSX HW, etc.). This enables 3D-intensive cores to run at full speed on devices with Mali GPUs.

**Status:** Working on tg5040, zero28 - displays correctly with aspect ratio preservation and rotation

**Supported Platforms:**

- rg35xxplus (Anbernic RG35XX Plus/H/SP series - Mali GPU)
- tg5040 (TrimUI Smart Pro - Mali GPU)
- zero28 (MagicX Mini Zero 28 - Mali GPU, portrait display with 90° rotation)
- my355 (Miyoo Flip - RK3566, Mali G52 GPU)

**Supported GLES Versions:**

- GLES 2.0 (baseline, always available)
- GLES 3.0, 3.1, 3.2 (with automatic fallback if not available)

## Architecture

### Design Philosophy

Following LessUI's focus on simplicity, the implementation:

- Uses SDL2's OpenGL ES context management (not raw EGL)
- Loads GL functions dynamically via `SDL_GL_GetProcAddress` (no system header dependencies)
- Supports GLES 2.0 through 3.2 with automatic version fallback
- Provides clean fallback to software rendering on unsupported platforms
- Unified video backend - ~2100 lines handling both HW and SW cores

### Module Structure

```
gl_video.h/c - Unified GL video backend (~2100 lines)
    ├── GL Context Management (SDL2-based)
    ├── FBO Creation & Lifecycle (HW cores)
    ├── Triple-buffered Textures (SW cores)
    ├── Shader Program (vertex + fragment)
    ├── Frame Presentation (fullscreen quad blit)
    ├── CRISP 2-pass Sharpening (intermediate FBO)
    └── Platform Capability Detection
```

### Pipeline Flow

```
1. Core requests HW rendering (during retro_load_game)
   └─> RETRO_ENVIRONMENT_SET_HW_RENDER

2. Create GL context + FBO (still during load_game)
   ├─> Determine target GLES version from context type
   │   ├─> OPENGLES2 → GLES 2.0
   │   ├─> OPENGLES3 → GLES 3.0
   │   └─> OPENGLES_VERSION → use version_major.version_minor
   ├─> Try to create context (with fallback to lower versions)
   ├─> Load GL function pointers
   ├─> Create FBO with RGBA8888 + depth24/stencil8
   └─> Compile presentation shaders

3. After retro_load_game returns
   └─> Call core's context_reset() (per libretro spec)

4. Frame rendering loop
   ├─> Bind FBO before core.run()
   ├─> Core renders to FBO
   ├─> Core signals RETRO_HW_FRAME_BUFFER_VALID
   └─> Present FBO to screen (shader-based blit)

5. Cleanup
   ├─> Call core's context_destroy()
   ├─> Destroy FBO resources
   └─> Destroy GL context
```

## Implementation Details

### Phase 1: Infrastructure

**Platform Capability Detection:**

- Added `HAS_OPENGLES` define to platform.h (rg35xxplus, tg5040)
- Default `HAS_OPENGLES 0` in defines.h for non-GPU platforms
- Conditional compilation keeps software path unchanged

**Files Modified:**

- `workspace/rg35xxplus/platform/platform.h` - Added `HAS_OPENGLES 1`
- `workspace/tg5040/platform/platform.h` - Added `HAS_OPENGLES 1`
- `workspace/all/common/defines.h` - Added capability default
- `workspace/all/common/gl_video.h` - Unified GL video API (~450 lines, includes stubs)
- `workspace/all/common/gl_video.c` - Complete implementation (~2100 lines)
- `workspace/all/common/Makefile` - Added gl_video.c to sources

**Integration Points:**

- `player.c` - Environment callback handling
  - `RETRO_ENVIRONMENT_SET_HW_RENDER` (line ~2626) - Initializes HW rendering
  - `RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER` (line ~2816) - Reports GLES2 preference
  - `video_refresh_callback` (line ~3866) - Handles HW frame presentation
  - `Core_quit()` shutdown (line ~4169) - Calls GLVideo_prepareShutdown()

### Phase 2: GL Context Creation

**SDL2 Window Modifications:**

- Modified `render_sdl2.c` to create windows with `SDL_WINDOW_OPENGL` flag on `HAS_OPENGLES` platforms
- Added `SDL2_getWindow()` to expose window pointer
- Added `PLAT_getWindow()` to platform API (rg35xxplus, tg5040)

**GL Context Management:**

- Creates SDL_GLContext with GLES 2.0 profile attributes
- Makes context current before GL operations
- Provides `get_proc_address` callback via `SDL_GL_GetProcAddress`
- Proper cleanup in shutdown path

**Files Modified:**

- `workspace/all/common/render_sdl2.h` - Added `SDL2_getWindow()`
- `workspace/all/common/render_sdl2.c` - Window with GL support, getter function
- `workspace/all/common/api.h` - Added `PLAT_getWindow()` (SDL2 only)
- `workspace/rg35xxplus/platform/platform.c` - Implemented `PLAT_getWindow()`
- `workspace/tg5040/platform/platform.c` - Implemented `PLAT_getWindow()`

### Phase 3: FBO Management

**FBO Configuration:**

- Color attachment: RGBA8888 texture (better compatibility than RGB565)
- Depth attachment: GL_DEPTH_COMPONENT16 renderbuffer (when requested)
- Packed depth/stencil: GL_DEPTH24_STENCIL8_OES (when both requested)
- FBO completeness validation
- Dynamic resizing support

**Resource Management:**

- `createFBO()` - Generates FBO with attachments (internal)
- `destroyFBO()` - Cleans up all FBO resources (internal)
- `GLVideo_resizeFBO()` - Recreates FBO with new dimensions
- `GLVideo_getCurrentFramebuffer()` - Returns FBO ID to cores

### Phase 4: Frame Presentation

**Shader System:**

Minimal GLES 2.0 shaders:

- **Vertex Shader:** MVP matrix transformation, passes texcoords through
- **Fragment Shader:** Simple texture lookup

**Presentation Pipeline:**

1. Make GL context current
2. Bind backbuffer (FBO 0)
3. Calculate aspect-preserving viewport (letterbox/pillarbox)
4. Set viewport and clear to black
5. Bind FBO texture and shader program
6. Set MVP matrix (identity transform: 0-1 → NDC)
7. Scale texture coordinates to sample rendered portion of FBO
8. Set up vertex attributes (position + texcoord, separate arrays)
9. Unbind VBO (use client-side arrays)
10. Draw quad with `GL_TRIANGLE_STRIP`
11. Swap buffers via `SDL_GL_SwapWindow()`

**Design Decisions:**

- Simple and direct - minimal transformation steps
- No VBO - client-side vertex arrays
- Texture scaling in texture coordinates (not shader)
- Cached shader locations (no per-frame `glGet*` calls)
- Aspect ratio handled via viewport (not vertex transformation)

### Scaling and Sharpness

**Scaling Modes** (integrated from `PlayerScaler`):

All 4 scaling modes from SDL2 software rendering are supported:

- **Native**: Integer scaling with centered image
- **Aspect**: Maintain aspect ratio with letterboxing/pillarboxing
- **Fullscreen**: Stretch to fill entire screen
- **Cropped**: Crop edges to fill screen while maintaining aspect

**Sharpness Filtering**:

| Mode      | SDL2 Software Path                         | HW Rendering (OpenGL) Path            |
| --------- | ------------------------------------------ | ------------------------------------- |
| **Sharp** | Nearest-neighbor only                      | `GL_NEAREST` (pixel-perfect)          |
| **Crisp** | 2-pass: 4x NN upscale → bilinear downscale | 2-pass: 4x `GL_NEAREST` → `GL_LINEAR` |
| **Soft**  | Bilinear only                              | `GL_LINEAR` (smooth)                  |

**CRISP mode implementation**: Both SDL2 and GL paths now use identical 2-pass rendering for sources smaller than the device resolution. The GL path creates an intermediate FBO at 4x source resolution, renders with `GL_NEAREST` for sharp pixel upscaling, then samples with `GL_LINEAR` for smooth downscaling to final size. This produces the same crisp-but-smooth result as the SDL2 path.

## GL Function Loading

**Approach:** No system GLES headers required - all functions loaded dynamically.

**Implementation:**

1. Defined minimal GL types (`GLuint`, `GLint`, `GLenum`, etc.)
2. Defined only needed GL constants (~40 total)
3. Declared GL function pointers as static variables
4. `loadGLFunctions()` loads all via `SDL_GL_GetProcAddress()` at init

**Functions Loaded (35+):**

- FBO: `glGenFramebuffers`, `glBindFramebuffer`, `glCheckFramebufferStatus`, etc.
- Textures: `glGenTextures`, `glBindTexture`, `glTexImage2D`, etc.
- Shaders: `glCreateShader`, `glCompileShader`, `glLinkProgram`, etc.
- Rendering: `glDrawArrays`, `glViewport`, `glClear`, etc.
- State Query: `glGetIntegerv`, `glIsEnabled` (for state save/restore)

**Why This Works:**

- SDL2 internally links against libGLESv2/libEGL
- `SDL_GL_GetProcAddress` provides all GL functions at runtime
- No compile-time dependency on GLES headers in toolchain
- Fully portable across platforms

## Code Review Findings & Fixes

### First Code Review (Internal)

**Blockers Fixed:**

1. ✅ Added GL library linkage (later removed - not needed with dynamic loading)
2. ✅ Added GL header includes (later replaced with custom definitions)
3. ✅ Fixed callback assignment ordering (memcpy after setting callbacks)
4. ✅ Added error checking in `makeCurrent()`

### RetroArch Expert Review

**Critical Fixes Applied:**

1. ✅ **Context Reset Timing**: `context_reset` is now called AFTER `retro_load_game()` returns, not during (per libretro spec)
2. ✅ **GL State Restoration**: Added comprehensive state reset in `present()` (disable depth/stencil/blend/cull/scissor tests)
3. ✅ **Removed context_reset from resizeFBO**: FBO resize no longer triggers unexpected context recreation
4. ✅ **Backbuffer Binding**: Explicitly bind FBO 0 before presentation pass
5. ✅ **FBO Binding Before Core**: Added `GLVideo_bindFBO()` call before `core.run()` in both loop files

**Important Improvements:**

- RGBA8888 texture format (was RGB565)
- Error state on resize failure (`hw_state.enabled = false`)
- Cached shader locations for performance
- Removed unused `quad_vao` field

### Menu Integration Fixes

**SDL/GL Context Conflicts:**

1. ✅ **Skip SDL_Renderer for HW cores**: Modified main loop to skip `GFX_present(&renderer)` when HW rendering active (frame already presented via GL)
2. ✅ **GL surface presentation**: Added `GLVideo_presentSurface()` to render SDL surfaces via GL instead of SDL_Renderer
3. ✅ **Menu crash fix**: Skip bitmap surface creation for HW cores (frame is in GPU memory, not CPU-accessible)
4. ✅ **Texture coordinate fix**: Y-flip texture coordinates for SDL surfaces (top-left origin → bottom-left GL origin)
5. ✅ **Dynamic resolution**: Use `SDL_GetWindowSize()` instead of hardcoded 1280x720

**Key Implementation:**

- Menu uses `GLVideo_presentSurface()` when HW rendering active
- Uploads SDL surface to GL texture and renders via shader
- Avoids SDL_Renderer/GL context conflicts that caused crashes
- Black background for HW cores (game screenshot requires GL readback)

### OpenGL ES Expert Review (2025-12-23)

**Critical Fixes Applied:**

1. ✅ **Blank Screen Fix**: Changed guard checks in `presentSurface()`, `swapBuffers()`, and `renderHUD()` from `!enabled` to `!context_ready`. The `enabled` flag means "HW core active", not "GL ready" - software mode has `enabled=false` but `context_ready=true`.

2. ✅ **GL State Save/Restore**: Added comprehensive state save/restore in `drawFrame()` when HW core is active. Saves/restores: program, texture binding, active texture, viewport, blend enable, depth test enable. Prevents state corruption between core rendering and frontend presentation.

3. ✅ **Shutdown Resource Leak**: Fixed `GLVideo_shutdown()` to check `context_ready` instead of `enabled`, so software-only GL contexts are properly cleaned up.

4. ✅ **Shader Precision Qualifier**: Added `precision highp float;` to vertex shader for Mali GPU compatibility (some older Mali drivers require explicit precision).

**Robustness Improvements:**

5. ✅ **FBO Validation**: Added check in `GLVideo_bindFBO()` that FBO handle is non-zero before binding.

6. ✅ **Shader Attribute Validation**: Added validation that `a_position` and `a_texcoord` locations are >= 0 after shader linking, with proper cleanup on failure.

7. ✅ **Division by Zero Guard**: Added validation in `GLVideo_drawFrame()` that texture dimensions are non-zero.

8. ✅ **Version Probe Context Restore**: Fixed `GLVideo_isVersionSupported()` to save and restore the current GL context after probing.

**New GL Functions Loaded:**

- `glGetIntegerv` - Query GL state (program, texture, viewport)
- `glIsEnabled` - Query GL capability state (blend, depth test)

### Shutdown Fix (2025-12-23)

**Shutdown Crash Fix:**

1. ✅ **Split shutdown into two phases**: Added `GLVideo_prepareShutdown()` that calls `context_destroy` but keeps GL context alive. The full `GLVideo_shutdown()` is deferred until after `dlclose()`.

2. ✅ **Fixed shutdown order**: The new sequence is:
   - `GLVideo_prepareShutdown()` - notifies core, calls `context_destroy`
   - `core.unload_game()` and `core.deinit()` - core cleanup
   - `dlclose(core.handle)` - unload core .so (destructors can still use GL)
   - `GLVideo_shutdown()` (via `GFX_quit()`) - destroy GL resources and context

3. ✅ **Fixed window/context destruction order**: `GLVideo_shutdown()` now called before `SDL_DestroyWindow()` in `SDL2_quitVideo()`.

## Testing Results

### Build Status

- ✅ All 1508 unit tests pass
- ✅ Lint passes
- ✅ Code formatted
- ✅ Cross-compiles for rg35xxplus (181KB)
- ✅ Cross-compiles for tg5040 (173KB)

### Platform Testing

**tg5040 (TrimUI Smart Pro - PowerVR Rogue GE8300):**

- ✅ GL context creation succeeds (GLES 2.0)
- ✅ FBO created successfully (1024x1024, RGBA8888, depth+stencil)
- ✅ Shader compiled and linked
- ✅ **Flycast (Dreamcast)**: Fully working - renders frames, menu functional, proper aspect ratio
- ❌ **PPSSPP (PSP)**: Crashes during `context_reset` after GPU detection
- ❌ **Mupen64Plus (N64)**: Crashes after ~1 second in main loop
- **GPU**: PowerVR Rogue GE8300, OpenGL ES 3.2 capable
- **Software renderer**: Used for SDL to avoid GL conflicts

**rg35xxplus (Anbernic RG35XX Plus/H/SP - Mali-G31):**

- ✅ GL context creation succeeds (GLES 2.0)
- ✅ FBO created successfully (1024x1024)
- ⚠️ **Mupen64Plus (N64)**: Renders multiple frames successfully, crashes during runtime
- ❌ **Flycast (Dreamcast)**: Crashes during `retro_init` (memory mapping, not GL-related)
- **GPU**: Mali-G31, OpenGL ES 3.2 capable
- **Note**: Better N64 compatibility than tg5040, worse DC compatibility

### libretro Specification Compliance

**Verified against RetroArch reference implementation (December 2024).**

✅ **Environment Callbacks:**

- `RETRO_ENVIRONMENT_SET_HW_RENDER` - Accepts GLES2/3 requests
- `RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER` - Reports OPENGLES2
- `get_current_framebuffer` - Returns FBO handle
- `get_proc_address` - Provides GL function pointers via SDL_GL_GetProcAddress
- `context_reset` - Called **after** `retro_load_game()` returns (per libretro spec)
- `context_destroy` - Called before core unload via `GLVideo_prepareShutdown()`

✅ **All 12 `retro_hw_render_callback` Fields Handled:**

| Field                     | Implementation                                 |
| ------------------------- | ---------------------------------------------- |
| `context_type`            | Checked via `GLVideo_isContextSupported()`     |
| `context_reset`           | Stored and called via `GLVideo_contextReset()` |
| `get_current_framebuffer` | Set to `GLVideo_getCurrentFramebuffer()`       |
| `get_proc_address`        | Set to `GLVideo_getProcAddress()`              |
| `depth`                   | FBO created with depth attachment if true      |
| `stencil`                 | FBO created with stencil attachment if true    |
| `bottom_left_origin`      | Handled in texture coordinate generation       |
| `version_major`           | Used for GLES version negotiation              |
| `version_minor`           | Used for GLES version negotiation              |
| `cache_context`           | Implicit - context only destroyed on shutdown  |
| `context_destroy`         | Called in `GLVideo_prepareShutdown()`          |
| `debug_context`           | Passed to SDL_GL_CONTEXT_DEBUG_FLAG            |

✅ **Frame Handling:**

- `RETRO_HW_FRAME_BUFFER_VALID` detection in `video_refresh_callback`
- FBO binding before `core.run()` via `GLVideo_bindFBO()`
- Clean separation from software rendering path

✅ **Optional Callbacks Implemented:**

- `RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT` - Acknowledged (shared contexts not needed for simple frontend)
- `RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE` - Returns false (OpenGL/GLES has no interface in libretro spec; RetroArch's gl2/gl3 drivers also return NULL for this)

## Current Status

### Recent Work (2025-12-23)

**GLES Version Detection & Compliance Review:**

- ✅ Added GLES 2.0/3.0/3.1/3.2 support with automatic fallback
- ✅ Version negotiation following RetroArch pattern
- ✅ Context type mapping: OPENGLES2→2.0, OPENGLES3→3.0, OPENGLES_VERSION→version_major.version_minor
- ✅ All 12 `retro_hw_render_callback` fields properly handled per libretro spec
- ✅ Fixed `bottom_left_origin` handling (prevents upside-down rendering)
- ✅ Added `debug_context` support for GL debug contexts

**Scaling Integration:**

- ✅ All 4 scaling modes integrated (Native, Aspect, Fullscreen, Cropped)
- ✅ Sharpness/filtering support (Sharp=GL_NEAREST, Crisp/Soft=GL_LINEAR)
- ✅ Direct viewport calculation (PlayerScaler designed for software rendering)
- ✅ Source cropping for CROPPED mode
- ✅ Fixed viewport clear (full screen clear before setting render viewport)
- ✅ Texture filtering applied per-frame based on sharpness setting

**Bug Fixes:**

- ✅ Prevented `renderer.dst_p = 0` assignments when HW rendering active
- ✅ Guarded `select_scaler()` call in menu to skip when renderer uninitialized
- ✅ Added `!show_menu` guard to prevent core execution during menu
- ✅ Added explicit FBO rebinding after menu exit
- ✅ Fixed menu crash when changing scaling options (was dereferencing NULL `m->bitmap`)

**Screenshot Feature:**

- ✅ Added `GLVideo_captureFrame()` using `glReadPixels()` to capture FBO
- ✅ Converts RGBA8888 → RGB565 with Y-flip (GL bottom-left → SDL top-left origin)
- ✅ Menu now shows game screenshot as background (same as software rendering)
- ✅ Fallback to black background if capture fails

**Current Status:**

- ✅ Aspect scaling displays correctly (proper pillarboxing)
- ✅ All scaling modes functional (Native, Aspect, Fullscreen, Cropped)
- ✅ Sharpness settings work (Sharp/Crisp/Soft)
- ✅ Menu scaling changes work without crashes
- ✅ Game screenshot visible behind menu

### Recent Work (2025-12-24)

**Menu Flickering Fix (GLES Platforms):**

- ✅ **Issue**: Software cores on GLES platforms (like Tetris) caused menu flickering because the backbuffer wasn't cleared, retaining old menu content in pillarbox/letterbox areas.
- ✅ **Fix**: Implemented `GLVideo_clear()` to explicitly clear the default framebuffer to black.
- ✅ **Integration**: Modified `SDL2_present` to call `GLVideo_clear()` before drawing software frames on GLES platforms.

**Performance & Responsiveness:**

- ✅ **Startup/Shutdown Optimization**: Consolidated filesystem `sync()` calls. Removed redundant syncs from individual write functions (SRAM, RTC, State, Config) and placed them at key transition points (sleep, shutdown, manual save). Eliminated "triple-sync" delays.
- ✅ **Immediate Screen Blanking**: Added `GFX_clear` + `GFX_present(NULL)` at the start of the exit sequence in both `player` and `launcher`. Provides instant visual feedback when quitting or launching games.
- ✅ **Shutdown "Flash" Fix**: Added a guard in `video_refresh_callback` (`if (quit) return;`) to prevent cores from drawing late frames after the screen has been blanked.
- ✅ **Cleanup**: Removed redundant `SND_quit()` calls and the confusing `Core_unload()` wrapper.
- ✅ **Safety**: Added a final `sync()` to the launcher's exit path to ensure data integrity.

**Portrait Display Rotation Fix (zero28):**

- ✅ **Issue**: Portrait devices like zero28 (480×640 physical, 640×480 logical) require 90° rotation. The GL path wasn't applying rotation correctly because SDL uses clockwise angles while OpenGL's rotation matrix uses counter-clockwise.
- ✅ **Fix**: Added rotation direction conversion in three places:
  - `GLVideo_presentSurface()` - UI/launcher rendering now applies platform rotation
  - `GLVideo_drawSoftwareFrame()` - Software core frames now rotate correctly
  - `GLVideo_present()` - HW core platform rotation is converted before combining with core rotation
- ✅ **Conversion Formula**: `gl_rotation = (sdl_rotation == 0) ? 0 : (4 - sdl_rotation)`
  - SDL rotation 1 (90° CW) → GL rotation 3 (270° CCW = 90° CW visually)
  - SDL rotation 3 (270° CW) → GL rotation 1 (90° CCW = 270° CW visually)
- ✅ **Tested**: Working correctly on zero28 for both UI and games.

### Working Features

- ✅ GL context creation (SDL2-based, GLES 2.0/3.0/3.1/3.2)
- ✅ Version negotiation with automatic fallback (3.2 → 3.1 → 3.0 → 2.0)
- ✅ FBO management (create, resize, destroy)
- ✅ Shader compilation and linking
- ✅ Dynamic GL function loading
- ✅ Frame presentation to screen
- ✅ All scaling modes (Native, Aspect, Fullscreen, Cropped)
- ✅ Sharpness/filtering (Sharp=GL_NEAREST, Crisp/Soft=GL_LINEAR)
- ✅ Texture coordinate scaling for partial FBO sampling
- ✅ Source cropping (for CROPPED scaling mode)
- ✅ `bottom_left_origin` handling (correct orientation per libretro spec)
- ✅ `debug_context` support (GL debug contexts when requested)
- ✅ `cache_context` (implicit - context only destroyed on shutdown)
- ✅ In-game menu rendering via GL (no SDL/GL conflicts)
- ✅ Game screenshot behind menu (via `glReadPixels()` FBO capture)
- ✅ Dynamic screen resolution (no hardcoded values)
- ✅ Debug HUD overlay with alpha blending (FPS, CPU usage, resolution)
- ✅ Software renderer for SDL (avoids GL context conflicts)
- ✅ Proper `context_reset` timing (after `retro_load_game` per libretro spec)
- ✅ GL error handling (drain after context_reset, reduce log spam)
- ✅ All 12 `retro_hw_render_callback` fields properly handled
- ✅ GL state save/restore (prevents state corruption with HW cores)
- ✅ Proper resource cleanup in both HW and SW modes
- ✅ Rotation support (0°/90°/180°/270° via MVP matrix, SDL CW → GL CCW conversion)
- ✅ Unified video backend (`gl_video.*` in common/, zero `#if HAS_OPENGLES` in player.c)
- ✅ 2-pass CRISP sharpening (intermediate FBO with GL_NEAREST upscale → GL_LINEAR downscale)

### Platform/Core Compatibility

**tg5040 (PowerVR Rogue GE8300):**

- ✅ **Flycast (Dreamcast)**: Fully working - all scaling modes, menu with screenshot
- ❌ **PPSSPP (PSP)**: Crashes during `context_reset` after logging GPU info
- ❌ **Mupen64Plus-Next (N64)**: Crashes after ~1 second during main loop

**rg35xxplus (Mali-G31):**

- ⚠️ **Mupen64Plus-Next (N64)**: Renders multiple frames successfully before crash
- ❌ **Flycast (Dreamcast)**: Crashes during `retro_init` (memory mapping issue, not GL-related)

**zero28 (Mali GPU, portrait display):**

- ✅ **Software cores**: Working with 90° rotation for portrait display
- ✅ **UI/Launcher**: Correctly rotated and displayed
- ⚠️ **HW cores**: Untested (rotation conversion in place)

### All Core Features Implemented

All planned features for OpenGL ES hardware rendering are now implemented.

### Implementation Approach

Simple and direct:

1. Core renders to FBO (1024×1024 RGBA8888)
2. Present function samples rendered portion (e.g., 640×480)
3. Displays with aspect ratio preservation via viewport
4. Uses basic MVP matrix (identity transform mapping 0-1 to NDC)

## Next Steps

### High Priority

1. ~~**Refactor HW Render Integration**~~ ✅ **DONE**
   - Moved from `player_hwrender.*` to `workspace/all/common/gl_video.*`
   - Renamed functions from `PlayerHWRender_*` to `GLVideo_*`
   - Zero `#if HAS_OPENGLES` in player.c (stub implementations in header)
   - Clean separation between HW and SW paths
   - Unified video backend for both software and HW cores

2. ~~**Compare Integration with RetroArch**~~ ✅ **DONE**
   - Verified context setup, timing, and state management matches RetroArch
   - Proper `context_reset` timing (after `retro_load_game` per libretro spec)
   - GL state save/restore for HW cores
   - FBO lifecycle and buffer swapping validated

3. **Debug PPSSPP and Mupen64Plus Crashes**
   - PPSSPP crashes during `context_reset` after logging GPU info (tg5040)
   - Mupen64Plus crashes during runtime on tg5040, works briefly on rg35xxplus
   - Try to get device-level crash dumps (GDB, coredump) for backtrace
   - Compare with working Flycast initialization to find differences
   - May be core-specific bugs rather than our implementation

### Medium Priority

4. ~~**Rotation Support**~~ ✅ **DONE**
   - MVP matrix rotation implemented via `matrix_rotate_z()` and `build_mvp_matrix()`
   - Supports 0°/90°/180°/270° rotation
   - Rotation parameter flows through `GLVideo_present()` → `GLVideo_drawFrame()`

5. **Remove Debug Logging**
   - Clean up LOG_debug calls added for troubleshooting (~62 in gl_video.c)
   - Keep only essential INFO-level logs

6. **Performance Optimization**
   - Profile frame time impact of HW rendering
   - Measure menu presentation overhead

### Low Priority

8. ~~**GLES 3.0 Support**~~ ✅ **DONE**
   - Now supports GLES 2.0, 3.0, 3.1, and 3.2
   - Automatic version negotiation with fallback to lower versions
   - Context type mapping follows RetroArch pattern:
     - `RETRO_HW_CONTEXT_OPENGLES2` → GLES 2.0
     - `RETRO_HW_CONTEXT_OPENGLES3` → GLES 3.0
     - `RETRO_HW_CONTEXT_OPENGLES_VERSION` → uses version_major.version_minor

9. ~~**Shared Context Support**~~ ✅ **DONE**
   - `RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT` - acknowledged

10. ~~**HW Render Interface**~~ ✅ **DONE**
    - `RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE` - returns false (no GLES interface in spec)

## Files Created/Modified

### New Files

- `workspace/all/common/gl_video.h` - Unified GL video backend API (~450 lines, includes stubs for non-GLES platforms)
- `workspace/all/common/gl_video.c` - Complete implementation (~2100 lines)

### Modified Files

- `workspace/all/player/player.c` - Environment callback integration
- `workspace/all/player/Makefile` - Added new source file
- `workspace/all/player/player_loop_vsync.inc` - Added FBO binding before core.run()
- `workspace/all/player/player_loop_audioclock.inc` - Added FBO binding before core.run()
- `workspace/all/common/defines.h` - Added HAS_OPENGLES capability
- `workspace/all/common/render_sdl2.h` - Added SDL2_getWindow()
- `workspace/all/common/render_sdl2.c` - Window with GL support, getter
- `workspace/all/common/api.h` - Added PLAT_getWindow() declaration
- `workspace/rg35xxplus/platform/platform.h` - Enabled HAS_OPENGLES
- `workspace/rg35xxplus/platform/platform.c` - Implemented PLAT_getWindow()
- `workspace/tg5040/platform/platform.h` - Enabled HAS_OPENGLES
- `workspace/tg5040/platform/platform.c` - Implemented PLAT_getWindow()
- `workspace/zero28/platform/platform.h` - Enabled HAS_OPENGLES
- `workspace/zero28/platform/platform.c` - Implemented PLAT_getWindow(), PLAT_getRotation()
- `workspace/my355/platform/platform.h` - Enabled HAS_OPENGLES
- `workspace/my355/platform/platform.c` - Implemented PLAT_getWindow(), PLAT_getRotation(), GLVideo effects
- `tests/support/platform_mocks.c` - Added PLAT_getWindow() stub

## Technical Highlights

### Simple, Direct Implementation

This implementation prioritizes simplicity and correctness:

**MVP Matrix:**

- Orthographic projection: maps vertex coords (0-1) to NDC (-1,1)
- Base matrix: `{2,0,0,0, 0,2,0,0, 0,0,1,0, -1,-1,0,1}`
- Rotation via `build_mvp_matrix()` with `matrix_rotate_z()` for 90°/180°/270°
- Rotation direction conversion: SDL uses CW, OpenGL uses CCW - converted via `(4 - rotation) % 4`

### FBO Configuration

**Texture Format:** RGBA8888

- **Why not RGB565?** Most HW cores (Flycast, PPSSPP) render in 24/32-bit color
- **Memory cost:** 1024×1024×4 = 4MB per FBO (acceptable for target devices)

**Depth/Stencil:**

- Optional based on core requirements
- Uses `GL_DEPTH24_STENCIL8_OES` when both requested (packed format)
- Separate `GL_DEPTH_COMPONENT16` when only depth needed

**Size:**

- Created at 1024×1024 (reasonable default for max_width/max_height)
- Cores typically render at native resolution (640×480 for DC, 480×272 for PSP)
- Texture coordinates scaled to sample only rendered portion

**Texture Coordinate Scaling:**

Core renders to portion of FBO (e.g., 640×480 into 1024×1024). Texture coordinates
are scaled to sample only the rendered region:

```c
float tex_scale_x = (float)width / (float)fbo_width;
float tex_scale_y = (float)height / (float)fbo_height;
// Apply to texcoords: {0,0, tex_scale_x,0, 0,tex_scale_y, tex_scale_x,tex_scale_y}
```

## Known Core Compatibility

| Core                       | tg5040 (PowerVR) | rg35xxplus (Mali) | Requirements         | Notes                                                                   |
| -------------------------- | ---------------- | ----------------- | -------------------- | ----------------------------------------------------------------------- |
| **Flycast** (DC)           | ✅ Working       | ❌ Crashes        | GLES2, depth+stencil | Fully working on PowerVR (scaling, menu, screenshot). Mali: nvmem crash |
| **Mupen64Plus-Next** (N64) | ❌ Crashes       | ⚠️ Partial        | GLES2, depth buffer  | PowerVR: crashes in main loop. Mali: renders frames then crashes        |
| **PPSSPP** (PSP)           | ❌ Crashes       | Untested          | GLES2/GLES3          | Crashes during `context_reset` after GPU detection                      |
| **Beetle PSX HW**          | Untested         | Untested          | GLES2/GLES3, depth   | Should work with GLES2                                                  |
| **ParaLLEl-RDP** (N64)     | Won't work       | Won't work        | GLES3.1+/Vulkan      | Outside GLES2 scope                                                     |

**Key Findings:**

- Different GPUs have different compatibility (PowerVR vs Mali behave differently)
- Flycast is the most stable core for HW rendering on PowerVR
- Core crashes appear to be internal to cores, not our GL setup
- Identical GL setup yields different results per core/GPU combination

## Troubleshooting

### No HW Rendering Requested

**Symptoms:** Core runs in software mode, no "HW render:" log messages

**Possible Causes:**

1. Core doesn't support HW rendering on ARM platforms
2. Core has config option to disable HW rendering
3. Platform doesn't have `HAS_OPENGLES` enabled
4. Core requests unsupported context type (e.g., Vulkan, GLES3)

**Check:**

```bash
grep "RETRO_ENVIRONMENT_SET_HW_RENDER\|HW render:" /path/to/logs/CORE.log
```

### Garbled/Flipped Output

**Symptoms:** Image appears but is distorted, flipped, or wrong size

**Possible Causes:**

1. `bottom_left_origin` handling incorrect
2. Texture coordinate scaling wrong
3. Aspect ratio not preserved
4. Viewport calculation incorrect

**Debug:**
Look for these log messages:

```
GL video: present called (WxH, rotation=N, scale=M, sharp=S)
GL video: viewport(X,Y WxH) src_crop(...)
```

### GL Function Load Failures

**Symptoms:** Crash on init with "failed to load GL function"

**Possible Causes:**

1. SDL2 not built with OpenGL ES support
2. libGLESv2.so not available on device
3. Wrong GL context profile requested

**Fix:**
Ensure SDL2 on device supports OpenGL ES and libraries are installed.

### Core Crashes After context_reset (N64)

**Symptoms:** Mupen64Plus-Next segfaults immediately after "calling core context_reset"

**Log Pattern:**

```
[INFO] HW render: calling core context_reset
[INFO] HW render: initialized successfully
[INFO] mupen64plus: Game controller 0 ...
Segmentation fault
```

**Possible Causes:**

1. Missing GL extension the core expects
2. GL state not matching what core assumes after context_reset
3. Core-specific initialization issue

**Debug Steps:**

1. Check if core works in RetroArch on same device
2. Compare GL context attributes with RetroArch
3. Add logging to track which GL calls core makes after context_reset

### Core Missing Shared Libraries (PSP)

**Symptoms:** PPSSPP fails with "libSM.so.6: cannot open shared object file"

**Cause:** Core was built with desktop X11 dependencies that don't exist on embedded device.

**Fix:** Rebuild PPSSPP core without X11/desktop dependencies. Use RetroArch's buildbot ARM builds or compile with proper cross-toolchain.

## Unified GL Rendering Pipeline ✅ IMPLEMENTED

The unified GL rendering pipeline has been implemented in `workspace/all/common/gl_video.{c,h}`.

### Architecture

**ALL cores render through a unified GL pipeline on GLES platforms:**

```
Software Core → GLVideo_uploadFrame() → GL Texture ─┐
                                                    ├→ GLVideo_present() → Screen
Hardware Core → renders to FBO → GL Texture ────────┘
```

**Key Features:**

- Single `gl_video` module handles both HW and SW cores
- Triple-buffered textures for software cores (`sw_textures[3]`)
- Shared presentation shader for all frame types
- Zero `#if HAS_OPENGLES` in player.c (stub implementations in header)
- Row-by-row upload for GLES2 pitch handling (no `GL_UNPACK_ROW_LENGTH`)

### File Structure

```
workspace/all/common/
├── gl_video.h          # Unified API + stubs for non-GLES platforms
└── gl_video.c          # Complete implementation (~1900 lines)
```

**Note**: Shader pipeline for effects/filters to be added later (see docs/shader-pipeline.md)

## Future Enhancements

**Potential improvements:**

- Shader pipeline for visual effects (see docs/shader-pipeline.md)
- Shared context support (`RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT_ENABLE`)
- Configurable FBO size (currently uses core's max_width/max_height)
- Performance profiling and optimization

**Out of Scope:**

- Vulkan support (different architecture, not supported on target devices)
- Desktop OpenGL (not available on embedded devices)
- Multiple render backends (GL-only simplifies implementation)

## References

- **libretro Specification**: Hardware rendering callbacks and lifecycle
- **RetroArch Implementation**: Reference for correct FBO setup and state management
- **SDL2 Documentation**: OpenGL ES context creation via SDL
- **OpenGL ES 2.0 Specification**: FBO usage, shader language

## Commit Message Template

```
Add OpenGL ES 2.0 hardware rendering support for GPU-accelerated cores

Implements hardware rendering support for libretro cores that require
OpenGL ES (Flycast, PPSSPP, Beetle PSX HW). Enables GPU acceleration
on platforms with Mali GPUs (rg35xxplus, tg5040).

Architecture:
- Unified gl_video module handles GL context and FBO lifecycle
- SDL2-based GL context creation (no raw EGL dependency)
- Dynamic GL function loading via SDL_GL_GetProcAddress
- Shader-based frame presentation with rotation support
- Clean fallback to software rendering on non-GPU platforms

Features:
- GLES 2.0 context with FBO (RGBA8888 + depth/stencil)
- Aspect-preserving viewport with letterboxing
- Rotation support (0°/90°/180°/270° via shader matrix)
- Y-axis flip for bottom_left_origin handling
- Comprehensive debug logging

Testing:
- All 1508 tests pass
- Builds successfully for rg35xxplus and tg5040
- Initial testing shows HW rendering active on tg5040

Resolves: https://github.com/lessui-hq/LessUI/issues/104
```
