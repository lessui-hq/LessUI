/**
 * show.c - Unified image display utility
 *
 * Displays a PNG image on the screen, used for boot logos and update screens.
 * Supports three rendering backends based on platform capabilities:
 *
 * 1. SDL2 (most platforms) - Hardware-accelerated rendering with auto-rotation
 * 2. SDL1 (legacy platforms) - Software rendering
 * 3. Direct framebuffer (miyoomini) - Zero-copy mmap rendering
 *
 * Usage: show.elf <image.png> [delay_seconds]
 *
 * If image path has no '/', assumes it's in SDCARD_PATH/.system/res/
 * Default delay is 2 seconds.
 *
 * Logging:
 *   Set LOG_FILE environment variable to enable logging to a file.
 *   Errors always go to stderr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"

///////////////////////////////
// Embedded logging (minimal, no dependencies)
///////////////////////////////

static FILE* log_file = NULL;

static void log_init(void) {
	const char* path = getenv("LOG_FILE");
	if (path && *path) {
		log_file = fopen(path, "a");
	}
}

static void log_cleanup(void) {
	if (log_file) {
		fclose(log_file);
		log_file = NULL;
	}
}

#define LOG_error(fmt, ...) do { \
	fprintf(stderr, "[show ERROR] " fmt "\n", ##__VA_ARGS__); \
	if (log_file) { fprintf(log_file, "[show ERROR] " fmt "\n", ##__VA_ARGS__); fflush(log_file); } \
} while(0)

#ifdef ENABLE_DEBUG_LOGS
#define LOG_debug(fmt, ...) do { \
	if (log_file) { fprintf(log_file, "[show DEBUG] " fmt "\n", ##__VA_ARGS__); fflush(log_file); } \
} while(0)
#else
#define LOG_debug(fmt, ...) ((void)0)
#endif

#ifdef ENABLE_INFO_LOGS
#define LOG_info(fmt, ...) do { \
	if (log_file) { fprintf(log_file, "[show INFO] " fmt "\n", ##__VA_ARGS__); fflush(log_file); } \
} while(0)
#else
#define LOG_info(fmt, ...) ((void)0)
#endif

///////////////////////////////
// SDL headers based on platform
///////////////////////////////

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#endif

// Miyoo Mini uses direct framebuffer access
#ifdef PLATFORM_MIYOOMINI
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

///////////////////////////////
// Pixel format constants
///////////////////////////////

#define FIXED_BPP 2
#define FIXED_DEPTH (FIXED_BPP * 8)
#define RGBA_MASK_565 0xF800, 0x07E0, 0x001F, 0x0000

///////////////////////////////
// Path resolution
///////////////////////////////

/**
 * Resolve image path.
 * If SHOW_NEEDS_PATH_RESOLUTION is defined and path has no '/',
 * assume it's in SDCARD_PATH/.system/res/
 */
static void resolve_path(char* out, const char* in) {
#if defined(SHOW_NEEDS_PATH_RESOLUTION) || defined(PLATFORM_MIYOOMINI)
	if (strchr(in, '/') == NULL) {
		sprintf(out, "%s/.system/res/%s", SDCARD_PATH, in);
		LOG_debug("Resolved path '%s' to '%s'", in, out);
	} else {
		strncpy(out, in, 255);
		out[255] = '\0';
	}
#else
	strncpy(out, in, 255);
	out[255] = '\0';
#endif
}

///////////////////////////////
// Backend: Miyoo Mini (direct framebuffer)
///////////////////////////////

#ifdef PLATFORM_MIYOOMINI
/**
 * Miyoo Mini: Direct framebuffer rendering
 *
 * Uses mmap to write directly to /dev/fb0. Forces 640x480 mode.
 * Images are pre-converted to 24-bit BGR format, we convert to RGBA.
 *
 * Returns: 0 on success, -1 on failure
 */
static int show_image(const char* path) {
	LOG_debug("show_miyoomini: opening framebuffer");
	int fb0_fd = open("/dev/fb0", O_RDWR);
	if (fb0_fd < 0) {
		LOG_error("Failed to open /dev/fb0");
		return -1;
	}

	struct fb_var_screeninfo vinfo;
	ioctl(fb0_fd, FBIOGET_VSCREENINFO, &vinfo);
	LOG_debug("Framebuffer info: %dx%d, %d bpp, yoffset=%d",
	          vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, vinfo.yoffset);

	// Force to 640x480 (miyoomini standard resolution)
	vinfo.xres = vinfo.xres_virtual = 640;
	vinfo.yres = 480;
	vinfo.yres_virtual = 1440;
	vinfo.yoffset = 0;
	vinfo.activate = FB_ACTIVATE_NOW;
	ioctl(fb0_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb0_fd, FBIOGET_VSCREENINFO, &vinfo);
	LOG_debug("Set framebuffer to %dx%d, yoffset=%d", vinfo.xres, vinfo.yres, vinfo.yoffset);

	// Map first page only
	int map_size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
	char* fb0_map = (char*)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd, 0);
	if (fb0_map == MAP_FAILED) {
		LOG_error("mmap failed for framebuffer");
		close(fb0_fd);
		return -1;
	}
	LOG_debug("Mapped framebuffer at %p, size %d", fb0_map, map_size);

	// Clear screen
	memset(fb0_map, 0, map_size);

	// Load image (24-bit opaque png)
	LOG_debug("Loading image: %s", path);
	SDL_Surface* img = IMG_Load(path);
	if (img == NULL) {
		LOG_error("IMG_Load failed: %s", IMG_GetError());
		munmap(fb0_map, map_size);
		close(fb0_fd);
		return -1;
	}
	LOG_debug("Image loaded: %dx%d", img->w, img->h);

	// Convert BGR to RGBA, starting from end of image (original backwards loop)
	uint8_t* dst = (uint8_t*)fb0_map;
	uint8_t* src = (uint8_t*)img->pixels;
	src += ((img->h * img->w) - 1) * 3;

	for (int y = 0; y < img->h; y++) {
		for (int x = 0; x < img->w; x++) {
			*(dst + 0) = *(src + 2); // r
			*(dst + 1) = *(src + 1); // g
			*(dst + 2) = *(src + 0); // b
			*(dst + 3) = 0xf;        // alpha
			dst += 4;
			src -= 3;
		}
	}

	SDL_FreeSurface(img);
	LOG_info("Image displayed successfully via framebuffer");

	// Note: We intentionally do NOT call munmap/close here.
	// The framebuffer stays mapped during the sleep in main().
	// Cleanup happens automatically when the process exits.
	// This matches the original behavior where sleep happened before cleanup.
	(void)fb0_fd;    // Suppress unused warning - closed on exit
	(void)fb0_map;   // Suppress unused warning - unmapped on exit
	(void)map_size;  // Suppress unused warning
	return 0;
}

///////////////////////////////
// Backend: SDL2 (hardware-accelerated)
///////////////////////////////

#elif defined(USE_SDL2)
/**
 * SDL2 platforms: Hardware-accelerated rendering
 *
 * Creates SDL2 window/renderer, auto-detects portrait mode and rotates.
 * Portrait displays are rotated to show landscape images correctly.
 *
 * Returns: 0 on success, -1 on failure
 */
static int show_image(const char* path) {
	LOG_debug("show_sdl2: initializing SDL");
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_error("SDL_Init failed: %s", SDL_GetError());
		return -1;
	}
	SDL_ShowCursor(0);

	// Create window - auto-sizes to display
	LOG_debug("Creating window");
	SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                                      0, 0, SDL_WINDOW_SHOWN);
	if (window == NULL) {
		LOG_error("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	// Get display dimensions and detect portrait mode
	int rotate = 0;
	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(0, &mode) < 0) {
		LOG_error("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}
	LOG_debug("Display mode: %dx%d", mode.w, mode.h);

	if (mode.h > mode.w) {
#ifdef SHOW_ROTATION_CLOCKWISE
		rotate = 1;  // 90 degrees CW (zero28)
		LOG_debug("Portrait mode detected, rotating 90 CW");
#else
		rotate = 3;  // 270 degrees CCW (most platforms)
		LOG_debug("Portrait mode detected, rotating 270 CCW");
#endif
	}

	int w = mode.w;
	int h = mode.h;
	int p = w * FIXED_BPP;

	// Create renderer
	LOG_debug("Creating renderer");
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
	                                            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL) {
		LOG_error("SDL_CreateRenderer failed: %s", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	// Create texture for rendering
	LOG_debug("Creating texture %dx%d", w, h);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
	                                         SDL_TEXTUREACCESS_STREAMING, w, h);
	if (texture == NULL) {
		LOG_error("SDL_CreateTexture failed: %s", SDL_GetError());
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	// Create surface for blitting
	SDL_Surface* screen = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	if (screen == NULL) {
		LOG_error("SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	// Lock texture and blit image to it
	SDL_LockTexture(texture, NULL, &screen->pixels, &screen->pitch);
	SDL_FillRect(screen, NULL, 0);

	LOG_debug("Loading image: %s", path);
	SDL_Surface* img = IMG_Load(path);
	if (img == NULL) {
		LOG_error("IMG_Load failed: %s", IMG_GetError());
		SDL_UnlockTexture(texture);
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}
	LOG_debug("Image loaded: %dx%d", img->w, img->h);

	// Center image on screen (same as original implementations)
	int blit_x = (screen->w - img->w) / 2;
	int blit_y = (screen->h - img->h) / 2;
	LOG_debug("Centering at: (%d,%d)", blit_x, blit_y);
	SDL_BlitSurface(img, NULL, screen, &(SDL_Rect){blit_x, blit_y, 0, 0});
	SDL_FreeSurface(img);
	SDL_UnlockTexture(texture);

	// Render with rotation if needed
	if (rotate) {
#ifdef SHOW_ROTATION_CLOCKWISE
		// CW rotation (zero28): dest rect at {h, 0, w, h}
		SDL_RenderCopyEx(renderer, texture, NULL, &(SDL_Rect){h, 0, w, h},
		                 rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
#else
		// CCW rotation (most platforms): dest rect at {0, w, w, h}
		SDL_RenderCopyEx(renderer, texture, NULL, &(SDL_Rect){0, w, w, h},
		                 rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
#endif
	} else {
		SDL_RenderCopy(renderer, texture, NULL, NULL);
	}
	SDL_RenderPresent(renderer);

	LOG_info("Image displayed successfully via SDL2 (rotate=%d)", rotate);

	// Cleanup
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

///////////////////////////////
// Backend: SDL1 (software rendering)
///////////////////////////////

#else
/**
 * SDL1 platforms: Software rendering
 *
 * Used by legacy platforms like trimuismart.
 * Rotation handled via SDL_VIDEO_FBCON_ROTATION env var.
 *
 * Returns: 0 on success, -1 on failure
 */
static int show_image(const char* path) {
	// Enable rotation for portrait displays (trimuismart)
	putenv("SDL_VIDEO_FBCON_ROTATION=CCW");

	LOG_debug("show_sdl1: initializing SDL");
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		LOG_error("SDL_Init failed: %s", SDL_GetError());
		return -1;
	}
	SDL_ShowCursor(0);

	// Set video mode (320x240 for trimuismart)
	LOG_debug("Setting video mode 320x240");
	SDL_Surface* screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
	if (screen == NULL) {
		LOG_error("SDL_SetVideoMode failed: %s", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	// Load image
	LOG_debug("Loading image: %s", path);
	SDL_Surface* img = IMG_Load(path);
	if (img == NULL) {
		LOG_error("IMG_Load failed: %s", IMG_GetError());
		SDL_Quit();
		return -1;
	}
	LOG_debug("Image loaded: %dx%d", img->w, img->h);

	// Center and blit image
	SDL_Rect dest = {(screen->w - img->w) / 2, (screen->h - img->h) / 2, 0, 0};
	LOG_debug("Centering at: %d,%d", dest.x, dest.y);
	SDL_BlitSurface(img, NULL, screen, &dest);
	SDL_Flip(screen);

	SDL_FreeSurface(img);
	LOG_info("Image displayed successfully via SDL1");

	// Note: We do NOT call SDL_Quit() or IMG_Quit() here.
	// The image stays on screen until the process exits (after sleep in main).
	return 0;
}
#endif

///////////////////////////////
// Main entry point
///////////////////////////////

int main(int argc, char* argv[]) {
	log_init();
	LOG_info("show.elf starting on %s", PLATFORM);

	if (argc < 2) {
		puts("Usage: show.elf image.png [delay]");
		log_cleanup();
		return EXIT_SUCCESS;
	}

	// Resolve path
	char path[256];
	resolve_path(path, argv[1]);
	LOG_debug("Image path: %s", path);

	// Check if file exists (silently exit if not - not an error)
	if (access(path, F_OK) != 0) {
		LOG_debug("Image not found, exiting silently: %s", path);
		log_cleanup();
		return EXIT_SUCCESS;
	}

	// Parse delay (default 2 seconds)
	int delay = argc > 2 ? atoi(argv[2]) : 2;
	LOG_debug("Delay: %d seconds", delay);

	// Display the image
	int result = show_image(path);
	if (result < 0) {
		LOG_info("show.elf exiting with failure");
		log_cleanup();
		return EXIT_FAILURE;
	}

	// Sleep to keep image visible (unified for all backends)
	LOG_debug("Sleeping for %d seconds...", delay);
	sleep(delay);
	LOG_debug("Sleep completed");

	LOG_info("show.elf exiting successfully");
	log_cleanup();
	return EXIT_SUCCESS;
}
