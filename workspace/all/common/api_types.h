/**
 * api_types.h - Shared type definitions
 *
 * Basic types used across the API. Separated from api.h to allow
 * inclusion without pulling in SDL and other heavy dependencies.
 */

#ifndef __API_TYPES_H__
#define __API_TYPES_H__

#include <stdint.h>

///////////////////////////////
// Audio types
///////////////////////////////

/**
 * Stereo audio frame (left and right channels).
 */
typedef struct SND_Frame {
	int16_t left; // Left channel sample (-32768 to 32767)
	int16_t right; // Right channel sample (-32768 to 32767)
} SND_Frame;

///////////////////////////////
// Input types
///////////////////////////////

/**
 * Analog stick axis values.
 */
typedef struct PAD_Axis {
	int x; // X-axis value (-32768 to 32767)
	int y; // Y-axis value (-32768 to 32767)
} PAD_Axis;

///////////////////////////////
// Hardware types
///////////////////////////////

/**
 * Lid sensor state for devices with flip covers.
 */
typedef struct LID_Context {
	int has_lid; // 1 if device has a lid sensor, 0 otherwise
	int is_open; // 1 if lid is open, 0 if closed
} LID_Context;

#endif // __API_TYPES_H__
