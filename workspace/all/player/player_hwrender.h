/**
 * player_hwrender.h - OpenGL ES hardware rendering support for Player
 *
 * This module provides hardware-accelerated rendering support for libretro cores
 * that require OpenGL ES (e.g., Flycast, PPSSPP, Mupen64Plus, Beetle PSX HW).
 *
 * Architecture:
 *   - Creates SDL2 OpenGL ES context when core requests HW rendering
 *   - Manages FBO (framebuffer object) for core to render into
 *   - Provides get_proc_address and get_current_framebuffer callbacks
 *   - Presents HW-rendered frames to screen via GL
 *
 * Usage:
 *   When RETRO_ENVIRONMENT_SET_HW_RENDER is called by a core:
 *   1. PlayerHWRender_init() creates GL context and FBO
 *   2. Frontend provides callbacks to core
 *   3. Core renders to FBO via get_current_framebuffer()
 *   4. PlayerHWRender_present() blits FBO to screen
 *
 * Platform Support:
 *   Only available on platforms with HAS_OPENGLES defined in platform.h.
 *   Currently supports: rg35xxplus, tg5040 (Mali GPU)
 */

#ifndef PLAYER_HWRENDER_H
#define PLAYER_HWRENDER_H

#include "defines.h"

#if HAS_OPENGLES

#include "libretro.h"
#include <stdbool.h>

///////////////////////////////
// Types
///////////////////////////////

/**
 * PlayerHWRenderState - Hardware render state and resources
 *
 * Manages the lifecycle of OpenGL ES context and FBO resources.
 * All GL resources are created lazily when a core requests HW rendering.
 */
typedef struct PlayerHWRenderState {
	// State flags
	bool enabled; // HW rendering is active for current core
	bool context_ready; // GL context is created and ready

	// Core's callback structure (copy of what core provided)
	struct retro_hw_render_callback hw_callback;

	// Actual context version created (may differ from requested if fallback occurred)
	unsigned int context_major;
	unsigned int context_minor;

	// SDL GL context (cast from SDL_GLContext)
	void* gl_context;

	// FBO resources
	unsigned int fbo; // Framebuffer object ID
	unsigned int fbo_texture; // Color attachment texture
	unsigned int fbo_depth_rb; // Depth/stencil renderbuffer (0 if not used)

	// FBO dimensions
	unsigned int fbo_width;
	unsigned int fbo_height;

	// Last rendered frame dimensions (for capture)
	unsigned int last_frame_width;
	unsigned int last_frame_height;

	// Presentation resources
	unsigned int present_program; // Shader program for FBO->screen blit

	// UI surface texture (for menu rendering via GL)
	unsigned int ui_texture;
	unsigned int ui_texture_width;
	unsigned int ui_texture_height;

	// HUD overlay texture (for debug HUD rendering via GL with alpha blending)
	unsigned int hud_texture;
	unsigned int hud_texture_width;
	unsigned int hud_texture_height;

	// Cached shader locations (to avoid glGet* calls per frame)
	int loc_mvp; // u_mvp uniform (4x4 MVP matrix)
	int loc_texture; // u_texture uniform
	int loc_position; // a_position attribute
	int loc_texcoord; // a_texcoord attribute
} PlayerHWRenderState;

///////////////////////////////
// Initialization / Shutdown
///////////////////////////////

/**
 * Initialize hardware rendering from core request.
 *
 * Creates SDL GL context and FBO based on core's retro_hw_render_callback.
 * Sets up get_proc_address and get_current_framebuffer callbacks.
 *
 * @param callback Core's HW render callback (from SET_HW_RENDER)
 * @param max_width Maximum framebuffer width (from av_info.geometry.max_width)
 * @param max_height Maximum framebuffer height (from av_info.geometry.max_height)
 * @return true if HW rendering setup succeeded
 */
bool PlayerHWRender_init(struct retro_hw_render_callback* callback, unsigned max_width,
                         unsigned max_height);

/**
 * Shutdown hardware rendering.
 *
 * Calls core's context_destroy callback, then destroys FBO and GL context.
 * Safe to call even if HW rendering was never initialized.
 */
void PlayerHWRender_shutdown(void);

///////////////////////////////
// State Queries
///////////////////////////////

/**
 * Check if hardware rendering is currently active.
 *
 * @return true if HW rendering is enabled and context is ready
 */
bool PlayerHWRender_isEnabled(void);

/**
 * Check if a context type is supported.
 *
 * @param context_type The retro_hw_context_type requested by core
 * @return true if we can provide this context type
 */
bool PlayerHWRender_isContextSupported(enum retro_hw_context_type context_type);

/**
 * Check if a specific GLES version is supported.
 *
 * Attempts to create a context with the given version to verify support.
 * This is a probe operation - any created context is destroyed afterward.
 *
 * @param major GLES major version (2 or 3)
 * @param minor GLES minor version (0, 1, or 2)
 * @return true if the version is supported
 */
bool PlayerHWRender_isVersionSupported(unsigned major, unsigned minor);

/**
 * Get the actual context version that was created.
 *
 * May differ from requested version if fallback occurred.
 *
 * @param major Output: major version
 * @param minor Output: minor version
 */
void PlayerHWRender_getContextVersion(unsigned* major, unsigned* minor);

///////////////////////////////
// Core Callbacks
///////////////////////////////

/**
 * Get the current framebuffer for core rendering.
 *
 * Called by core via get_current_framebuffer callback.
 * Returns the FBO ID that the core should render into.
 *
 * @return FBO handle, or 0 if not initialized
 */
uintptr_t PlayerHWRender_getCurrentFramebuffer(void);

/**
 * Get OpenGL function pointer.
 *
 * Called by core via get_proc_address callback.
 * Uses SDL_GL_GetProcAddress internally.
 *
 * @param sym Function name (e.g., "glClear", "glBindFramebuffer")
 * @return Function pointer, or NULL if not found
 */
retro_proc_address_t PlayerHWRender_getProcAddress(const char* sym);

///////////////////////////////
// Frame Operations
///////////////////////////////

/**
 * Present the HW-rendered frame to screen.
 *
 * Blits FBO texture to screen using GL, handling rotation and scaling.
 * Called from video_refresh_callback when data == RETRO_HW_FRAME_BUFFER_VALID.
 *
 * @param width Frame width
 * @param height Frame height
 * @param rotation Current rotation (0=0, 1=90 CCW, 2=180, 3=270 CCW)
 * @param scaling_mode Scaling mode (PLAYER_SCALE_NATIVE, ASPECT, FULLSCREEN, CROPPED)
 * @param sharpness Texture filtering (SHARPNESS_SHARP, CRISP, SOFT)
 * @param aspect_ratio Core's reported aspect ratio (0 = use dimensions)
 */
void PlayerHWRender_present(unsigned width, unsigned height, unsigned rotation, int scaling_mode,
                            int sharpness, double aspect_ratio);

/**
 * Resize FBO for new dimensions.
 *
 * Called when core geometry changes (SET_GEOMETRY or SET_SYSTEM_AV_INFO).
 *
 * @param width New framebuffer width
 * @param height New framebuffer height
 * @return true if resize succeeded
 */
bool PlayerHWRender_resizeFBO(unsigned width, unsigned height);

///////////////////////////////
// Context Management
///////////////////////////////

/**
 * Make the GL context current.
 *
 * Called before any GL operations to ensure correct context is active.
 */
void PlayerHWRender_makeCurrent(void);

/**
 * Call core's context_reset callback.
 *
 * Called after GL context and FBO are ready, and after retro_load_game().
 * Signals to core that it can create GL resources.
 */
void PlayerHWRender_contextReset(void);

/**
 * Bind the FBO for core rendering.
 *
 * Should be called before retro_run() so the core renders to our FBO.
 */
void PlayerHWRender_bindFBO(void);

/**
 * Present an SDL surface to screen via GL.
 *
 * Used by menu and debug HUD when HW rendering is active.
 * Uploads the surface to a GL texture and renders it fullscreen.
 * This avoids conflicts between SDL_Renderer and our GL context.
 *
 * @param surface SDL surface to present (must be RGB565 format)
 */
void PlayerHWRender_presentSurface(SDL_Surface* surface);

/**
 * Swap the GL buffers to display the rendered frame.
 *
 * Must be called after PlayerHWRender_present() and any overlay rendering
 * (like PlayerHWRender_renderHUD()) to actually show the frame on screen.
 */
void PlayerHWRender_swapBuffers(void);

/**
 * Render HUD overlay on top of the current frame.
 *
 * Uploads RGBA pixel data to a texture and renders it over the game frame
 * with alpha blending. Should be called after PlayerHWRender_present() but
 * before PlayerHWRender_swapBuffers().
 *
 * The alpha channel is used for transparency: 0 = fully transparent,
 * 255 = fully opaque. This allows the game to show through behind the text.
 *
 * @param pixels RGBA8888 pixel data (4 bytes per pixel: R, G, B, A)
 * @param width Width of the HUD texture in pixels
 * @param height Height of the HUD texture in pixels
 * @param screen_w Target screen width (for positioning)
 * @param screen_h Target screen height (for positioning)
 */
void PlayerHWRender_renderHUD(const uint32_t* pixels, int width, int height, int screen_w,
                              int screen_h);

/**
 * Capture the current frame from the FBO as an SDL surface.
 *
 * Reads pixels from the FBO using glReadPixels and creates an RGB565
 * SDL surface. The Y axis is flipped during conversion (OpenGL origin
 * is bottom-left, SDL is top-left).
 *
 * Used by the in-game menu to capture a screenshot for the background.
 *
 * @return SDL_Surface* in RGB565 format, or NULL if capture failed.
 *         Caller is responsible for freeing the surface with SDL_FreeSurface().
 */
SDL_Surface* PlayerHWRender_captureFrame(void);

#else /* !HAS_OPENGLES */

// Stub implementations for platforms without OpenGL ES support
// These allow the code to compile without #if HAS_OPENGLES everywhere

#include "libretro.h"
#include <stdbool.h>

static inline bool PlayerHWRender_init(struct retro_hw_render_callback* callback,
                                       unsigned max_width, unsigned max_height) {
	(void)callback;
	(void)max_width;
	(void)max_height;
	return false;
}

static inline void PlayerHWRender_shutdown(void) {}

static inline bool PlayerHWRender_isEnabled(void) {
	return false;
}

static inline bool PlayerHWRender_isContextSupported(enum retro_hw_context_type context_type) {
	(void)context_type;
	return false;
}

static inline bool PlayerHWRender_isVersionSupported(unsigned major, unsigned minor) {
	(void)major;
	(void)minor;
	return false;
}

static inline void PlayerHWRender_getContextVersion(unsigned* major, unsigned* minor) {
	if (major)
		*major = 0;
	if (minor)
		*minor = 0;
}

static inline uintptr_t PlayerHWRender_getCurrentFramebuffer(void) {
	return 0;
}

static inline retro_proc_address_t PlayerHWRender_getProcAddress(const char* sym) {
	(void)sym;
	return NULL;
}

static inline void PlayerHWRender_present(unsigned width, unsigned height, unsigned rotation,
                                          int scaling_mode, int sharpness, double aspect_ratio) {
	(void)width;
	(void)height;
	(void)rotation;
	(void)scaling_mode;
	(void)sharpness;
	(void)aspect_ratio;
}

static inline bool PlayerHWRender_resizeFBO(unsigned width, unsigned height) {
	(void)width;
	(void)height;
	return false;
}

static inline void PlayerHWRender_makeCurrent(void) {}

static inline void PlayerHWRender_contextReset(void) {}

static inline void PlayerHWRender_bindFBO(void) {}

static inline void PlayerHWRender_presentSurface(SDL_Surface* surface) {
	(void)surface;
}

static inline void PlayerHWRender_swapBuffers(void) {}

static inline void PlayerHWRender_renderHUD(const uint32_t* pixels, int width, int height,
                                            int screen_w, int screen_h) {
	(void)pixels;
	(void)width;
	(void)height;
	(void)screen_w;
	(void)screen_h;
}

static inline SDL_Surface* PlayerHWRender_captureFrame(void) {
	return NULL;
}

#endif /* HAS_OPENGLES */

#endif /* PLAYER_HWRENDER_H */
