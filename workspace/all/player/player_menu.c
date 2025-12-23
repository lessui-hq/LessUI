/**
 * player_menu.c - In-game menu system implementation
 *
 * This module handles the in-game pause menu including:
 * - Menu display and navigation
 * - Save state management with previews
 * - Multi-disc game support
 * - Sleep/wake power management
 */

#include "player_menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "log.h"
#include "player_context.h"
#include "player_hwrender.h"
#include "player_internal.h"
#include "player_mappings.h"
#include "sdl.h"
#include "utils.h"

///////////////////////////////
// Service callbacks from context
///////////////////////////////
// All external dependencies from player.c are now accessed via
// ctx->callbacks. This eliminates extern declarations and improves
// testability and modularity.
//
// The callbacks are:
// - sram_write(), rtc_write() - Memory persistence
// - state_get_path(), state_read(), state_write(), state_autosave() - Save states
// - game_change_disc() - Multi-disc support
// - select_scaler(), video_refresh() - Video
// - set_overclock() - CPU speed
// - menu_options(), options_menu - Options menu
// - get_hdmi(), hdmi_mon() - HDMI monitoring
// - frame_ready_for_flip - Frame state flag

///////////////////////////////
// Menu item aliases
///////////////////////////////

#define ITEM_CONT MENU_ITEM_CONT
#define ITEM_SAVE MENU_ITEM_SAVE
#define ITEM_LOAD MENU_ITEM_LOAD
#define ITEM_OPTS MENU_ITEM_OPTS
#define ITEM_QUIT MENU_ITEM_QUIT

// Status codes for menu actions
enum {
	STATUS_CONT = 0,
	STATUS_SAVE = 1,
	STATUS_LOAD = 11,
	STATUS_OPTS = 23,
	STATUS_DISC = 24,
	STATUS_QUIT = 30,
	STATUS_RESET = 31,
};

///////////////////////////////
// Menu state
///////////////////////////////

static PlayerMenuState menu = {
    .bitmap = NULL,
    .overlay = NULL,
    .items =
        {
            [ITEM_CONT] = "Continue",
            [ITEM_SAVE] = "Save",
            [ITEM_LOAD] = "Load",
            [ITEM_OPTS] = "Options",
            [ITEM_QUIT] = "Quit",
        },
    .disc_paths = {NULL},
    .launcher_dir = {0},
    .slot_path = {0},
    .base_path = {0},
    .bmp_path = {0},
    .txt_path = {0},
    .disc = -1,
    .total_discs = 0,
    .slot = 0,
    .save_exists = 0,
    .preview_exists = 0,
};

// Provide access to menu state for context initialization
PlayerMenuState* PlayerMenu_getState(void) {
	return &menu;
}

///////////////////////////////
// Menu lifecycle
///////////////////////////////

static void Menu_init_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;
	struct Game* g = ctx->game;
	int dev_w = *ctx->device_width;
	int dev_h = *ctx->device_height;

	m->overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, dev_w, dev_h, FIXED_DEPTH, RGBA_MASK_AUTO);
	SDLX_SetAlpha(m->overlay, SDL_SRCALPHA, 0x80);
	SDL_FillRect(m->overlay, NULL, 0);

	char emu_name[256];
	getEmuName(g->path, emu_name);
	(void)snprintf(m->launcher_dir, sizeof(m->launcher_dir), SHARED_USERDATA_PATH "/.launcher/%s",
	               emu_name);
	mkdir(m->launcher_dir, 0755);

	(void)snprintf(m->slot_path, sizeof(m->slot_path), "%s/%s.txt", m->launcher_dir, g->name);

	if (*ctx->simple_mode)
		m->items[ITEM_OPTS] = "Reset";

	if (g->m3u_path[0]) {
		char* tmp;
		SAFE_STRCPY(m->base_path, g->m3u_path);
		tmp = strrchr(m->base_path, '/') + 1;
		tmp[0] = '\0';

		// Read m3u file
		FILE* file = fopen(g->m3u_path, "r");
		if (file) {
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue;

				char disc_path[256];
				SAFE_STRCPY(disc_path, m->base_path);
				tmp = disc_path + strlen(disc_path);
				safe_strcpy(tmp, line, sizeof(disc_path) - (tmp - disc_path));

				if (exists(disc_path)) {
					char* dup = strdup(disc_path);
					if (!dup) {
						LOG_error("Failed to allocate disc path");
						break;
					}
					m->disc_paths[m->total_discs] = dup;
					if (exactMatch(disc_path, g->path)) {
						m->disc = m->total_discs;
					}
					m->total_discs += 1;
				}
			}
			(void)fclose(file); // M3U file opened for reading
		}
	}
}

static void Menu_quit_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;

	// Free allocated disc paths
	for (int i = 0; i < m->total_discs; i++) {
		free(m->disc_paths[i]);
		m->disc_paths[i] = NULL;
	}
	m->total_discs = 0;

	SDL_FreeSurface(m->overlay);
}

///////////////////////////////
// Sleep/wake handlers
///////////////////////////////

static void Menu_beforeSleep_ctx(PlayerContext* ctx) {
	struct Game* g = ctx->game;
	PlayerCallbacks* cb = ctx->callbacks;
	cb->sram_write();
	cb->rtc_write();
	cb->state_autosave();
	putFile(AUTO_RESUME_PATH, g->path + strlen(SDCARD_PATH));
	PWR_setCPUSpeed(CPU_SPEED_IDLE);
}

static void Menu_afterSleep_ctx(PlayerContext* ctx) {
	unlink(AUTO_RESUME_PATH);
	ctx->callbacks->set_overclock(*ctx->overclock);
}

///////////////////////////////
// State management
///////////////////////////////

static void Menu_initState_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;
	if (exists(m->slot_path))
		m->slot = getInt(m->slot_path);
	if (m->slot == 8)
		m->slot = 0;

	m->save_exists = 0;
	m->preview_exists = 0;
}

static void Menu_updateState_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;
	struct Game* g = ctx->game;
	PlayerCallbacks* cb = ctx->callbacks;

	int last_slot = *ctx->state_slot;
	*ctx->state_slot = m->slot;

	char save_path[256];
	cb->state_get_path(save_path);

	*ctx->state_slot = last_slot;

	(void)snprintf(m->bmp_path, sizeof(m->bmp_path), "%s/%s.%d.bmp", m->launcher_dir, g->name,
	               m->slot);
	(void)snprintf(m->txt_path, sizeof(m->txt_path), "%s/%s.%d.txt", m->launcher_dir, g->name,
	               m->slot);

	m->save_exists = exists(save_path);
	m->preview_exists = m->save_exists && exists(m->bmp_path);
}

static void Menu_saveState_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;
	GFX_Renderer* r = (GFX_Renderer*)ctx->renderer;
	PlayerCallbacks* cb = ctx->callbacks;

	Menu_updateState_ctx(ctx);

	if (m->total_discs) {
		char* disc_path = m->disc_paths[m->disc];
		putFile(m->txt_path, disc_path + strlen(m->base_path));
	}

	SDL_Surface* bitmap = m->bitmap;
	if (!bitmap)
		bitmap = SDL_CreateRGBSurfaceFrom(r->src, r->true_w, r->true_h, FIXED_DEPTH, r->src_p,
		                                  RGBA_MASK_565);
	SDL_RWops* out = SDL_RWFromFile(m->bmp_path, "wb");
	SDL_SaveBMP_RW(bitmap, out, 1);

	if (bitmap != m->bitmap)
		SDL_FreeSurface(bitmap);

	*ctx->state_slot = m->slot;
	putInt(m->slot_path, m->slot);
	cb->state_write();
}

static void Menu_loadState_ctx(PlayerContext* ctx) {
	PlayerMenuState* m = ctx->menu;
	PlayerCallbacks* cb = ctx->callbacks;

	Menu_updateState_ctx(ctx);

	if (m->save_exists) {
		if (m->total_discs) {
			char slot_disc_name[256];
			getFile(m->txt_path, slot_disc_name, 256);

			char slot_disc_path[256];
			if (slot_disc_name[0] == '/')
				SAFE_STRCPY(slot_disc_path, slot_disc_name);
			else
				(void)snprintf(slot_disc_path, sizeof(slot_disc_path), "%s%s", m->base_path,
				               slot_disc_name);

			char* disc_path = m->disc_paths[m->disc];
			if (!exactMatch(slot_disc_path, disc_path)) {
				cb->game_change_disc(slot_disc_path);
			}
		}

		*ctx->state_slot = m->slot;
		putInt(m->slot_path, m->slot);
		cb->state_read();
	}
}

///////////////////////////////
// Menu scaling
///////////////////////////////

static void Menu_scale_ctx(PlayerContext* ctx, SDL_Surface* src, SDL_Surface* dst) {
	GFX_Renderer* r = (GFX_Renderer*)ctx->renderer;
	struct Core* c = ctx->core;
	int dev_w = *ctx->device_width;
	int dev_h = *ctx->device_height;

	uint16_t* s = src->pixels;
	uint16_t* d = dst->pixels;

	int sw = src->w;
	int sh = src->h;
	int sp = src->pitch / FIXED_BPP;

	int dw = dst->w;
	int dh = dst->h;
	int dp = dst->pitch / FIXED_BPP;

	int rx = 0;
	int ry = 0;
	int rw = dw;
	int rh = dh;

	int scaling = *ctx->screen_scaling;
	if (scaling == PLAYER_SCALE_CROPPED && dev_w == HDMI_WIDTH) {
		scaling = PLAYER_SCALE_NATIVE;
	}
	if (scaling == PLAYER_SCALE_NATIVE) {
		rx = r->dst_x;
		ry = r->dst_y;
		rw = r->src_w;
		rh = r->src_h;
		if (r->scale) {
			rw *= r->scale;
			rh *= r->scale;
		} else {
			rw -= r->src_x * 2;
			rh -= r->src_y * 2;
			sw = rw;
			sh = rh;
		}

		if (dw == dev_w / 2) {
			rx /= 2;
			ry /= 2;
			rw /= 2;
			rh /= 2;
		}
	} else if (scaling == PLAYER_SCALE_CROPPED) {
		sw -= r->src_x * 2;
		sh -= r->src_y * 2;

		rx = r->dst_x;
		ry = r->dst_y;
		rw = sw * r->scale;
		rh = sh * r->scale;

		if (dw == dev_w / 2) {
			rx /= 2;
			ry /= 2;
			rw /= 2;
			rh /= 2;
		}
	}

	if (scaling == PLAYER_SCALE_ASPECT || rw > dw || rh > dh) {
		double fixed_aspect_ratio = ((double)dev_w) / dev_h;
		int core_aspect = c->aspect_ratio * 1000;
		int fixed_aspect = fixed_aspect_ratio * 1000;

		if (core_aspect > fixed_aspect) {
			rw = dw;
			rh = rw / c->aspect_ratio;
			rh += rh % 2;
		} else if (core_aspect < fixed_aspect) {
			rh = dh;
			rw = rh * c->aspect_ratio;
			rw += rw % 2;
			rw = (rw / 8) * 8;
		} else {
			rw = dw;
			rh = dh;
		}

		rx = (dw - rw) / 2;
		ry = (dh - rh) / 2;
	}

	// Nearest neighbor scaling
	int mx = (sw << 16) / rw;
	int my = (sh << 16) / rh;
	int ox = (r->src_x << 16);
	int sx;
	int sy = (r->src_y << 16);
	int lr = -1;
	int sr = 0;
	int dr = ry * dp;
	int cp = dp * FIXED_BPP;

	for (int dy = 0; dy < rh; dy++) {
		sx = ox;
		sr = (sy >> 16) * sp;
		if (sr == lr) {
			memcpy(d + dr, d + dr - dp, cp);
		} else {
			for (int dx = 0; dx < rw; dx++) {
				d[dr + rx + dx] = s[sr + (sx >> 16)];
				sx += mx;
			}
		}
		lr = sr;
		sy += my;
		dr += dp;
	}
}

///////////////////////////////
// Alias lookup
///////////////////////////////

static void getAlias(const char* path, char* alias) {
	char* tmp;
	char map_path[256];
	SAFE_STRCPY(map_path, path);
	tmp = strrchr(map_path, '/');
	if (tmp) {
		tmp += 1;
		safe_strcpy(tmp, "map.txt", sizeof(map_path) - (tmp - map_path));
	}
	char* file_name = strrchr(path, '/');
	if (file_name)
		file_name += 1;

	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue;

				tmp = strchr(line, '\t');
				if (!tmp)
					continue;
				tmp[0] = '\0';
				tmp += 1;

				if (exactMatch(line, file_name)) {
					safe_strcpy(alias, tmp, MAX_PATH);
					(void)fclose(file); // M3U file opened for reading
					return;
				}
			}
			(void)fclose(file); // M3U file opened for reading
		}
	}
}

///////////////////////////////
// Main menu loop
///////////////////////////////

// Static wrappers for PWR_update callback (used within this module)
static void Menu_beforeSleep(void) {
	Menu_beforeSleep_ctx(PlayerContext_get());
}

static void Menu_afterSleep(void) {
	Menu_afterSleep_ctx(PlayerContext_get());
}

static void Menu_loop_ctx(PlayerContext* ctx) {
	LOG_debug("Menu_loop_ctx: enter, HW=%d", PlayerHWRender_isEnabled());

	PlayerMenuState* m = ctx->menu;
	GFX_Renderer* r = (GFX_Renderer*)ctx->renderer;
	struct Game* g = ctx->game;
	struct Core* c = ctx->core;
	SDL_Surface** scr = ctx->screen;
	PlayerCallbacks* cb = ctx->callbacks;
	int dev_w = *ctx->device_width;
	int dev_h = *ctx->device_height;
	int dev_p = *ctx->device_pitch;

	// For HW rendering, we can't access the frame buffer (it's in GPU memory)
	// so create a blank backing surface instead of scaling the game frame
	SDL_Surface* backing =
	    SDL_CreateRGBSurface(SDL_SWSURFACE, dev_w, dev_h, FIXED_DEPTH, RGBA_MASK_565);

	if (PlayerHWRender_isEnabled()) {
		LOG_debug("Menu_loop_ctx: HW rendering - using blank backing");
		SDL_FillRect(backing, NULL, 0);
		m->bitmap = NULL;
	} else {
		LOG_debug("Menu_loop_ctx: creating bitmap surface");
		m->bitmap = SDL_CreateRGBSurfaceFrom(r->src, r->true_w, r->true_h, FIXED_DEPTH, r->src_p,
		                                     RGBA_MASK_565);
		LOG_debug("Menu_loop_ctx: scaling to backing");
		Menu_scale_ctx(ctx, m->bitmap, backing);
	}

	int restore_w = (*scr)->w;
	int restore_h = (*scr)->h;
	int restore_p = (*scr)->pitch;
	if (restore_w != dev_w || restore_h != dev_h) {
		*scr = GFX_resize(dev_w, dev_h, dev_p);
	}

	cb->sram_write();
	cb->rtc_write();
	PWR_warn(0);
	if (!HAS_POWER_BUTTON)
		PWR_enableSleep();
	PWR_setCPUSpeed(CPU_SPEED_IDLE);
	GFX_setEffect(EFFECT_NONE);

	int rumble_strength = VIB_getStrength();
	VIB_setStrength(0);

	PWR_enableAutosleep();
	PAD_reset();

	// Path and string things
	char rom_name[256];
	getDisplayName(g->name, rom_name);
	getAlias(g->path, rom_name);

	int rom_disc = -1;
	char disc_name[16];
	if (m->total_discs) {
		rom_disc = m->disc;
		(void)snprintf(disc_name, sizeof(disc_name), "Disc %i", m->disc + 1);
	}

	int selected = 0;
	Menu_initState_ctx(ctx);

	int show_setting = 0;
	int dirty = 1;

	SDL_Surface* preview =
	    SDL_CreateRGBSurface(SDL_SWSURFACE, dev_w / 2, dev_h / 2, FIXED_DEPTH, RGBA_MASK_565);

	while (*ctx->show_menu) {
		GFX_startFrame();
		uint32_t now = SDL_GetTicks();

		PAD_poll();

		if (PAD_justPressed(BTN_UP)) {
			selected -= 1;
			if (selected < 0)
				selected += MENU_ITEM_COUNT;
			dirty = 1;
		} else if (PAD_justPressed(BTN_DOWN)) {
			selected += 1;
			if (selected >= MENU_ITEM_COUNT)
				selected -= MENU_ITEM_COUNT;
			dirty = 1;
		} else if (PAD_justPressed(BTN_LEFT)) {
			if (m->total_discs > 1 && selected == ITEM_CONT) {
				m->disc -= 1;
				if (m->disc < 0)
					m->disc += m->total_discs;
				dirty = 1;
				(void)snprintf(disc_name, sizeof(disc_name), "Disc %i", m->disc + 1);
			} else if (selected == ITEM_SAVE || selected == ITEM_LOAD) {
				m->slot -= 1;
				if (m->slot < 0)
					m->slot += MENU_SLOT_COUNT;
				dirty = 1;
			}
		} else if (PAD_justPressed(BTN_RIGHT)) {
			if (m->total_discs > 1 && selected == ITEM_CONT) {
				m->disc += 1;
				if (m->disc == m->total_discs)
					m->disc -= m->total_discs;
				dirty = 1;
				(void)snprintf(disc_name, sizeof(disc_name), "Disc %i", m->disc + 1);
			} else if (selected == ITEM_SAVE || selected == ITEM_LOAD) {
				m->slot += 1;
				if (m->slot >= MENU_SLOT_COUNT)
					m->slot -= MENU_SLOT_COUNT;
				dirty = 1;
			}
		}

		if (dirty && (selected == ITEM_SAVE || selected == ITEM_LOAD)) {
			Menu_updateState_ctx(ctx);
		}

		if (PAD_justPressed(BTN_B) || (BTN_WAKE != BTN_MENU && PAD_tappedMenu(now))) {
			*ctx->show_menu = 0;
		} else if (PAD_justPressed(BTN_A)) {
			switch (selected) {
			case ITEM_CONT:
				if (m->total_discs && rom_disc != m->disc) {
					char* disc_path = m->disc_paths[m->disc];
					cb->game_change_disc(disc_path);
				}
				*ctx->show_menu = 0;
				break;

			case ITEM_SAVE: {
				Menu_saveState_ctx(ctx);
				*ctx->show_menu = 0;
			} break;
			case ITEM_LOAD: {
				Menu_loadState_ctx(ctx);
				*ctx->show_menu = 0;
			} break;
			case ITEM_OPTS: {
				if (*ctx->simple_mode) {
					c->reset();
					*ctx->show_menu = 0;
				} else {
					int old_scaling = *ctx->screen_scaling;
					cb->menu_options(cb->options_menu);
					if (*ctx->screen_scaling != old_scaling) {
						// Only recalc scaler and rescale bitmap for software rendering
						// HW rendering handles scaling in PlayerHWRender_present()
						// and has no bitmap to rescale (backing is already black)
						if (m->bitmap != NULL) {
							cb->select_scaler(r->true_w, r->true_h, r->src_p);

							restore_w = (*scr)->w;
							restore_h = (*scr)->h;
							restore_p = (*scr)->pitch;
							*scr = GFX_resize(dev_w, dev_h, dev_p);

							SDL_FillRect(backing, NULL, 0);
							Menu_scale_ctx(ctx, m->bitmap, backing);
						}
					}
					dirty = 1;
				}
			} break;
			case ITEM_QUIT:
				*ctx->show_menu = 0;
				*ctx->quit = 1;
				break;
			}
			if (!*ctx->show_menu)
				break;
		}

		PWR_update(&dirty, &show_setting, Menu_beforeSleep, Menu_afterSleep);

		if (dirty) {
			GFX_clear(*scr);

			SDL_BlitSurface(backing, NULL, *scr, NULL);
			SDL_BlitSurface(m->overlay, NULL, *scr, NULL);

			int ox, oy;
			int ow = GFX_blitHardwareGroup(*scr, show_setting);
			int max_width = DP(ui.screen_width) - DP(ui.edge_padding * 2) - ow;

			char display_name[256];
			int text_width = GFX_truncateText(font.large, rom_name, display_name, max_width,
			                                  DP(ui.button_padding * 2));
			max_width = MIN(max_width, text_width);

			SDL_Surface* text;
			text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);
			GFX_blitPill(
			    ASSET_BLACK_PILL, *scr,
			    &(SDL_Rect){ui.edge_padding_px, ui.edge_padding_px, max_width, ui.pill_height_px});
			SDL_BlitSurface(text, &(SDL_Rect){0, 0, max_width - DP(ui.button_padding * 2), text->h},
			                *scr,
			                &(SDL_Rect){ui.edge_padding_px + DP(ui.button_padding),
			                            ui.edge_padding_px + ui.text_offset_px});
			SDL_FreeSurface(text);

			if (show_setting && !cb->get_hdmi())
				GFX_blitHardwareHints(*scr, show_setting);
			else
				GFX_blitButtonGroup(
				    (char*[]){BTN_SLEEP == BTN_POWER ? "POWER" : "MENU", "SLEEP", NULL}, 0, *scr,
				    0);
			GFX_blitButtonGroup((char*[]){"B", "BACK", "A", "OKAY", NULL}, 1, *scr, 1);

			// Vertically center menu items (calculate in pixel space to avoid rounding accumulation)
			int header_offset_px = ui.edge_padding_px + ui.pill_height_px;
			int footer_offset_px = ui.screen_height_px - ui.edge_padding_px - ui.pill_height_px;
			int content_area_height_px = footer_offset_px - header_offset_px;
			int menu_height_px = MENU_ITEM_COUNT * ui.pill_height_px;
			int oy_px =
			    header_offset_px + (content_area_height_px - menu_height_px) / 2 - DP(ui.padding);

			for (int i = 0; i < MENU_ITEM_COUNT; i++) {
				char* item = m->items[i];
				SDL_Color text_color = COLOR_WHITE;

				if (i == selected) {
					if (m->total_discs > 1 && i == ITEM_CONT) {
						GFX_blitPill(ASSET_DARK_GRAY_PILL, *scr,
						             &(SDL_Rect){ui.edge_padding_px, oy_px + DP(ui.padding),
						                         DP(ui.screen_width - ui.edge_padding * 2),
						                         ui.pill_height_px});
						text = TTF_RenderUTF8_Blended(font.large, disc_name, COLOR_WHITE);
						SDL_BlitSurface(
						    text, NULL, *scr,
						    &(SDL_Rect){DP(ui.screen_width - ui.edge_padding - ui.button_padding) -
						                    text->w,
						                oy_px + DP(ui.padding) + ui.text_offset_px});
						SDL_FreeSurface(text);
					}

					TTF_SizeUTF8(font.large, item, &ow, NULL);
					ow += DP(ui.button_padding * 2);

					GFX_blitPill(ASSET_WHITE_PILL, *scr,
					             &(SDL_Rect){ui.edge_padding_px,
					                         oy_px + DP(ui.padding) + (i * ui.pill_height_px), ow,
					                         ui.pill_height_px});
					text_color = COLOR_BLACK;
				} else {
					text = TTF_RenderUTF8_Blended(font.large, item, COLOR_BLACK);
					SDL_BlitSurface(text, NULL, *scr,
					                &(SDL_Rect){DP(2 + ui.edge_padding + ui.button_padding),
					                            oy_px + DP(1 + ui.padding) +
					                                (i * ui.pill_height_px) + ui.text_offset_px});
					SDL_FreeSurface(text);
				}

				text = TTF_RenderUTF8_Blended(font.large, item, text_color);
				SDL_BlitSurface(text, NULL, *scr,
				                &(SDL_Rect){ui.edge_padding_px + DP(ui.button_padding),
				                            oy_px + DP(ui.padding) + (i * ui.pill_height_px) +
				                                ui.text_offset_px});
				SDL_FreeSurface(text);
			}

			// Slot preview
			if (selected == ITEM_SAVE || selected == ITEM_LOAD) {
#define WINDOW_RADIUS 4
#define PAGINATION_HEIGHT 6
				int hw = dev_w / 2;
				int hh = dev_h / 2;
				int pw = hw + DP(WINDOW_RADIUS * 2);
				int ph = hh + DP(WINDOW_RADIUS * 2 + PAGINATION_HEIGHT + WINDOW_RADIUS);
				ox = dev_w - pw - DP(ui.edge_padding);
				oy = (dev_h - ph) / 2;

				GFX_blitRect(ASSET_STATE_BG, *scr, &(SDL_Rect){ox, oy, pw, ph});
				ox += DP(WINDOW_RADIUS);
				oy += DP(WINDOW_RADIUS);

				if (m->preview_exists) {
					SDL_Surface* bmp = IMG_Load(m->bmp_path);
					SDL_Surface* raw_preview =
					    SDL_ConvertSurface(bmp, (*scr)->format, SDL_SWSURFACE);

					SDL_FillRect(preview, NULL, 0);
					Menu_scale_ctx(ctx, raw_preview, preview);
					SDL_BlitSurface(preview, NULL, *scr, &(SDL_Rect){ox, oy});
					SDL_FreeSurface(raw_preview);
					SDL_FreeSurface(bmp);
				} else {
					SDL_Rect preview_rect = {ox, oy, hw, hh};
					SDL_FillRect(*scr, &preview_rect, 0);
					if (m->save_exists)
						GFX_blitMessage(font.large, "No Preview", *scr, &preview_rect);
					else
						GFX_blitMessage(font.large, "Empty Slot", *scr, &preview_rect);
				}

				// Pagination dots
				ox += (pw - DP(15 * MENU_SLOT_COUNT)) / 2;
				oy += hh + DP(WINDOW_RADIUS);
				for (int i = 0; i < MENU_SLOT_COUNT; i++) {
					if (i == m->slot)
						GFX_blitAsset(ASSET_PAGE, NULL, *scr, &(SDL_Rect){ox + DP(i * 15), oy});
					else
						GFX_blitAsset(ASSET_DOT, NULL, *scr,
						              &(SDL_Rect){ox + DP(i * 15) + DP(2), oy + DP(2)});
				}
			}

			// Use GL presentation when HW rendering is active to avoid SDL/GL conflicts
			if (PlayerHWRender_isEnabled()) {
				LOG_debug("Menu: about to call PlayerHWRender_presentSurface");
				PlayerHWRender_presentSurface(*scr);
				LOG_debug("Menu: returned from PlayerHWRender_presentSurface");
			} else {
				GFX_present(NULL);
			}
			dirty = 0;
		} else
			GFX_sync();
		cb->hdmi_mon();
	}

	SDL_FreeSurface(preview);

	PAD_reset();

	GFX_clearAll();
	PWR_warn(1);

	if (!*ctx->quit) {
		if (restore_w != dev_w || restore_h != dev_h) {
			*scr = GFX_resize(restore_w, restore_h, restore_p);
		}
		GFX_setEffect(*ctx->screen_effect);
		GFX_clear(*scr);
		cb->video_refresh(r->src, r->true_w, r->true_h, r->src_p);
		// Skip SDL present for HW rendering - the frame is already on screen
		// and calling GFX_present would conflict with the GL context
		if (*cb->frame_ready_for_flip && !PlayerHWRender_isEnabled()) {
			GFX_present(r);
			*cb->frame_ready_for_flip = 0;
		}

		cb->set_overclock(*ctx->overclock);
		if (rumble_strength)
			VIB_setStrength(rumble_strength);

		if (!HAS_POWER_BUTTON)
			PWR_disableSleep();
	} else if (exists(NOUI_PATH))
		PWR_powerOff();

	SDL_FreeSurface(m->bitmap);
	m->bitmap = NULL;
	SDL_FreeSurface(backing);
	PWR_disableAutosleep();
}

///////////////////////////////
// Public API
///////////////////////////////

void PlayerMenu_init(PlayerContext* ctx) {
	Menu_init_ctx(ctx);
}

void PlayerMenu_quit(PlayerContext* ctx) {
	Menu_quit_ctx(ctx);
}

void PlayerMenu_loop(PlayerContext* ctx) {
	Menu_loop_ctx(ctx);
}

void PlayerMenu_beforeSleep(PlayerContext* ctx) {
	Menu_beforeSleep_ctx(ctx);
}

void PlayerMenu_afterSleep(PlayerContext* ctx) {
	Menu_afterSleep_ctx(ctx);
}

void PlayerMenu_initState(PlayerContext* ctx) {
	Menu_initState_ctx(ctx);
}

void PlayerMenu_updateState(PlayerContext* ctx) {
	Menu_updateState_ctx(ctx);
}

void PlayerMenu_saveState(PlayerContext* ctx) {
	Menu_saveState_ctx(ctx);
}

void PlayerMenu_loadState(PlayerContext* ctx) {
	Menu_loadState_ctx(ctx);
}

void PlayerMenu_scale(PlayerContext* ctx, SDL_Surface* src, SDL_Surface* dst) {
	Menu_scale_ctx(ctx, src, dst);
}

void PlayerMenu_getAlias(PlayerContext* ctx, char* path, char* alias) {
	(void)ctx;
	getAlias(path, alias);
}

int PlayerMenu_message(PlayerContext* ctx, char* message, char** pairs) {
	(void)ctx;
	(void)message;
	(void)pairs;
	// TODO: Move Menu_message implementation here
	return 0;
}

int PlayerMenu_options(PlayerContext* ctx, MenuList* list) {
	return ctx->callbacks->menu_options(list);
}

///////////////////////////////
// Menu Navigation (testable pure functions)
///////////////////////////////

void PlayerMenuNav_init(PlayerMenuNavState* state, int count, int max_visible) {
	state->count = count;
	state->max_visible = max_visible;
	state->selected = 0;
	state->start = 0;
	state->end = (count < max_visible) ? count : max_visible;
	state->visible_rows = state->end;
	state->dirty = 1;
	state->await_input = 0;
	state->should_exit = 0;
}

int PlayerMenuNav_navigate(PlayerMenuNavState* state, int direction) {
	if (state->count <= 0)
		return 0;

	if (direction < 0) {
		// Up
		state->selected -= 1;
		if (state->selected < 0) {
			// Wrap to bottom
			state->selected = state->count - 1;
			state->start =
			    (state->count > state->max_visible) ? state->count - state->max_visible : 0;
			state->end = state->count;
		} else if (state->selected < state->start) {
			// Scroll up
			state->start -= 1;
			state->end -= 1;
		}
	} else if (direction > 0) {
		// Down
		state->selected += 1;
		if (state->selected >= state->count) {
			// Wrap to top
			state->selected = 0;
			state->start = 0;
			state->end = state->visible_rows;
		} else if (state->selected >= state->end) {
			// Scroll down
			state->start += 1;
			state->end += 1;
		}
	} else {
		return 0; // No direction
	}

	return 1;
}

void PlayerMenuNav_advanceItem(PlayerMenuNavState* state) {
	state->selected += 1;
	if (state->selected >= state->count) {
		// Wrap to top
		state->selected = 0;
		state->start = 0;
		state->end = state->visible_rows;
	} else if (state->selected >= state->end) {
		// Scroll down
		state->start += 1;
		state->end += 1;
	}
}

int PlayerMenuNav_cycleValue(MenuItem* item, int direction) {
	if (!item->values)
		return 0;

	if (direction < 0) {
		// Left - decrement with wraparound
		if (item->value > 0) {
			item->value -= 1;
		} else {
			// Count values and wrap to end
			int j;
			for (j = 0; item->values[j]; j++)
				;
			item->value = j - 1;
		}
	} else if (direction > 0) {
		// Right - increment with wraparound
		if (item->values[item->value + 1]) {
			item->value += 1;
		} else {
			item->value = 0;
		}
	} else {
		return 0; // No direction
	}

	return 1;
}

PlayerMenuAction PlayerMenuNav_getAction(MenuList* list, MenuItem* item, int menu_type, int btn_a,
                                         int btn_b, int btn_x, char** btn_labels) {
	if (btn_b) {
		return MENU_ACTION_EXIT;
	}

	if (btn_a) {
		if (item->on_confirm) {
			return MENU_ACTION_CONFIRM;
		}
		if (item->submenu) {
			return MENU_ACTION_SUBMENU;
		}
		if (list->on_confirm) {
			// Check if this is a button binding item
			// Must have both btn_labels and matching values to be a binding
			if (btn_labels && item->values == btn_labels) {
				return MENU_ACTION_AWAIT_INPUT;
			}
			return MENU_ACTION_CONFIRM;
		}
	}

	if (btn_x && menu_type == MENU_INPUT) {
		return MENU_ACTION_CLEAR_INPUT;
	}

	return MENU_ACTION_NONE;
}
