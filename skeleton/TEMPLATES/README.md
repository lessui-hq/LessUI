# LessUI Pak Templates

This directory contains templates for direct paks (copied as-is to all platforms).

## Directory Structure

```
TEMPLATES/
└── paks/                   # Direct paks (copied as-is to all platforms)
    └── PAK.pak/
        └── launch.sh
```

## Pak Systems

LessUI has two pak systems:

### 1. Emulator Paks (workspace/all/paks/Emus/)
Template-based emulator paks for libretro cores. These are **generated** from templates.

- **Location**: `workspace/all/paks/Emus/`
- **How it works**: One template + per-core configs → generates 43 cores × 12 platforms
- **Purpose**: Eliminates duplication for emulators
- **Generation**: `./scripts/generate-paks.sh`

### 2. Direct Paks (skeleton/TEMPLATES/paks/)
Special-purpose paks that don't use the template system. These are **copied** directly.

- **Location**: `skeleton/TEMPLATES/paks/`
- **How it works**: Complete pak directories copied to all platforms
- **Purpose**: For special cases like PAK.pak (native app launcher)

## Usage

Paks are automatically generated during `make setup`:

```bash
make setup  # Generates all paks for all platforms
```

## Adding a Direct Pak

1. Create pak directory in `paks/`:
   ```bash
   mkdir -p paks/NEWPAK.pak
   ```

2. Add files (will be copied as-is):
   ```
   paks/NEWPAK.pak/
   ├── launch.sh
   └── default.cfg (optional)
   ```

3. Run generation - it will be copied to all platforms automatically

## Files

- **Emulator pak templates**: `workspace/all/paks/Emus/`
- **Direct pak templates**: `skeleton/TEMPLATES/paks/`
- **Generated** (during build): `build/SYSTEM/*/paks/Emus/`
- **Never committed**: Generated paks (see `.gitignore`)

See `docs/paks-architecture.md` for comprehensive documentation.
