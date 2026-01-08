/**
 * gl_video.h - OpenGL ES rendering backend
 *
 * This module provides hardware-accelerated rendering support for libretro cores
 * that require OpenGL ES (e.g., Flycast, PPSSPP, Mupen64Plus, Beetle PSX HW).
 *
 * It will eventually be expanded to handle ALL video rendering (software cores
 * uploading to GL textures), unifying the video pipeline.
 *
 * Architecture:
 *   - Creates SDL2 OpenGL ES context when core requests HW rendering
 *   - Manages FBO (framebuffer object) for core to render into
 *   - Provides get_proc_address and get_current_framebuffer callbacks
 *   - Presents HW-rendered frames to screen via GL
 *
 * Usage:
 *   When RETRO_ENVIRONMENT_SET_HW_RENDER is called by a core:
 *   1. GLVideo_init() creates GL context and FBO
 *   2. Frontend provides callbacks to core
 *   3. Core renders to FBO via get_current_framebuffer()
 *   4. GLVideo_present() blits FBO to screen
 *
 * Platform Support:
 *   Only available on platforms with HAS_OPENGLES defined in platform.h.
 *   Currently supports: rg35xxplus, tg5040 (Mali GPU)
 */

#ifndef GL_VIDEO_H
#define GL_VIDEO_H

#include "defines.h"

#if HAS_OPENGLES

#include "sdl.h"
#include <stdbool.h>

// Forward declarations to avoid including libretro.h in non-player components
struct retro_hw_render_callback;
typedef void (*retro_proc_address_t)(void);

// Pixel formats (compatible with libretro)
#define GL_VIDEO_PIXEL_FORMAT_0RGB1555 0
#define GL_VIDEO_PIXEL_FORMAT_XRGB8888 1
#define GL_VIDEO_PIXEL_FORMAT_RGB565 2

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
bool GLVideo_init(struct retro_hw_render_callback* callback, unsigned max_width,
                  unsigned max_height);

/**
 * Initialize GL context for software rendering.
 *
 * Creates SDL GL context and presentation resources without FBO.
 * Used by render_sdl2 on platforms that support GL but use software cores.
 *
 * @return true if GL setup succeeded
 */
bool GLVideo_initSoftware(void);

/**
 * Prepare for shutdown by calling context_destroy.
 *
 * Notifies the core that the GL context is about to be destroyed, but keeps
 * the GL context alive. This allows the core's dlclose() destructors to still
 * use GL functions if needed. Call GLVideo_shutdown() later to actually
 * destroy the GL context.
 *
 * Safe to call multiple times or even if HW rendering was never initialized.
 */
void GLVideo_prepareShutdown(void);

/**
 * Shutdown hardware rendering.
 *
 * Calls core's context_destroy callback (if not already called via
 * GLVideo_prepareShutdown), then destroys FBO and GL context.
 * Safe to call even if HW rendering was never initialized.
 */
void GLVideo_shutdown(void);

///////////////////////////////
// State Queries
///////////////////////////////

/**
 * Check if hardware rendering is currently active.
 *
 * @return true if HW rendering is enabled and context is ready
 */
bool GLVideo_isEnabled(void);

/**
 * Check if a context type is supported.
 *
 * @param context_type The retro_hw_context_type requested by core (as int)
 * @return true if we can provide this context type
 */
bool GLVideo_isContextSupported(int context_type);

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
bool GLVideo_isVersionSupported(unsigned major, unsigned minor);

/**
 * Get the actual context version that was created.
 *
 * May differ from requested version if fallback occurred.
 *
 * @param major Output: major version
 * @param minor Output: minor version
 */
void GLVideo_getContextVersion(unsigned* major, unsigned* minor);

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
uintptr_t GLVideo_getCurrentFramebuffer(void);

/**
 * Get OpenGL function pointer.
 *
 * Called by core via get_proc_address callback.
 * Uses SDL_GL_GetProcAddress internally.
 *
 * @param sym Function name (e.g., "glClear", "glBindFramebuffer")
 * @return Function pointer, or NULL if not found
 */
retro_proc_address_t GLVideo_getProcAddress(const char* sym);

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
 * @param visual_scale Content-to-screen scale factor for effects
 */
void GLVideo_present(unsigned width, unsigned height, unsigned rotation, int scaling_mode,
                     int sharpness, double aspect_ratio, int visual_scale);

/**
 * Resize FBO for new dimensions.
 *
 * Called when core geometry changes (SET_GEOMETRY or SET_SYSTEM_AV_INFO).
 *
 * @param width New framebuffer width
 * @param height New framebuffer height
 * @return true if resize succeeded
 */
bool GLVideo_resizeFBO(unsigned width, unsigned height);

///////////////////////////////
// Context Management
///////////////////////////////

/**
 * Make the GL context current.
 *
 * Called before any GL operations to ensure correct context is active.
 */
void GLVideo_makeCurrent(void);

/**
 * Call core's context_reset callback.
 *
 * Called after GL context and FBO are ready, and after retro_load_game().
 * Signals to core that it can create GL resources.
 */
void GLVideo_contextReset(void);

/**
 * Bind the FBO for core rendering.
 *
 * Should be called before retro_run() so the core renders to our FBO.
 */
void GLVideo_bindFBO(void);

/**
 * Upload a software-rendered frame to a GL texture.
 *
 * Used for cores that render to a system memory buffer (RGB565).
 * Uploads the data to one of the triple-buffered textures.
 *
 * @param data Pixel data pointer (usually RGB565)
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param pitch Bytes per row in data buffer
 * @param pixel_format Libretro pixel format (RETRO_PIXEL_FORMAT_RGB565, etc.)
 */
void GLVideo_uploadFrame(const void* data, unsigned width, unsigned height, size_t pitch,
                         unsigned pixel_format);

/**
 * Draw a texture to the screen using GL.
 *
 * Low-level frame presentation function used by both HW and SW paths.
 *
 * @param texture_id GL texture ID to draw
 * @param tex_w Texture width in pixels (for coordinate normalization)
 * @param tex_h Texture height in pixels
 * @param src_rect Source rectangle in pixels
 * @param dst_rect Destination rectangle (viewport) in pixels
 * @param rotation Rotation (0-3)
 * @param sharpness Sharpness mode
 * @param bottom_left_origin true if texture origin is bottom-left (no flip needed)
 */
void GLVideo_drawFrame(unsigned int texture_id, unsigned int tex_w, unsigned int tex_h,
                       const SDL_Rect* src_rect, const SDL_Rect* dst_rect, unsigned rotation,
                       int sharpness, bool bottom_left_origin);

/**
 * Draw the current software frame to the screen.
 *
 * Convenience wrapper for GLVideo_drawFrame that uses the current software texture.
 * Also handles effect overlay rendering for software cores.
 *
 * @param src_rect Source rectangle in pixels
 * @param dst_rect Destination rectangle (viewport) in pixels
 * @param rotation Rotation (0-3)
 * @param sharpness Sharpness mode
 * @param visual_scale Content-to-screen scale factor for effects
 */
void GLVideo_drawSoftwareFrame(const SDL_Rect* src_rect, const SDL_Rect* dst_rect,
                               unsigned rotation, int sharpness, int visual_scale);

/**
 * Present an SDL surface to screen via GL.
 *
 * Used by menu and debug HUD when HW rendering is active.
 * Uploads the surface to a GL texture and renders it fullscreen.
 * This avoids conflicts between SDL_Renderer and our GL context.
 *
 * @param surface SDL surface to present (must be RGB565 format)
 */
void GLVideo_presentSurface(SDL_Surface* surface);

/**
 * Swap the GL buffers to display the rendered frame.
 *
 * Must be called after GLVideo_present() and any overlay rendering
 * (like GLVideo_renderHUD()) to actually show the frame on screen.
 */
void GLVideo_swapBuffers(void);

/**
 * Set vsync mode for the GL context.
 *
 * In audio-clock mode, vsync should be disabled so audio blocking
 * is the sole timing source. In vsync mode, vsync should be enabled
 * for tear-free rendering.
 *
 * @param enabled 1 to enable vsync, 0 to disable
 * @return 0 on success, -1 on failure
 */
int GLVideo_setVsync(int enabled);

/**
 * Clear the screen to black.
 *
 * Used by software rendering path to ensure backbuffer is clean before drawing.
 */
void GLVideo_clear(void);

/**
 * Render HUD overlay on top of the current frame.
 *
 * Uploads RGBA pixel data to a texture and renders it over the game frame
 * with alpha blending. Should be called after GLVideo_present() but
 * before GLVideo_swapBuffers().
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
void GLVideo_renderHUD(const uint32_t* pixels, int width, int height, int screen_w, int screen_h);

/**
 * Sets effect type for next frame.
 *
 * @param type Effect type (EFFECT_NONE, EFFECT_LINE, EFFECT_GRID, etc.)
 */
void GLVideo_setEffect(int type);

/**
 * Sets effect color (for DMG grid colorization).
 *
 * @param color RGB565 color value
 */
void GLVideo_setEffectColor(int color);

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
SDL_Surface* GLVideo_captureFrame(void);

#else /* !HAS_OPENGLES */

// Stub implementations for platforms without OpenGL ES support
// These allow the code to compile without #if HAS_OPENGLES everywhere

#include "libretro.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations for SDL types (avoid including SDL.h which may not exist on SDL1 platforms)
struct SDL_Surface;
typedef struct SDL_Surface SDL_Surface;
struct SDL_Rect;
typedef struct SDL_Rect SDL_Rect;

static inline bool GLVideo_init(struct retro_hw_render_callback* callback, unsigned max_width,
                                unsigned max_height) {
	(void)callback;
	(void)max_width;
	(void)max_height;
	return false;
}

static inline bool GLVideo_initSoftware(void) {
	return false;
}

static inline void GLVideo_prepareShutdown(void) {}

static inline void GLVideo_shutdown(void) {}

static inline bool GLVideo_isEnabled(void) {
	return false;
}

static inline bool GLVideo_isContextSupported(int context_type) {
	(void)context_type;
	return false;
}

static inline bool GLVideo_isVersionSupported(unsigned major, unsigned minor) {
	(void)major;
	(void)minor;
	return false;
}

static inline void GLVideo_getContextVersion(unsigned* major, unsigned* minor) {
	if (major)
		*major = 0;
	if (minor)
		*minor = 0;
}

static inline uintptr_t GLVideo_getCurrentFramebuffer(void) {
	return 0;
}

static inline retro_proc_address_t GLVideo_getProcAddress(const char* sym) {
	(void)sym;
	return NULL;
}

static inline void GLVideo_present(unsigned width, unsigned height, unsigned rotation,
                                   int scaling_mode, int sharpness, double aspect_ratio,
                                   int visual_scale) {
	(void)width;
	(void)height;
	(void)rotation;
	(void)scaling_mode;
	(void)sharpness;
	(void)aspect_ratio;
	(void)visual_scale;
}

static inline bool GLVideo_resizeFBO(unsigned width, unsigned height) {
	(void)width;
	(void)height;
	return false;
}

static inline void GLVideo_makeCurrent(void) {}

static inline void GLVideo_contextReset(void) {}

static inline void GLVideo_bindFBO(void) {}

static inline void GLVideo_uploadFrame(const void* data, unsigned width, unsigned height,
                                       size_t pitch, unsigned pixel_format) {
	(void)data;
	(void)width;
	(void)height;
	(void)pitch;
	(void)pixel_format;
}

static inline void GLVideo_drawFrame(unsigned int texture_id, unsigned int tex_w,
                                     unsigned int tex_h, const SDL_Rect* src_rect,
                                     const SDL_Rect* dst_rect, unsigned rotation, int sharpness,
                                     bool bottom_left_origin) {
	(void)texture_id;
	(void)tex_w;
	(void)tex_h;
	(void)src_rect;
	(void)dst_rect;
	(void)rotation;
	(void)sharpness;
	(void)bottom_left_origin;
}

static inline void GLVideo_drawSoftwareFrame(const SDL_Rect* src_rect, const SDL_Rect* dst_rect,
                                             unsigned rotation, int sharpness, int visual_scale) {
	(void)src_rect;
	(void)dst_rect;
	(void)rotation;
	(void)sharpness;
	(void)visual_scale;
}

static inline void GLVideo_presentSurface(SDL_Surface* surface) {
	(void)surface;
}

static inline void GLVideo_swapBuffers(void) {}

static inline int GLVideo_setVsync(int enabled) {
	(void)enabled;
	return -1;
}

static inline void GLVideo_clear(void) {}

static inline void GLVideo_renderHUD(const uint32_t* pixels, int width, int height, int screen_w,
                                     int screen_h) {
	(void)pixels;
	(void)width;
	(void)height;
	(void)screen_w;
	(void)screen_h;
}

static inline void GLVideo_setEffect(int type) {
	(void)type;
}

static inline void GLVideo_setEffectColor(int color) {
	(void)color;
}

static inline SDL_Surface* GLVideo_captureFrame(void) {
	return NULL;
}

#endif /* HAS_OPENGLES */

#endif /* GL_VIDEO_H */
