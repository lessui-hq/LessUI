/**
 * nointro_parser.h - Parser for No-Intro ROM naming convention
 *
 * Parses ROM filenames following the No-Intro naming standard:
 * https://wiki.no-intro.org/index.php?title=Naming_Convention
 *
 * Example filename:
 *   "Legend of Zelda, The (USA) (En,Ja) (v1.2) (Beta).nes"
 *
 * Parsed fields:
 *   - title: "Legend of Zelda, The"
 *   - display_name: "The Legend of Zelda" (article moved to front)
 *   - regions: "USA"
 *   - languages: "En,Ja"
 *   - version: "v1.2"
 *   - dev_status: "Beta"
 *   - etc.
 */

#ifndef NOINTRO_PARSER_H
#define NOINTRO_PARSER_H

#include <stdbool.h>

/**
 * Maximum length for each parsed field.
 * Most fields are short (regions, languages), but titles can be long.
 */
#define NOINTRO_MAX_TITLE 128
#define NOINTRO_MAX_FIELD 64

/**
 * Parsed No-Intro ROM name structure.
 *
 * All fields are null-terminated strings. Empty fields are set to "".
 */
typedef struct {
	char title[NOINTRO_MAX_TITLE]; // Raw title (may have ", The" suffix)
	char display_name[NOINTRO_MAX_TITLE]; // Display-ready name (article moved)
	char region[NOINTRO_MAX_FIELD]; // e.g., "USA", "Japan, USA", "World"
	char language[NOINTRO_MAX_FIELD]; // e.g., "En", "En,Ja"
	char version[NOINTRO_MAX_FIELD]; // e.g., "v1.2", "Rev A"
	char dev_status[NOINTRO_MAX_FIELD]; // e.g., "Beta", "Proto", "Sample"
	char additional[NOINTRO_MAX_FIELD]; // e.g., "Rumble Version"
	char special[NOINTRO_MAX_FIELD]; // e.g., "ST", "MB"
	char license[NOINTRO_MAX_FIELD]; // e.g., "Unl" (unlicensed)
	char status[NOINTRO_MAX_FIELD]; // e.g., "b" (bad dump), "!" (verified)
	bool has_tags; // True if any tags were found
} NoIntroName;

/**
 * Parses a No-Intro format filename into structured fields.
 *
 * This function handles the complete No-Intro naming convention including:
 * - Region tags: (USA), (Japan, USA), (World), (Europe)
 * - Language tags: (En), (En,Ja), (Fr,De,Es)
 * - Version tags: (v1.0), (Rev A), (Rev 1)
 * - Development status: (Beta), (Proto), (Sample)
 * - Additional info: (Rumble Version), (Disc 1)
 * - Special flags: (ST), (MB), (NP)
 * - License: (Unl) for unlicensed
 * - Status flags: [b] bad dump, [!] verified, etc.
 *
 * Article handling:
 * - "Name, The" -> display_name = "The Name"
 * - "Name, A" -> display_name = "A Name"
 * - "Name, An" -> display_name = "An Name"
 * - "The Name" -> display_name = "The Name" (no change)
 *
 * @param filename Input filename (with or without extension)
 * @param parsed Output structure (will be initialized and populated)
 *
 * @note File extension is automatically stripped before parsing
 * @note All parentheses and brackets after the title are treated as tags
 * @note The function is permissive - it won't fail on malformed input
 *
 * Example:
 *   NoIntroName info;
 *   parseNoIntroName("Super Metroid (Japan, USA) (En,Ja).sfc", &info);
 *   // info.display_name = "Super Metroid"
 *   // info.region = "Japan, USA"
 *   // info.language = "En,Ja"
 */
void parseNoIntroName(const char* filename, NoIntroName* parsed);

/**
 * Initializes a NoIntroName structure with empty values.
 *
 * @param parsed Structure to initialize
 */
void initNoIntroName(NoIntroName* parsed);

#endif // NOINTRO_PARSER_H
