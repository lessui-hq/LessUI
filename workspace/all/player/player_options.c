/**
 * player_options.c - Option list management utilities implementation
 */

#include "player_options.h"

#include <string.h>

PlayerOption* PlayerOptions_find(PlayerOptionList* list, const char* key) {
	if (!list || !key) {
		return NULL;
	}

	for (int i = 0; i < list->count; i++) {
		PlayerOption* option = &list->options[i];
		if (option->key && strcmp(option->key, key) == 0) {
			return option;
		}
	}

	return NULL;
}

const char* PlayerOptions_getValue(PlayerOptionList* list, const char* key) {
	PlayerOption* option = PlayerOptions_find(list, key);

	if (option && option->value >= 0 && option->value < option->count) {
		return option->values[option->value];
	}

	return NULL;
}

void PlayerOptions_setValue(PlayerOptionList* list, const char* key, const char* value) {
	PlayerOption* option = PlayerOptions_find(list, key);

	if (!option || !value) {
		return;
	}

	// Find the value index
	int value_index = -1;
	for (int i = 0; i < option->count; i++) {
		if (option->values[i] && strcmp(option->values[i], value) == 0) {
			value_index = i;
			break;
		}
	}

	// Set the value if found
	if (value_index >= 0) {
		option->value = value_index;
		list->changed = 1;
	}
}

void PlayerOptions_setRawValue(PlayerOptionList* list, const char* key, int value_index) {
	PlayerOption* option = PlayerOptions_find(list, key);

	if (!option) {
		return;
	}

	// Bounds check
	if (value_index >= 0 && value_index < option->count) {
		option->value = value_index;
		list->changed = 1;
	}
}

int PlayerOptions_getValueIndex(const PlayerOption* opt, const char* value) {
	if (!value || !opt || !opt->values) {
		return 0;
	}

	for (int i = 0; i < opt->count; i++) {
		if (opt->values[i] && strcmp(opt->values[i], value) == 0) {
			return i;
		}
	}

	return 0;
}
