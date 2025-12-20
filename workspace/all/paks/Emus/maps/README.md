# Pak-Bundled ROM Name Mappings

This directory contains default ROM name mapping files that are bundled with emulator paks. These mappings provide user-friendly display names for ROMs with cryptic filenames (e.g., arcade games).

## Directory Structure

Maps are organized by **core filename** (without `.so` extension):

```
maps/
├── mame2003_plus_libretro/
│   └── map.txt       # MAME 2003 Plus arcade game mappings
├── fbneo_libretro/
│   └── map.txt       # FBNeo arcade game mappings
└── README.md
```

**Why organized by core?**
- Multiple paks can use the same core (e.g., CPS1, CPS2, CPS3, NG all use fbneo_libretro.so)
- Maps are shared: one fbneo_libretro map gets copied to all paks that use it
- Direct mapping from `cores.json` "core" field to map directory

## How It Works

When a pak is generated, if a `maps/{CORE}/map.txt` file exists, it will be copied into the pak directory as `{CORE}.pak/map.txt`. At runtime, launcher merges this pak-provided map with any user-created map in the ROM directory.

### Precedence

1. **User's map** (`/Roms/MAME/map.txt`) - highest priority
2. **Pak-bundled map** (`MAME.pak/map.txt`) - fallback

User entries always override pak entries, allowing users to customize any game name.

## Format

The map.txt format is tab-delimited:

```
filename<TAB>Display Name
```

Example:
```
mslug.zip	Metal Slug
kof98.zip	The King of Fighters '98
sf2.zip	Street Fighter II
```

## Use Cases

- **Arcade games** (MAME, FBNeo) - Map cryptic ROM names to full game titles
- **PICO-8 games** - Map `.p8.png` files to readable names
- **Any console** - Provide custom display names for any ROM collection

## Adding Mappings

To add a new mapping file:

1. Find the core filename in `cores.json` (e.g., `"core": "snes9x_libretro.so"`)
2. Strip the `.so`: `snes9x_libretro`
3. Create directory: `maps/snes9x_libretro/`
4. Create `map.txt` with your mappings
5. Run `./scripts/generate-paks.sh` to rebuild paks

The mapping will be automatically copied to **all paks** that use that core.

## Hiding ROMs

To hide a ROM from the file list, prefix the display name with a dot:

```
test.zip	.Test ROM
debug.zip	.Debug Build
```

These entries will still appear in the map but will be filtered out by launcher's display logic.
