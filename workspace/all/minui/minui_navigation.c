/**
 * minui_navigation.c - Navigation logic for MinUI
 *
 * Provides testable navigation functions using the context pattern.
 */

#include "minui_navigation.h"
#include "minui_launcher.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

///////////////////////////////
// Pure navigation logic
///////////////////////////////

void MinUINav_determineAction(const Entry* entry, const char* current_path,
                              const char* collections_path, MinUINavAction* out_action) {
	if (!entry || !out_action) {
		if (out_action) {
			out_action->action = MINUI_NAV_NONE;
		}
		return;
	}

	out_action->action = MINUI_NAV_NONE;
	out_action->path[0] = '\0';
	out_action->last_path[0] = '\0';
	out_action->auto_launch = 0;
	out_action->resume_slot = -1;

	// Copy the entry path
	strncpy(out_action->path, entry->path, sizeof(out_action->path) - 1);
	out_action->path[sizeof(out_action->path) - 1] = '\0';

	switch (entry->type) {
	case ENTRY_ROM:
		out_action->action = MINUI_NAV_OPEN_ROM;
		// For collection ROMs, use collection path for state restoration
		if (collections_path && current_path &&
		    prefixMatch((char*)collections_path, (char*)current_path)) {
			char* tmp = strrchr(entry->path, '/');
			if (tmp) {
				(void)snprintf(out_action->last_path, sizeof(out_action->last_path), "%s%s",
				               current_path, tmp);
			}
		}
		break;

	case ENTRY_PAK:
		out_action->action = MINUI_NAV_OPEN_PAK;
		break;

	case ENTRY_DIR:
		out_action->action = MINUI_NAV_OPEN_DIR;
		out_action->auto_launch = 1; // Directories auto-launch by default
		break;

	default:
		out_action->action = MINUI_NAV_NONE;
		break;
	}
}

bool MinUINav_shouldAutoLaunch(const char* dir_path, char* out_launch_path, int launch_path_size) {
	if (!dir_path || !out_launch_path || launch_path_size <= 0) {
		return false;
	}

	out_launch_path[0] = '\0';

	// Get directory name for cue/m3u file naming
	char* dir_name = strrchr(dir_path, '/');
	if (!dir_name) {
		return false;
	}
	dir_name++; // Skip the slash

	// Check for .cue file
	(void)snprintf(out_launch_path, launch_path_size, "%s/%s.cue", dir_path, dir_name);
	if (exists(out_launch_path)) {
		return true;
	}

	// Check for .m3u file
	(void)snprintf(out_launch_path, launch_path_size, "%s/%s.m3u", dir_path, dir_name);
	if (exists(out_launch_path)) {
		return true;
	}

	out_launch_path[0] = '\0';
	return false;
}

void MinUINav_buildPakCommand(const char* pak_path, char* out_cmd, int cmd_size) {
	if (!pak_path || !out_cmd || cmd_size <= 0) {
		if (out_cmd && cmd_size > 0) {
			out_cmd[0] = '\0';
		}
		return;
	}

	// Use the launcher module for proper quoting
	// Note: MinUI_buildPakCommand modifies pak_path, so we copy it first
	char pak_copy[512];
	strncpy(pak_copy, pak_path, sizeof(pak_copy) - 1);
	pak_copy[sizeof(pak_copy) - 1] = '\0';
	MinUI_buildPakCommand(out_cmd, cmd_size, pak_copy);
}

void MinUINav_buildRomCommand(const char* rom_path, const char* emu_name, const char* emu_path,
                              bool should_resume, const char* slot_path, const char* m3u_path,
                              const char* userdata_path, char* out_cmd, int cmd_size,
                              char* out_sd_path, int sd_path_size) {
	if (!rom_path || !emu_path || !out_cmd || cmd_size <= 0) {
		if (out_cmd && cmd_size > 0) {
			out_cmd[0] = '\0';
		}
		return;
	}

	// Start with the ROM path
	strncpy(out_sd_path, rom_path, sd_path_size - 1);
	out_sd_path[sd_path_size - 1] = '\0';

	// Handle multi-disc resume (get the saved disc)
	if (should_resume && m3u_path && m3u_path[0] != '\0' && userdata_path && emu_name &&
	    slot_path) {
		// Get the ROM filename from m3u path
		char* rom_file = strrchr(m3u_path, '/');
		if (rom_file) {
			rom_file++; // Skip slash

			// Read the slot file to get the slot number
			// Note: In production this would read from slot_path,
			// but for testability we pass the slot value in
		}
	}

	// Build the command using the launcher module
	// Note: MinUI_buildRomCommand modifies paths, so we use copies
	char emu_copy[512], rom_copy[512];
	strncpy(emu_copy, emu_path, sizeof(emu_copy) - 1);
	emu_copy[sizeof(emu_copy) - 1] = '\0';
	strncpy(rom_copy, out_sd_path, sizeof(rom_copy) - 1);
	rom_copy[sizeof(rom_copy) - 1] = '\0';
	MinUI_buildRomCommand(out_cmd, cmd_size, emu_copy, rom_copy);
}

///////////////////////////////
// Context-aware navigation
///////////////////////////////

void MinUINav_openEntry(MinUIContext* ctx, Entry* entry) {
	if (!ctx || !entry) {
		return;
	}

	// Get current directory path for collection handling
	// Note: Directory is defined in minui.c, we use void* here
	void* top = ctx_getTop(ctx);
	const char* current_path = top ? *((char**)top) : NULL; // First field of Directory is path

	MinUINavAction action;
	MinUINav_determineAction(entry, current_path, NULL, &action); // TODO: pass COLLECTIONS_PATH

	// Set the recent alias from entry name
	if (ctx->recent_alias) {
		*ctx->recent_alias = entry->name;
	}

	switch (action.action) {
	case MINUI_NAV_OPEN_ROM:
		if (ctx->callbacks && ctx->callbacks->queue_next) {
			// In full implementation, this would call MinUINav_openRom
			// For now, just note that we need the full openRom logic
		}
		break;

	case MINUI_NAV_OPEN_PAK:
		MinUINav_openPak(ctx, action.path);
		break;

	case MINUI_NAV_OPEN_DIR:
		MinUINav_openDirectory(ctx, action.path, action.auto_launch);
		break;

	default:
		break;
	}
}

void MinUINav_openPak(MinUIContext* ctx, const char* path) {
	if (!ctx || !path || !ctx->callbacks) {
		return;
	}

	// Add to recents if in Roms path (would need ROMS_PATH)
	// if (ctx->callbacks->add_recent && prefixMatch(ROMS_PATH, path)) {
	//     ctx->callbacks->add_recent((char*)path, NULL);
	// }

	// Save last path
	if (ctx->callbacks->save_last) {
		ctx->callbacks->save_last((char*)path);
	}

	// Build and queue command
	if (ctx->callbacks->queue_next) {
		char cmd[512];
		MinUINav_buildPakCommand(path, cmd, sizeof(cmd));
		ctx->callbacks->queue_next(cmd);
	}
}

void MinUINav_openDirectory(MinUIContext* ctx, const char* path, int auto_launch) {
	if (!ctx || !path) {
		return;
	}

	// Check for auto-launch
	if (auto_launch) {
		char launch_path[512];
		if (MinUINav_shouldAutoLaunch(path, launch_path, sizeof(launch_path))) {
			// Would call MinUINav_openRom with launch_path
			// For now, this is a stub
			return;
		}
	}

	// Open directory using callback
	if (ctx->callbacks && ctx->callbacks->open_directory) {
		ctx->callbacks->open_directory((char*)path, 0); // Don't auto-launch subdirs
	}
}

void MinUINav_closeDirectory(MinUIContext* ctx) {
	if (!ctx || !ctx->restore) {
		return;
	}

	// Note: Directory is defined in minui.c, we use void* here
	void* top = ctx_getTop(ctx);
	Array* stack = ctx_getStack(ctx);

	if (!top || !stack || stack->count <= 1) {
		return;
	}

	// Note: Full implementation would need accessor functions for Directory fields
	// For now, this is called from minui.c which has access to the struct
	// The context-aware version will be implemented incrementally
}

void MinUINav_openRom(MinUIContext* ctx, const char* path, const char* last) {
	// This is a complex function that will be implemented incrementally
	// For now, it's a stub that shows the structure
	if (!ctx || !path) {
		return;
	}

	// 1. Handle multi-disc games (m3u)
	// 2. Get emulator name and path
	// 3. Handle resume state
	// 4. Add to recents
	// 5. Save last path
	// 6. Build and queue command

	(void)last; // Unused for now
}
