/**
 * minarch_options.h - Option list management utilities for MinArch
 *
 * Provides pure utility functions for searching and manipulating
 * option lists without complex initialization or global state dependencies.
 */

#ifndef MINARCH_OPTIONS_H
#define MINARCH_OPTIONS_H

/**
 * Option structure representing a single configurable option.
 *
 * Each option has a key (internal identifier), display name, possible values,
 * and a current value index.
 */
typedef struct MinArchOption {
	char* key; // Internal key (e.g., "video_scale")
	char* name; // Display name (e.g., "Video Scale")
	char* desc; // Description text (truncated)
	char* full; // Full description text
	char* var; // Raw variable string from core (internal)
	int default_value; // Default value index
	int value; // Current value index
	int count; // Number of possible values
	int lock; // Option is locked (from config file)
	char** values; // Value strings (internal)
	char** labels; // Label strings (for display)
} MinArchOption;

/**
 * Option list structure containing multiple options.
 */
typedef struct MinArchOptionList {
	int count; // Number of options
	int changed; // Has any option changed?
	MinArchOption* options; // Array of options

	int enabled_count; // Number of enabled options (filtered)
	MinArchOption** enabled_options; // Array of pointers to enabled options
} MinArchOptionList;

/**
 * Finds an option in a list by key.
 *
 * Searches the option list for an option with the given key.
 * Returns NULL if not found.
 *
 * @param list Option list to search
 * @param key Option key to find
 * @return Pointer to option, or NULL if not found
 *
 * @example
 *   MinArchOption* opt = MinArchOptions_find(&list, "video_scale");
 */
MinArchOption* MinArchOptions_find(MinArchOptionList* list, const char* key);

/**
 * Gets the current value string for an option.
 *
 * Looks up the option by key and returns its current value string.
 * Returns NULL if option not found.
 *
 * @param list Option list to search
 * @param key Option key to find
 * @return Current value string, or NULL if not found
 *
 * @example
 *   const char* scale = MinArchOptions_getValue(&list, "video_scale");
 *   // Returns: "2x" (or whatever is currently set)
 */
const char* MinArchOptions_getValue(MinArchOptionList* list, const char* key);

/**
 * Sets an option to a specific value by string.
 *
 * Finds the option by key, then searches for the value in its value list
 * and sets the option to that value's index. Marks the list as changed.
 *
 * @param list Option list containing the option
 * @param key Option key to set
 * @param value Value string to set (must match one of the option's values)
 *
 * @example
 *   MinArchOptions_setValue(&list, "video_scale", "3x");
 */
void MinArchOptions_setValue(MinArchOptionList* list, const char* key, const char* value);

/**
 * Sets an option to a specific value by index.
 *
 * Directly sets the option's value index without string lookup.
 * Marks the list as changed.
 *
 * @param list Option list containing the option
 * @param key Option key to set
 * @param value_index Index into the option's values array
 *
 * @example
 *   MinArchOptions_setRawValue(&list, "video_scale", 2); // Set to third value
 */
void MinArchOptions_setRawValue(MinArchOptionList* list, const char* key, int value_index);

/**
 * Finds the index of a value in an option's value list.
 *
 * Searches the option's values array for a matching string.
 * Returns 0 (default) if value is NULL or not found.
 *
 * @param opt Option to search
 * @param value Value string to find
 * @return Index of value in values array, or 0 if not found
 *
 * @example
 *   // Given option with values ["1x", "2x", "3x"]
 *   MinArchOptions_getValueIndex(opt, "2x") -> 1
 *   MinArchOptions_getValueIndex(opt, "4x") -> 0 (not found)
 *   MinArchOptions_getValueIndex(opt, NULL) -> 0
 */
int MinArchOptions_getValueIndex(const MinArchOption* opt, const char* value);

#endif /* MINARCH_OPTIONS_H */
