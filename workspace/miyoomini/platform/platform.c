/**
 * platform.c - Miyoo Mini platform implementation
 *
 * REFACTORED VERSION - Uses shared effect_system for effect state management
 *
 * Provides hardware-specific implementations for the Miyoo Mini family of devices.
 * This is one of the most complex platform implementations in MinUI, featuring:
 *
 * - Hardware-accelerated blitting via MI_GFX API (zero-copy rendering)
 * - ION memory allocator for physically contiguous buffers
 * - Lid sensor support (Hall effect sensor)
 * - AXP223 power management IC (Plus model)
 * - Hardware variant detection (Mini vs Plus, 480p vs 560p)
 *
 * Supported Devices:
 * - Miyoo Mini (original) - 640x480, GPIO battery monitoring
 * - Miyoo Mini Plus - 640x480, AXP223 PMIC, improved hardware
 * - Miyoo Mini Plus (560p variant) - 752x560 resolution option
 *
 * @note Based on eggs' GFXSample_rev15 implementation
 */

#include <linux/fb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <msettings.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include "api.h"
#include "defines.h"
#include "effect_surface.h"
#include "effect_system.h"
#include "platform.h"
#include "scaler.h"
#include "utils.h"

///////////////////////////////
// MI_GFX Hardware Blitting API
///////////////////////////////

#include <mi_gfx.h>
#include <mi_sys.h>

///////////////////////////////
// Device Registry and Variant Configuration
///////////////////////////////

// Device registry - all known devices that work with this platform
static const DeviceInfo miyoomini_devices[] = {
    // Standard Miyoo Mini
    {.device_id = "miyoomini", .display_name = "Mini", .manufacturer = "Miyoo"},

    // Miyoo Mini Plus (640x480)
    {.device_id = "miyoominiplus", .display_name = "Mini Plus", .manufacturer = "Miyoo"},

    // Miyoo Mini Plus (560p variant)
    {.device_id = "miyoominiplus560p", .display_name = "Mini Plus (560p)", .manufacturer = "Miyoo"},

    // Miyoo Mini Flip (MY285 - same as standard but clamshell form factor)
    {.device_id = "miyoominiflip", .display_name = "Mini Flip", .manufacturer = "Miyoo"},

    // Sentinel
    {NULL, NULL, NULL}};

// Variant configuration table
typedef struct {
	VariantType variant;
	int screen_width;
	int screen_height;
	float screen_diagonal_default;
	uint32_t hw_features;
} VariantConfig;

static const VariantConfig miyoomini_variants[] = {
    {.variant = VARIANT_MINI_STANDARD,
     .screen_width = 640,
     .screen_height = 480,
     .screen_diagonal_default = 2.8f,
     .hw_features = HW_FEATURE_NEON},
    {.variant = VARIANT_MINI_PLUS,
     .screen_width = 640,
     .screen_height = 480,
     .screen_diagonal_default = 2.8f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_PMIC | HW_FEATURE_VOLUME_HW},
    {.variant = VARIANT_MINI_PLUS_560P,
     .screen_width = 752,
     .screen_height = 560,
     .screen_diagonal_default = 2.8f,
     .hw_features = HW_FEATURE_NEON | HW_FEATURE_PMIC | HW_FEATURE_VOLUME_HW},
    {.variant = VARIANT_NONE} // Sentinel
};

// Device-to-variant mapping
typedef struct {
	int has_pmic; // AXP223 PMIC detected
	int has_560p; // 560p screen mode available
	VariantType variant;
	const DeviceInfo* device;
} DeviceVariantMap;

static const DeviceVariantMap miyoomini_device_map[] = {
    {0, 0, VARIANT_MINI_STANDARD, &miyoomini_devices[0]}, // Standard Mini
    {1, 0, VARIANT_MINI_PLUS, &miyoomini_devices[1]}, // Plus (480p)
    {1, 1, VARIANT_MINI_PLUS_560P, &miyoomini_devices[2]}, // Plus (560p)
    {-1, -1, VARIANT_NONE, NULL} // Sentinel
};

static const VariantConfig* getVariantConfig(VariantType variant) {
	for (int i = 0; miyoomini_variants[i].variant != VARIANT_NONE; i++) {
		if (miyoomini_variants[i].variant == variant)
			return &miyoomini_variants[i];
	}
	return NULL;
}

static int hasMode(const char* path, const char* mode) {
	FILE* f = fopen(path, "r");
	if (!f)
		return 0;
	char s[128];
	while (fgets(s, sizeof s, f))
		if (strstr(s, mode))
			return fclose(f), 1;
	fclose(f);
	return 0;
}

void PLAT_detectVariant(PlatformVariant* v) {
	v->platform = PLATFORM;
	v->has_hdmi = 0;

	// Check for Miyoo Mini Flip (MY285) - NOTE: This is NOT the RK3566 Miyoo Flip!
	// MY285 is a clamshell variant of the original Miyoo Mini (R16/SSD202D)
	char* model = getenv("MY_MODEL");
	if (exactMatch(model, "MY285")) {
		// Flip uses standard Mini variant but different screen size
		v->device = &miyoomini_devices[3]; // Mini Flip
		v->variant = VARIANT_MINI_STANDARD;
		const VariantConfig* config = getVariantConfig(v->variant);
		if (config) {
			v->screen_width = config->screen_width;
			v->screen_height = config->screen_height;
			v->screen_diagonal = 3.5f; // Mini Flip has 3.5" screen (vs 2.8" Mini)
			v->hw_features = config->hw_features;
		}
		LOG_info("Detected device: %s %s (%dx%d, %.1f\")\n", v->device->manufacturer,
		         v->device->display_name, v->screen_width, v->screen_height, v->screen_diagonal);
		return;
	}

	// Detect Plus variant via PMIC presence
	int has_pmic = exists("/customer/app/axp_test");

	// Detect 560p screen mode
	int has_560p = hasMode("/sys/class/graphics/fb0/modes", "752x560p");

	// Look up device in mapping table
	const DeviceVariantMap* map = NULL;
	for (int i = 0; miyoomini_device_map[i].variant != VARIANT_NONE; i++) {
		if (miyoomini_device_map[i].has_pmic == has_pmic &&
		    miyoomini_device_map[i].has_560p == has_560p) {
			map = &miyoomini_device_map[i];
			break;
		}
	}

	// Fallback to standard if not found
	if (!map) {
		LOG_warn("Unknown device configuration (PMIC=%d, 560p=%d), defaulting to Miyoo Mini\n",
		         has_pmic, has_560p);
		map = &miyoomini_device_map[0];
	}

	// Set device info
	v->device = map->device;
	v->variant = map->variant;

	// Apply variant configuration
	const VariantConfig* config = getVariantConfig(map->variant);
	if (config) {
		v->screen_width = config->screen_width;
		v->screen_height = config->screen_height;
		v->screen_diagonal = config->screen_diagonal_default;
		v->hw_features = config->hw_features;
	}

	LOG_info("Detected device: %s %s (%dx%d, %.1f\")\n", v->device->manufacturer,
	         v->device->display_name, v->screen_width, v->screen_height, v->screen_diagonal);
}

// SDL surface extension: stores physical address for MI_GFX
#define pixelsPa unused1

// Align value to 4KB boundary for cache operations
#define ALIGN4K(val) ((val + 4095) & (~4095))

/**
 * Determines MI_GFX color format from SDL surface pixel format.
 */
static inline MI_GFX_ColorFmt_e GFX_ColorFmt(SDL_Surface* surface) {
	if (surface) {
		if (surface->format->BytesPerPixel == 2) {
			if (surface->format->Amask == 0x0000)
				return E_MI_GFX_FMT_RGB565;
			if (surface->format->Amask == 0x8000)
				return E_MI_GFX_FMT_ARGB1555;
			if (surface->format->Amask == 0xF000)
				return E_MI_GFX_FMT_ARGB4444;
			if (surface->format->Amask == 0x0001)
				return E_MI_GFX_FMT_RGBA5551;
			if (surface->format->Amask == 0x000F)
				return E_MI_GFX_FMT_RGBA4444;
			return E_MI_GFX_FMT_RGB565;
		}
		if (surface->format->Bmask == 0x000000FF)
			return E_MI_GFX_FMT_ARGB8888;
		if (surface->format->Rmask == 0x000000FF)
			return E_MI_GFX_FMT_ABGR8888;
	}
	return E_MI_GFX_FMT_ARGB8888;
}

/**
 * Flushes CPU cache for a rectangular region of a surface.
 */
static inline void FlushCacheNeeded(void* pixels, uint32_t pitch, uint32_t y, uint32_t h) {
	uintptr_t pixptr = (uintptr_t)pixels;
	uintptr_t startaddress = (pixptr + pitch * y) & (~4095);
	uint32_t size = ALIGN4K(pixptr + pitch * (y + h)) - startaddress;
	if (size)
		MI_SYS_FlushInvCache((void*)startaddress, size);
}

/**
 * Hardware-accelerated blit using MI_GFX (replaces SDL_BlitSurface).
 */
static inline void GFX_BlitSurfaceExec(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst,
                                       SDL_Rect* dstrect, uint32_t rotate, uint32_t mirror,
                                       uint32_t nowait) {
	if ((src) && (dst) && (src->pixelsPa) && (dst->pixelsPa)) {
		MI_GFX_Surface_t Src;
		MI_GFX_Surface_t Dst;
		MI_GFX_Rect_t SrcRect;
		MI_GFX_Rect_t DstRect;
		MI_GFX_Opt_t Opt;
		MI_U16 Fence;

		Src.phyAddr = src->pixelsPa;
		Src.u32Width = src->w;
		Src.u32Height = src->h;
		Src.u32Stride = src->pitch;
		Src.eColorFmt = GFX_ColorFmt(src);
		if (srcrect) {
			SrcRect.s32Xpos = srcrect->x;
			SrcRect.s32Ypos = srcrect->y;
			SrcRect.u32Width = srcrect->w;
			SrcRect.u32Height = srcrect->h;
		} else {
			SrcRect.s32Xpos = 0;
			SrcRect.s32Ypos = 0;
			SrcRect.u32Width = Src.u32Width;
			SrcRect.u32Height = Src.u32Height;
		}
		FlushCacheNeeded(src->pixels, src->pitch, SrcRect.s32Ypos, SrcRect.u32Height);

		Dst.phyAddr = dst->pixelsPa;
		Dst.u32Width = dst->w;
		Dst.u32Height = dst->h;
		Dst.u32Stride = dst->pitch;
		Dst.eColorFmt = GFX_ColorFmt(dst);
		if (dstrect) {
			DstRect.s32Xpos = dstrect->x;
			DstRect.s32Ypos = dstrect->y;
			if (dstrect->w | dstrect->h) {
				DstRect.u32Width = dstrect->w;
				DstRect.u32Height = dstrect->h;
			} else {
				DstRect.u32Width = SrcRect.u32Width;
				DstRect.u32Height = SrcRect.u32Height;
			}
		} else {
			DstRect.s32Xpos = 0;
			DstRect.s32Ypos = 0;
			DstRect.u32Width = Dst.u32Width;
			DstRect.u32Height = Dst.u32Height;
		}
		if (rotate & 1)
			FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Width);
		else
			FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Height);

		// Configure blending options
		memset(&Opt, 0, sizeof(Opt));

		// Handle alpha blending if requested
		if (src->flags & SDL_SRCALPHA) {
			Opt.eDstDfbBldOp = E_MI_GFX_DFB_BLD_INVSRCALPHA;
			if (src->format->alpha != SDL_ALPHA_OPAQUE) {
				Opt.u32GlobalSrcConstColor =
				    (src->format->alpha << (src->format->Ashift - src->format->Aloss)) &
				    src->format->Amask;
				Opt.eDFBBlendFlag = (MI_Gfx_DfbBlendFlags_e)(E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY |
				                                             E_MI_GFX_DFB_BLEND_COLORALPHA |
				                                             E_MI_GFX_DFB_BLEND_ALPHACHANNEL);
			} else if (src->format->Amask) {
				Opt.eDFBBlendFlag = E_MI_GFX_DFB_BLEND_ALPHACHANNEL;
				Opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_SRCALPHA;
			} else {
				Opt.eDFBBlendFlag = E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY;
			}
		}

		// Handle color key (transparency) if requested
		if (src->flags & SDL_SRCCOLORKEY) {
			Opt.stSrcColorKeyInfo.bEnColorKey = TRUE;
			Opt.stSrcColorKeyInfo.eCKeyFmt = Src.eColorFmt;
			Opt.stSrcColorKeyInfo.eCKeyOp = E_MI_GFX_RGB_OP_EQUAL;
			Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorStart =
			    Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorEnd = src->format->colorkey;
		}
		if (Opt.eSrcDfbBldOp == 0)
			Opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
		Opt.eRotate = (MI_GFX_Rotate_e)rotate;
		Opt.eMirror = (MI_GFX_Mirror_e)mirror;
		Opt.stClipRect.s32Xpos = dst->clip_rect.x;
		Opt.stClipRect.s32Ypos = dst->clip_rect.y;
		Opt.stClipRect.u32Width = dst->clip_rect.w;
		Opt.stClipRect.u32Height = dst->clip_rect.h;

		static int blit_logged = 0;
		if (!blit_logged && (src->format->Amask != 0)) {
			LOG_info("MI_GFX blit: src %dx%d (bpp=%d Amask=0x%X) -> dst %dx%d (bpp=%d)\n", src->w,
			         src->h, src->format->BitsPerPixel, src->format->Amask, dst->w, dst->h,
			         dst->format->BitsPerPixel);
			LOG_info("MI_GFX blit: flags=0x%X eDFBBlendFlag=0x%X eSrcDfbBldOp=%d eDstDfbBldOp=%d\n",
			         src->flags, Opt.eDFBBlendFlag, Opt.eSrcDfbBldOp, Opt.eDstDfbBldOp);
			blit_logged = 1;
		}
		MI_GFX_BitBlit(&Src, &SrcRect, &Dst, &DstRect, &Opt, &Fence);
		if (!nowait)
			MI_GFX_WaitAllDone(FALSE, Fence);
	} else {
		LOG_info("Fallback to SDL_BlitSurface (no pixelsPa)\n");
		SDL_BlitSurface(src, srcrect, dst, dstrect);
	}
}

///////////////////////////////
// Lid Sensor (Hall Effect)
///////////////////////////////

#define LID_PATH "/sys/devices/soc0/soc/soc:hall-mh248/hallvalue"

void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}

int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open != lid.is_open) {
			lid.is_open = lid_open;
			if (state)
				*state = lid_open;
			return 1;
		}
	}
	return 0;
}

///////////////////////////////
// Input
///////////////////////////////

void PLAT_initInput(void) {}
void PLAT_quitInput(void) {}

///////////////////////////////
// Video - ION Memory and MI_GFX
///////////////////////////////

typedef struct HWBuffer {
	MI_PHY padd;
	void* vadd;
} HWBuffer;

#define EFFECT_BUFFER_SIZE (FIXED_WIDTH * FIXED_HEIGHT * 4)

static struct VID_Context {
	SDL_Surface* video;
	SDL_Surface* screen;
	SDL_Surface* effect;
	HWBuffer buffer;
	HWBuffer effect_buffer;

	int page;
	int width;
	int height;
	int pitch;

	int direct;
	int cleared;

	// Game rendering flag (set by PLAT_blitRenderer, cleared by PLAT_flip)
	int in_game;
} vid;

// Use shared EffectState from effect_system.h
static EffectState effect_state;

#define MODES_PATH "/sys/class/graphics/fb0/modes"

SDL_Surface* PLAT_initVideo(void) {
	// Detect device variant
	PLAT_detectVariant(&platform_variant);

	putenv("SDL_HIDE_BATTERY=1");
	// Enable strict vsync for proper frame pacing (rate control handles audio sync)
	putenv("GFX_FLIPWAIT=1");
	putenv("GFX_BLOCKING=1");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);

	vid.video = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);

	int buffer_size = ALIGN4K(PAGE_SIZE) * PAGE_COUNT;
	MI_SYS_MMA_Alloc(NULL, ALIGN4K(buffer_size), &vid.buffer.padd);
	MI_SYS_Mmap(vid.buffer.padd, ALIGN4K(buffer_size), &vid.buffer.vadd, true);

	vid.page = 1;
	vid.direct = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	vid.cleared = 0;

	vid.screen =
	    SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page * PAGE_SIZE), vid.width,
	                             vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_AUTO);
	vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page * PAGE_SIZE);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	// Initialize effect state using shared effect_system
	EFFECT_init(&effect_state);

	return vid.direct ? vid.video : vid.screen;
}

void PLAT_quitVideo(void) {
	if (vid.effect) {
		vid.effect->pixels = NULL;
		vid.effect->pixelsPa = 0;
		SDL_FreeSurface(vid.effect);
		vid.effect = NULL;
	}
	if (vid.effect_buffer.vadd) {
		MI_SYS_Munmap(vid.effect_buffer.vadd, ALIGN4K(EFFECT_BUFFER_SIZE));
		MI_SYS_MMA_Free(vid.effect_buffer.padd);
		vid.effect_buffer.vadd = NULL;
		vid.effect_buffer.padd = 0;
	}

	SDL_FreeSurface(vid.screen);

	MI_SYS_Munmap(vid.buffer.vadd, ALIGN4K(PAGE_SIZE));
	MI_SYS_MMA_Free(vid.buffer.padd);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	MI_SYS_FlushInvCache(vid.buffer.vadd + ALIGN4K(vid.page * PAGE_SIZE), ALIGN4K(PAGE_SIZE));
	MI_SYS_MemsetPa(vid.buffer.padd + ALIGN4K(vid.page * PAGE_SIZE), 0, PAGE_SIZE);
	SDL_FillRect(screen, NULL, 0);
}

void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	vid.cleared = 1;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	vid.direct = w == FIXED_WIDTH && h == FIXED_HEIGHT && pitch == FIXED_PITCH;
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;

	if (vid.direct) {
		memset(vid.video->pixels, 0, vid.pitch * vid.height);
	} else {
		vid.screen->pixels = NULL;
		vid.screen->pixelsPa = NULL;
		SDL_FreeSurface(vid.screen);

		vid.screen =
		    SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page * PAGE_SIZE), vid.width,
		                             vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_AUTO);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page * PAGE_SIZE);
		memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	}

	return vid.direct ? vid.video : vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {}
void PLAT_setNearestNeighbor(int enabled) {}

///////////////////////////////
// Pixel Effects and Scaling
///////////////////////////////

void PLAT_setSharpness(int sharpness) {
	(void)sharpness;
	// Force overlay regeneration by invalidating live state
	effect_state.live_scale = -1;
}

/**
 * Creates or updates the effect overlay surface.
 * Now uses EFFECT_getPatternPath() and EFFECT_getOpacity() from effect_system.
 */
static void updateEffectOverlay(void) {
	// Apply pending effect settings
	EFFECT_applyPending(&effect_state);

	// Clear overlay if no effect
	if (effect_state.type == EFFECT_NONE) {
		if (vid.effect) {
			vid.effect->pixels = NULL;
			vid.effect->pixelsPa = 0;
			SDL_FreeSurface(vid.effect);
			vid.effect = NULL;
		}
		if (vid.effect_buffer.vadd) {
			MI_SYS_Munmap(vid.effect_buffer.vadd, ALIGN4K(EFFECT_BUFFER_SIZE));
			MI_SYS_MMA_Free(vid.effect_buffer.padd);
			vid.effect_buffer.vadd = NULL;
			vid.effect_buffer.padd = 0;
		}
		EFFECT_markLive(&effect_state);
		return;
	}

	// Skip if overlay is already correct
	if (!EFFECT_needsUpdate(&effect_state))
		return;

	int scale = effect_state.scale > 0 ? effect_state.scale : 1;

	// Use shared EFFECT_getPatternPath() instead of local getEffectPattern()
	char pattern_path[256];
	const char* pattern =
	    EFFECT_getPatternPath(pattern_path, sizeof(pattern_path), effect_state.type, scale);
	if (!pattern) {
		LOG_info("Effect: no pattern for type %d scale %d\n", effect_state.type, scale);
		return;
	}

	// Use shared EFFECT_getOpacity()
	int opacity = EFFECT_getOpacity(scale);

	// Get color for grid effect tinting (GameBoy DMG palettes)
	int color = (effect_state.type == EFFECT_GRID) ? effect_state.color : 0;

	LOG_info("Effect: creating overlay type=%d scale=%d opacity=%d color=0x%04x pattern=%s\n",
	         effect_state.type, scale, opacity, color, pattern);

	// Pattern is pre-sized for this scale, tile at 1:1 (no scaling)
	SDL_Surface* temp =
	    EFFECT_createTiledSurfaceWithColor(pattern, 1, FIXED_WIDTH, FIXED_HEIGHT, color);
	if (!temp) {
		LOG_info("Effect: EFFECT_createTiledSurfaceWithColor failed!\n");
		return;
	}

	// Allocate ION memory for hardware blitting (if not already allocated)
	if (!vid.effect_buffer.vadd) {
		MI_SYS_MMA_Alloc(NULL, ALIGN4K(EFFECT_BUFFER_SIZE), &vid.effect_buffer.padd);
		MI_SYS_Mmap(vid.effect_buffer.padd, ALIGN4K(EFFECT_BUFFER_SIZE), &vid.effect_buffer.vadd,
		            true);
		LOG_info("Effect: allocated ION buffer padd=0x%llX vadd=%p\n",
		         (unsigned long long)vid.effect_buffer.padd, vid.effect_buffer.vadd);
	}

	// Free existing overlay surface (but keep ION buffer)
	if (vid.effect) {
		vid.effect->pixels = NULL;
		vid.effect->pixelsPa = 0;
		SDL_FreeSurface(vid.effect);
	}

	// Create SDL surface backed by ION memory (ARGB8888 for alpha blending)
	vid.effect =
	    SDL_CreateRGBSurfaceFrom(vid.effect_buffer.vadd, FIXED_WIDTH, FIXED_HEIGHT, 32,
	                             FIXED_WIDTH * 4, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (!vid.effect) {
		LOG_info("Effect: SDL_CreateRGBSurfaceFrom failed!\n");
		SDL_FreeSurface(temp);
		return;
	}
	vid.effect->pixelsPa = vid.effect_buffer.padd;

	// Copy tiled pattern to ION-backed surface
	memcpy(vid.effect->pixels, temp->pixels, EFFECT_BUFFER_SIZE);
	SDL_FreeSurface(temp);

	// Enable alpha blending with opacity from effect_system
	SDLX_SetAlpha(vid.effect, SDL_SRCALPHA, opacity);

	LOG_info("Effect: overlay created %dx%d in ION memory, pixelsPa=0x%llX\n", vid.effect->w,
	         vid.effect->h, (unsigned long long)vid.effect->pixelsPa);

	EFFECT_markLive(&effect_state);
}

void PLAT_setEffect(int effect) {
	if (effect != effect_state.next_type) {
		LOG_info("PLAT_setEffect: %d -> %d\n", effect_state.next_type, effect);
	}
	EFFECT_setType(&effect_state, effect);
}

void PLAT_setEffectColor(int color) {
	EFFECT_setColor(&effect_state, color);
}

void PLAT_vsync(int remaining) {
	if (remaining > 0)
		SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// Track scale for effect overlay generation
	EFFECT_setScale(&effect_state, renderer->scale);

	switch (renderer->scale) {
	case 6:
		return scale6x6_n16;
	case 5:
		return scale5x5_n16;
	case 4:
		return scale4x4_n16;
	case 3:
		return scale3x3_n16;
	case 2:
		return scale2x2_n16;
	default:
		return scale1x1_n16;
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.in_game = 1;

	// Clear to black when effects enabled to ensure clean black borders
	if (effect_state.next_type != EFFECT_NONE) {
		memset(renderer->dst, 0, renderer->dst_p * FIXED_HEIGHT);
	}

	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(renderer->src, dst, renderer->src_w, renderer->src_h,
	                           renderer->src_p, renderer->dst_w, renderer->dst_h, renderer->dst_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	if (!vid.direct)
		GFX_BlitSurfaceExec(vid.screen, NULL, vid.video, NULL, 0, 0, 0);

	// Apply effect overlay when in game mode (not menus)
	if (vid.in_game && effect_state.next_type != EFFECT_NONE) {
		updateEffectOverlay();
		if (vid.effect) {
			GFX_BlitSurfaceExec(vid.effect, NULL, vid.video, NULL, 0, 0, 0);
		}
	}

	SDL_Flip(vid.video);

	vid.in_game = 0;

	if (!vid.direct) {
		vid.page ^= 1;
		vid.screen->pixels = vid.buffer.vadd + ALIGN4K(vid.page * PAGE_SIZE);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page * PAGE_SIZE);
	}

	if (vid.cleared) {
		PLAT_clearVideo(vid.screen);
		vid.cleared = 0;
	}
}

///////////////////////////////
// Power Management - AXP223 PMIC (Plus Model)
///////////////////////////////

#define AXPDEV "/dev/i2c-1"
#define AXPID (0x34)

int axp_write(unsigned char address, unsigned char val) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[2];
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	buf[0] = address;
	buf[1] = val;
	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	packets.nmsgs = 1;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0)
		return -1;
	return 0;
}

int axp_read(unsigned char address) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char val;
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;
	msg[1].addr = AXPID;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &val;

	packets.nmsgs = 2;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0)
		return -1;
	return val;
}

///////////////////////////////
// Battery and Power Status
///////////////////////////////

static int online = 0;

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = (platform_variant.hw_features & HW_FEATURE_PMIC)
	                   ? (axp_read(0x00) & 0x4) > 0
	                   : getInt("/sys/devices/gpiochip0/gpio/gpio59/value");

	int i = getInt("/tmp/battery");

	if (i > 80)
		*charge = 100;
	else if (i > 60)
		*charge = 80;
	else if (i > 40)
		*charge = 60;
	else if (i > 20)
		*charge = 40;
	else if (i > 10)
		*charge = 20;
	else
		*charge = 10;

	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status, 16);
	online = prefixMatch("up", status);
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt("/sys/class/gpio/gpio4/value", 1);
		putInt("/sys/class/gpio/unexport", 4);
		putInt("/sys/class/pwm/pwmchip0/export", 0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable", 0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable", 1);
	} else {
		putInt("/sys/class/gpio/export", 4);
		putFile("/sys/class/gpio/gpio4/direction", "out");
		putInt("/sys/class/gpio/gpio4/value", 0);
	}
}

void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("shutdown");
	while (1)
		pause();
}

///////////////////////////////
// CPU Speed Control
///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	const char* level_name = "UNKNOWN";
	switch (speed) {
	case CPU_SPEED_MENU:
		freq = 504000;
		level_name = "MENU";
		break;
	case CPU_SPEED_POWERSAVE:
		freq = 1104000;
		level_name = "POWERSAVE";
		break;
	case CPU_SPEED_NORMAL:
		freq = 1296000;
		level_name = "NORMAL";
		break;
	case CPU_SPEED_PERFORMANCE:
		freq = 1488000;
		level_name = "PERFORMANCE";
		break;
	}

	LOG_info("PLAT_setCPUSpeed: %s (%d kHz)\n", level_name, freq);
	char cmd[32];
	sprintf(cmd, "overclock.elf %d\n", freq);
	int ret = system(cmd);
	if (ret != 0) {
		LOG_warn("overclock.elf returned %d for freq %d\n", ret, freq);
	}
}

/**
 * Gets available CPU frequencies from sysfs.
 *
 * miyoomini exposes frequencies via sysfs even though we use overclock.elf for setting.
 *
 * @param frequencies Output array to fill with frequencies (in kHz)
 * @param max_count Maximum number of frequencies to return
 * @return Number of frequencies found
 */
int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
	return PWR_getAvailableCPUFrequencies_sysfs(frequencies, max_count);
}

/**
 * Sets CPU frequency directly via overclock.elf.
 *
 * @param freq_khz Target frequency in kHz
 * @return 0 on success, -1 on failure
 */
int PLAT_setCPUFrequency(int freq_khz) {
	char cmd[32];
	sprintf(cmd, "overclock.elf %d\n", freq_khz);
	int ret = system(cmd);
	return (ret == 0) ? 0 : -1;
}

///////////////////////////////
// Rumble/Vibration
///////////////////////////////

void PLAT_setRumble(int strength) {
	static char lastvalue = 0;
	const char str_export[2] = "48";
	const char str_direction[3] = "out";
	char value[1];
	int fd;

	value[0] = (strength == 0 ? 0x31 : 0x30);
	if (lastvalue != value[0]) {
		fd = open("/sys/class/gpio/export", O_WRONLY);
		if (fd > 0) {
			write(fd, str_export, 2);
			close(fd);
		}
		fd = open("/sys/class/gpio/gpio48/direction", O_WRONLY);
		if (fd > 0) {
			write(fd, str_direction, 3);
			close(fd);
		}
		fd = open("/sys/class/gpio/gpio48/value", O_WRONLY);
		if (fd > 0) {
			write(fd, value, 1);
			close(fd);
		}
		lastvalue = value[0];
	}
}

///////////////////////////////
// Audio Configuration
///////////////////////////////

int PLAT_pickSampleRate(int requested, int max) {
	return max;
}

///////////////////////////////
// Device Identification
///////////////////////////////

char* PLAT_getModel(void) {
	return (char*)PLAT_getDeviceName();
}

int PLAT_isOnline(void) {
	return online;
}
