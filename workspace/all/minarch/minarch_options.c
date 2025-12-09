/**
 * minarch_options.c - Option list management utilities implementation
 */

#include "minarch_options.h"

#include <string.h>

MinArchOption* MinArchOptions_find(MinArchOptionList* list, const char* key) {
	if (!list || !key) {
		return NULL;
	}

	for (int i = 0; i < list->count; i++) {
		MinArchOption* option = &list->options[i];
		if (option->key && strcmp(option->key, key) == 0) {
			return option;
		}
	}

	return NULL;
}

const char* MinArchOptions_getValue(MinArchOptionList* list, const char* key) {
	MinArchOption* option = MinArchOptions_find(list, key);

	if (option && option->value >= 0 && option->value < option->count) {
		return option->values[option->value];
	}

	return NULL;
}

void MinArchOptions_setValue(MinArchOptionList* list, const char* key, const char* value) {
	MinArchOption* option = MinArchOptions_find(list, key);

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

void MinArchOptions_setRawValue(MinArchOptionList* list, const char* key, int value_index) {
	MinArchOption* option = MinArchOptions_find(list, key);

	if (!option) {
		return;
	}

	// Bounds check
	if (value_index >= 0 && value_index < option->count) {
		option->value = value_index;
		list->changed = 1;
	}
}

int MinArchOptions_getValueIndex(const MinArchOption* opt, const char* value) {
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
