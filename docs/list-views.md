# List Views

This document describes the list view system in minui (the launcher).

## Overview

List views are the primary UI element in minui, used for ROM browsing, tools, recents, and collections.

**Implementation:** `workspace/all/minui/minui.c`

## Current Features

### Sorting

ROM lists are sorted using natural sort with No-Intro conventions:

| Feature | Description |
|---------|-------------|
| Natural sort | Numbers compared by value: "Game 2" < "Game 10" |
| Article skipping | "The Legend of Zelda" sorts under "L", not "T" |
| Case insensitive | "mario" and "Mario" sort together |

Supported articles for sort skipping: "The ", "A ", "An " (must have trailing space)

### Navigation

| Feature | Description |
|---------|-------------|
| Single item scroll | D-pad up/down moves one item at a time |
| Page jump | D-pad left/right jumps by visible row count |
| Alphabetic jump | L1/R1 jumps to next/previous letter group (A-Z, #) |
| Wrap-around | Scrolling past bottom wraps to top and vice versa |
| Button repeat | 300ms initial delay, then 100ms repeat interval |

### Selection & Highlighting

| Feature | Description |
|---------|-------------|
| Selection pill | White rounded rectangle behind selected item |
| Text color flip | Selected text changes from white to black |

### Text Display

| Feature | Description |
|---------|-------------|
| Text truncation | Names exceeding available width get ellipsis (...) |
| Display name cleaning | Strips file extensions, region codes, parenthetical metadata |
| Article fixing | "Legend of Zelda, The" displays as "The Legend of Zelda" |
| Sorting prefix removal | Strips "NNN) " prefixes used for custom sort order |
| Duplicate disambiguation | Shows filename or emulator name when display names collide |

### Display Name Aliasing (map.txt)

Custom display names can be defined via `map.txt` files using tab-delimited format:

```
mario.gb	Super Mario Land
tetris.gb	Tetris DX
unwanted.gb	.hidden
```

| Location | Purpose |
|----------|---------|
| `/Roms/GB/map.txt` | Aliases for ROMs in that system folder |
| `/Roms/map.txt` | Aliases for system folder names at root |
| `/Collections/map.txt` | Aliases for collection entries |

**Special behaviors:**
- Alias starting with `.` hides the entry from the list
- List re-sorts alphabetically after aliases are applied
- Aliases are also used by minarch for in-game title display

### Thumbnails

| Feature | Description |
|---------|-------------|
| Thumbnail display | Shows image on right side when ROM is selected |
| Path convention | ROM at `/Roms/GB/game.gb` → thumbnail at `/Roms/GB/.res/game.gb.png` |
| Scaling | Preserves aspect ratio, max 50% screen width |
| Caching | Thumbnail cached to avoid reloading on every frame |

### List Types

| List | Description |
|------|-------------|
| ROM list | Files within a system folder |
| Root directory | System folders, Tools, Recently Played, Collections |
| Recently Played | Last 24 games launched (stored in `.minui/recent.txt`) |
| Collections | Custom ROM lists defined by `.txt` files |
| Multi-disc | Disc selection for games with `.m3u` playlists |

### Hardware Status

| Feature | Description |
|---------|-------------|
| Battery indicator | Shows current charge level (top-right) |
| Brightness indicator | Shows when adjusting brightness |
| Volume indicator | Shows when adjusting volume |

### Button Hints

Context-sensitive button labels displayed at bottom of screen:
- OPEN / SELECT - A button action
- BACK - B button action
- RESUME - X button when save state exists
- Action-specific labels per menu context

### Visual Styling

| Property | Value |
|----------|-------|
| Indentation | Tabs (4-wide) |
| Font sizes | Large (16pt), Medium (14pt), Small (12pt), Tiny (10pt) |
| Colors | White text on dark background, black text when selected |
| Spacing | DP-scaled padding and margins for cross-device consistency |
| Row height | Dynamically calculated based on screen PPI |

## Implementation Details

### Scrolling Window

Lists maintain a visible window with `start` and `end` indices:

```c
// When selection moves below visible area
if (selected >= end) {
    start++;
    end++;
}

// When selection moves above visible area
if (selected < start) {
    start--;
    end--;
}
```

### Alphabetic Index

Each directory builds an index array mapping letters (A-Z, #) to entry positions:

```c
// getIndexChar() returns 0 for non-alpha, 1-26 for A-Z
int alpha = getIndexChar(entry->name);
if (alphas[alpha] == -1) {
    alphas[alpha] = entry_index;  // First entry for this letter
}
```

L1/R1 navigation uses this index to jump between letter groups.

### Display Name Processing

Names go through several transformations:

1. `getDisplayName()` - Strips extensions, region codes, parentheticals, fixes articles
2. `trimSortingMeta()` - Removes "NNN) " prefixes
3. `map.txt` lookup - Applies custom aliases
4. `getUniqueName()` - Generates disambiguator for duplicates

### Sorting Algorithm

Sorting uses `strnatcasecmp()` which provides:

1. Natural number ordering ("Game 2" < "Game 10")
2. Article skipping for sort ("The Zelda" sorts under "Z")
3. Case-insensitive comparison
