# FAKESD - macOS Development SD Card

This directory simulates the SD card structure for LessUI development on macOS.

## Directory Structure

```
FAKESD/
├── Roms/           # ROM files organized by console
│   ├── GB/         # Game Boy ROMs (.gb files)
│   ├── GBA/        # Game Boy Advance ROMs (.gba files)
│   ├── NES/        # Nintendo Entertainment System (.nes files)
│   ├── SNES/       # Super Nintendo (.sfc, .smc files)
│   └── ...         # Add directories for other systems
│
├── .userdata/      # User-specific data (auto-created by minui)
│   └── [saves, states, etc.]
│
└── .system/        # System files (auto-created by minui)
    └── [platform-specific data]
```

## Setup

1. **Add ROMs**: Create subdirectories in `Roms/` for each console:
   ```bash
   mkdir -p Roms/GB Roms/GBA Roms/NES Roms/SNES
   ```

2. **Copy ROM files**: Add your legally obtained ROM files:
   ```bash
   cp ~/Downloads/game.gb Roms/GB/
   ```

3. **Run minui**: Use the dev build to test:
   ```bash
   make dev-run
   ```

## Supported Console Directories

Common console directory names (case-sensitive):
- `GB` - Game Boy
- `GBC` - Game Boy Color
- `GBA` - Game Boy Advance
- `NES` - Nintendo Entertainment System
- `SNES` - Super Nintendo
- `MD` - Sega Genesis/Mega Drive
- `GG` - Sega Game Gear
- `PS` - PlayStation
- `PCE` - PC Engine/TurboGrafx-16
- `PICO` - PICO-8

See the main LessUI documentation for a complete list of supported systems.

## Notes

- This directory is ignored by git (except for structure)
- ROM files are for development/testing only
- .userdata/ will be populated when you run minui and play games
- Save states and settings will persist between runs
