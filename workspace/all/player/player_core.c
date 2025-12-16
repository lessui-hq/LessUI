/**
 * player_core.c - Core AV info processing utilities
 *
 * Pure functions for processing libretro core audio/video information.
 * Extracted from player.c for testability.
 */

#include "player_core.h"

void PlayerCore_buildGameInfo(const struct Game* game, struct retro_game_info* out_info) {
	if (!game || !out_info)
		return;

	// Use extracted temp path if available, otherwise original path
	out_info->path = game->tmp_path[0] ? game->tmp_path : game->path;
	out_info->data = game->data;
	out_info->size = game->size;
	out_info->meta = NULL; // Not used by most cores
}

double PlayerCore_calculateAspectRatio(double provided_aspect, unsigned int width,
                                       unsigned int height) {
	if (provided_aspect > 0) {
		return provided_aspect;
	}

	// Calculate from geometry if not provided
	if (height == 0) {
		return 1.0; // Fallback to square aspect
	}

	return (double)width / height;
}

void PlayerCore_processAVInfo(const struct retro_system_av_info* av_info,
                              PlayerCoreAVInfo* out_info) {
	if (!av_info || !out_info)
		return;

	out_info->fps = av_info->timing.fps;
	out_info->sample_rate = av_info->timing.sample_rate;
	out_info->aspect_ratio = PlayerCore_calculateAspectRatio(av_info->geometry.aspect_ratio,
	                                                         av_info->geometry.base_width,
	                                                         av_info->geometry.base_height);
}
