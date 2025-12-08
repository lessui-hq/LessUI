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
	int value; // Current value index
	int default_value; // Default value index
	int count; // Number of possible values
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
 *   MinArchOption* opt = MinArch_findOption(&list, "video_scale");
 */
MinArchOption* MinArch_findOption(MinArchOptionList* list, const char* key);

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
 *   const char* scale = MinArch_getOptionValue(&list, "video_scale");
 *   // Returns: "2x" (or whatever is currently set)
 */
const char* MinArch_getOptionValue(MinArchOptionList* list, const char* key);

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
 *   MinArch_setOptionValue(&list, "video_scale", "3x");
 */
void MinArch_setOptionValue(MinArchOptionList* list, const char* key, const char* value);

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
 *   MinArch_setOptionRawValue(&list, "video_scale", 2); // Set to third value
 */
void MinArch_setOptionRawValue(MinArchOptionList* list, const char* key, int value_index);

#endif /* MINARCH_OPTIONS_H */
