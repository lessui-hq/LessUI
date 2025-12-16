/**
 * player_core.h - Core AV info processing utilities
 *
 * Provides pure functions for processing libretro core audio/video
 * information structures. These handle the math and logic for
 * extracting timing and aspect ratio data from cores.
 *
 * Extracted from player.c for testability.
 */

#ifndef __PLAYER_CORE_H__
#define __PLAYER_CORE_H__

#include "libretro.h"
#include "player_internal.h"

/**
 * Processed AV timing and aspect ratio information.
 */
typedef struct {
	double fps; /**< Frames per second */
	double sample_rate; /**< Audio sample rate in Hz */
	double aspect_ratio; /**< Display aspect ratio (width/height) */
} PlayerCoreAVInfo;

/**
 * Builds a retro_game_info struct from a Game instance.
 *
 * Selects the appropriate path (tmp_path if extracted from ZIP,
 * otherwise original path) and populates the game info structure.
 *
 * @param game Game instance to build info from
 * @param out_info Output structure to populate
 *
 * @example
 *   struct Game game = {...};
 *   struct retro_game_info info;
 *   PlayerCore_buildGameInfo(&game, &info);
 *   // info.path = game.tmp_path (if set) or game.path
 *   // info.data = game.data
 *   // info.size = game.size
 */
void PlayerCore_buildGameInfo(const struct Game* game, struct retro_game_info* out_info);

/**
 * Processes AV info from a core into usable timing and aspect ratio.
 *
 * Extracts FPS, sample rate, and calculates aspect ratio from
 * retro_system_av_info. If the core doesn't provide an aspect ratio
 * (value <= 0), calculates it from base geometry dimensions.
 *
 * @param av_info AV info structure from core.get_system_av_info()
 * @param out_info Output structure with processed values
 *
 * @example
 *   struct retro_system_av_info av = {...};
 *   PlayerCoreAVInfo info;
 *   PlayerCore_processAVInfo(&av, &info);
 *   // info.fps = av.timing.fps
 *   // info.sample_rate = av.timing.sample_rate
 *   // info.aspect_ratio = calculated or provided aspect ratio
 */
void PlayerCore_processAVInfo(const struct retro_system_av_info* av_info,
                              PlayerCoreAVInfo* out_info);

/**
 * Calculates aspect ratio from geometry.
 *
 * If the provided aspect ratio is valid (> 0), returns it.
 * Otherwise calculates from width and height.
 *
 * @param provided_aspect Aspect ratio from core (may be 0 or negative)
 * @param width Base width from geometry
 * @param height Base height from geometry
 * @return Calculated aspect ratio (always > 0)
 *
 * @example
 *   // Core provides aspect ratio
 *   double ar = PlayerCore_calculateAspectRatio(1.333, 256, 224);
 *   // Returns 1.333
 *
 *   // Core doesn't provide aspect ratio (0)
 *   double ar = PlayerCore_calculateAspectRatio(0, 256, 224);
 *   // Returns 256.0/224.0 = 1.142857
 */
double PlayerCore_calculateAspectRatio(double provided_aspect, unsigned int width,
                                       unsigned int height);

#endif // __PLAYER_CORE_H__
