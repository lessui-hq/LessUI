#include "shui_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////
// String utilities
///////////////////////////////

char* safe_strdup(const char* s) {
	return s ? strdup(s) : NULL;
}

void trimWhitespace(char* s) {
	if (!s || !*s) return;

	// Trim leading
	char* start = s;
	while (*start && isspace((unsigned char)*start)) start++;

	// Trim trailing
	char* end = start + strlen(start) - 1;
	while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

	// Shift if needed
	if (start != s) {
		memmove(s, start, strlen(start) + 1);
	}
}

void unescapeNewlines(char* dst, const char* src, size_t dst_size) {
	if (!dst || !src || dst_size == 0) return;

	size_t si = 0, di = 0;
	while (src[si] && di < dst_size - 1) {
		if (src[si] == '\\' && src[si + 1] == 'n') {
			dst[di++] = '\n';
			si += 2;
		} else {
			dst[di++] = src[si++];
		}
	}
	dst[di] = '\0';
}

void toUppercase(char* s) {
	if (!s) return;
	for (; *s; s++) {
		*s = toupper((unsigned char)*s);
	}
}

///////////////////////////////
// JSON helpers
///////////////////////////////

int json_get_int(JSON_Object* obj, const char* name, int def) {
	JSON_Value* val = json_object_get_value(obj, name);
	if (val && json_value_get_type(val) == JSONNumber) {
		return (int)json_object_get_number(obj, name);
	}
	return def;
}

bool json_get_bool(JSON_Object* obj, const char* name, bool def) {
	JSON_Value* val = json_object_get_value(obj, name);
	if (val && json_value_get_type(val) == JSONBoolean) {
		return json_object_get_boolean(obj, name) != 0;
	}
	return def;
}

///////////////////////////////
// Color utilities
///////////////////////////////

#ifdef PLATFORM
SDL_Color hexToColor(const char* hex) {
	SDL_Color color = {0, 0, 0, 255};
	if (!hex || hex[0] != '#' || strlen(hex) < 7) {
		return color;
	}

	unsigned int r, g, b;
	if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
		color.r = r;
		color.g = g;
		color.b = b;
	}
	return color;
}
#endif
