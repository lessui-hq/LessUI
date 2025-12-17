# Cross-Platform Pak Architecture

This document describes the unified pak architecture for LessUI, enabling self-contained, cross-platform paks that include source code, scripts, resources, and platform-specific assets in a single location.

## Overview

### Previous Problems (Solved)

1. **Duplication**: Tool paks like `Clock.pak` had `launch.sh` duplicated 11 times across platforms
2. **Scattered code**: Native source lived in separate directories while pak scaffolding lived elsewhere
3. **No single source of truth**: Updates required touching many files
4. **Inconsistencies**: Minor differences crept in (trailing newlines, debug comments)
5. **EXTRAS complexity**: Separate release package created confusion for users

### Solution

A unified directory structure where each pak is fully self-contained in `workspace/all/paks/`:

- Metadata (`pak.json`)
- Source code (if native)
- Shell scripts (cross-platform with platform branching)
- Resources and platform-specific assets

Paks can reference shared code in `workspace/all/common/` since they're in the same repository.

## Directory Structure

```
workspace/all/paks/
├── makefile                        # Master makefile - discovers and builds all paks
├── Clock/
│   ├── pak.json                    # Metadata (name, platforms, build type)
│   ├── launch.sh                   # Entry point (cross-platform)
│   ├── src/                        # Native C code
│   │   ├── clock.c
│   │   └── makefile                # Build configuration
│   └── build/                      # Generated output (per-platform)
│       └── miyoomini/
│           └── clock.elf
│
├── Wifi/                           # Future: WiFi configuration pak
│   ├── pak.json
│   ├── launch.sh
│   ├── bin/
│   │   ├── service-on              # Cross-platform shell scripts
│   │   └── miyoomini/iw            # Platform-specific binary
│   ├── lib/
│   │   └── miyoomini/              # Platform-specific libraries
│   └── res/
│       └── wpa_supplicant.conf.tmpl
│
├── Input/                          # Future: migrate from workspace/all/minput
│   ├── pak.json
│   ├── launch.sh
│   └── src/
│       ├── minput.c
│       └── makefile
│
└── Files/                          # Future: file manager pak
    ├── pak.json
    ├── launch.sh                   # Platform branching for different binaries
    └── bin/
        ├── miyoomini/DinguxCommander
        └── magicmini/351Files

# Note: Emulator paks are in workspace/all/paks/Emus/
# They use a template system due to high uniformity across cores
```

## pak.json Schema

```json
{
  "name": "Wifi",
  "type": "tool",
  "description": "Manage WiFi settings",
  "version": "1.0.0",

  "platforms": ["miyoomini", "my282", "my355", "rg35xxplus", "tg5040"],

  "build": {
    "type": "native",
    "source": "src/",
    "output": "{{PLATFORM}}/clock.elf",
    "makefile": "src/makefile"
  },

  "dependencies": {
    "jq": {
      "version": "1.7.1",
      "arch_binary": true,
      "url_template": "https://github.com/jqlang/jq/releases/download/jq-{{VERSION}}/jq-linux-{{ARCH}}"
    },
    "launcher-presenter": {
      "version": "0.10.0",
      "platform_binary": true,
      "url_template": "https://github.com/josegonzalez/launcher-presenter/releases/download/{{VERSION}}/launcher-presenter-{{PLATFORM}}"
    }
  },

  "launch": "launch.sh"
}
```

### Schema Fields

| Field                                 | Required | Description                                               |
| ------------------------------------- | -------- | --------------------------------------------------------- |
| `name`                                | Yes      | Display name of the pak                                   |
| `type`                                | Yes      | One of: `tool`, `emulator`, `app`                         |
| `description`                         | No       | Brief description                                         |
| `version`                             | No       | Semantic version                                          |
| `debug`                               | No       | If `true`, pak is only included in DEBUG=1 builds         |
| `platforms`                           | Yes      | Array of supported platform IDs                           |
| `build`                               | No       | Native code build configuration                           |
| `build.type`                          | -        | `native` (compile C), `download` (fetch binaries), `none` |
| `build.source`                        | -        | Path to source directory                                  |
| `build.output`                        | -        | Output path pattern (supports `{{PLATFORM}}`)             |
| `build.makefile`                      | -        | Path to makefile                                          |
| `dependencies`                        | No       | External binaries to download                             |
| `dependencies.<name>.version`         | -        | Version to download                                       |
| `dependencies.<name>.arch_binary`     | -        | `true` if binary varies by arch (arm/arm64)               |
| `dependencies.<name>.platform_binary` | -        | `true` if binary varies by platform                       |
| `dependencies.<name>.url_template`    | -        | Download URL with placeholders                            |
| `launch`                              | Yes      | Entry point script                                        |

## Pak Types

### Type 1: Shell Script Only

Simplest pak type - just shell scripts, no compilation needed.

```
Bootlogo.pak/
├── pak.json
├── launch.sh
└── res/
    └── template.bmp
```

**pak.json:**

```json
{
  "name": "Bootlogo",
  "type": "tool",
  "platforms": ["miyoomini", "trimuismart", "rg35xxplus", "tg5040"],
  "launch": "launch.sh"
}
```

### Type 2: Native Code (Compiled)

Pak includes C source code that's cross-compiled for each platform.

```
Clock.pak/
├── pak.json
├── launch.sh
└── src/
    ├── clock.c
    └── makefile
```

**pak.json:**

```json
{
  "name": "Clock",
  "type": "tool",
  "platforms": ["all"],
  "build": {
    "type": "native",
    "source": "src/",
    "makefile": "src/makefile"
  },
  "launch": "launch.sh"
}
```

**launch.sh:**

```sh
#!/bin/sh
cd "$(dirname "$0")"
./clock.elf
```

### Type 3: Downloaded Binaries

External binaries fetched during build (like sftpgo, jq).

```
FTP.Server.pak/
├── pak.json
├── launch.sh
├── bin/
│   └── .gitkeep
└── settings.json
```

**pak.json:**

```json
{
  "name": "FTP Server",
  "type": "tool",
  "platforms": ["miyoomini", "my282", "my355", "rg35xxplus", "tg5040"],
  "dependencies": {
    "sftpgo": {
      "version": "v2.6.6",
      "arch_binary": true,
      "url_template": "https://github.com/drakkan/sftpgo/releases/download/{{VERSION}}/sftpgo_{{VERSION}}_linux_{{ARCH}}.tar.xz"
    },
    "jq": {
      "version": "1.7.1",
      "arch_binary": true,
      "url_template": "https://github.com/jqlang/jq/releases/download/jq-{{VERSION}}/jq-linux-{{ARCH}}"
    }
  },
  "launch": "launch.sh"
}
```

### Type 4: Platform-Specific Binaries

Pre-built binaries that differ per-platform (like Files.pak with different file managers).

```
Files.pak/
├── pak.json
├── launch.sh
└── bin/
    ├── miyoomini/DinguxCommander
    ├── magicmini/351Files
    └── rg35xxplus/              # Empty - uses system binary
```

**launch.sh:**

```sh
#!/bin/sh
cd "$(dirname "$0")"

case "$PLATFORM" in
    rg35xxplus)
        # Use system file manager
        DIR="/mnt/vendor/bin/fileM"
        if [ -d "$DIR" ]; then
            cd "$DIR"
            HOME="$SDCARD_PATH"
            ./dinguxCommand_en.dge
        else
            show.elf "$PAK_DIR/res/missing.png" 4
        fi
        ;;
    magicmini)
        HOME="$SDCARD_PATH"
        ./bin/$PLATFORM/351Files
        ;;
    *)
        HOME="$SDCARD_PATH"
        ./bin/$PLATFORM/DinguxCommander
        ;;
esac
```

### Type 5: Emulator (Templated)

Emulators use templates with variable substitution.

```
GB.pak/
├── pak.json
├── launch.sh.template
└── configs/
    ├── default.cfg
    └── tg5040/
        └── default-brick.cfg
```

**pak.json:**

```json
{
  "name": "GB",
  "type": "emulator",
  "platforms": ["all"],
  "core": {
    "emu_exe": "gambatte",
    "arm64_only": false
  },
  "launch": "launch.sh.template"
}
```

**launch.sh.template:**

```sh
#!/bin/sh

EMU_EXE={{EMU_EXE}}
EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
cd "$HOME"
{{NICE_PREFIX}}player.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM"
```

## Platform Handling Patterns

### Pattern 1: Environment Variables

Scripts can use environment variables set by Launcher:

- `$PLATFORM` - Platform ID (e.g., "miyoomini", "rg35xxplus")
- `$DEVICE` - Device variant (e.g., "miyoominiplus", "brick")
- `$SDCARD_PATH` - SD card mount point
- `$USERDATA_PATH` - User data directory

### Pattern 2: Platform Branching in Scripts

```sh
#!/bin/sh
case "$PLATFORM" in
    miyoomini|my282|my355)
        # Miyoo family handling
        ;;
    rg35xxplus)
        # Anbernic handling
        ;;
    tg5040)
        # TrimUI handling
        ;;
esac
```

### Pattern 3: Platform-Specific Files

Use directory structure for platform-specific assets:

```
bin/
├── service-on              # Cross-platform script
├── arm/jq                  # Architecture-specific binary
├── arm64/jq
├── miyoomini/iw            # Platform-specific binary
└── rg35xxplus/
```

**Loading pattern:**

```sh
export PATH="$PAK_DIR/bin/$ARCH:$PAK_DIR/bin/$PLATFORM:$PAK_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$PAK_DIR/lib/$PLATFORM:$PAK_DIR/lib:$LD_LIBRARY_PATH"
```

### Pattern 4: Template Overrides

For config files, use base + override pattern:

```
res/
├── wpa_supplicant.conf.tmpl           # Base template
├── wpa_supplicant.conf.miyoomini.tmpl # Platform override
└── wpa_supplicant.conf.my355.tmpl     # Platform override
```

**Selection logic:**

```sh
template="$PAK_DIR/res/wpa_supplicant.conf.tmpl"
if [ -f "$PAK_DIR/res/wpa_supplicant.conf.$PLATFORM.tmpl" ]; then
    template="$PAK_DIR/res/wpa_supplicant.conf.$PLATFORM.tmpl"
fi
```

## Build System Integration

### Build Flow

```
make setup
  └── scripts/generate-paks.sh
      └── Generates emulator paks only (GB.pak, GBA.pak, etc.)
      └── Tool paks are NOT generated here

make build PLATFORM=miyoomini
  └── workspace/Makefile calls workspace/all/paks/makefile
      └── For each pak with src/makefile:
          └── Cross-compile in Docker
              └── Output: workspace/all/paks/<pak>/build/<platform>/<binary>.elf

make system PLATFORM=miyoomini
  └── For each pak in workspace/all/paks/:
      ├── Check pak.json for platform compatibility
      ├── Create build/SYSTEM/<platform>/paks/<pak>.pak/
      └── Copy launch.sh, pak.json, res/, bin/, lib/, AND compiled binary
```

### Key Design Decision

Tool paks are constructed entirely during `make system`, not `make setup`. This means:

- No duplicate copy steps (everything happens once)
- Binary and assets are assembled together
- Platform filtering happens at construction time

### Key Files

- `workspace/all/paks/makefile` - Master makefile that discovers and builds all paks with native code
- `Makefile` (system target) - Constructs complete tool paks in build directory
- `scripts/generate-paks.sh` - Only handles emulator paks (not tool paks)

## Migration Guide (Completed)

### Migrated Paks

All high and medium priority tool paks have been successfully migrated:

| Pak          | Type                       | Old Location            | New Location                    | Highlights                                                                                          |
| ------------ | -------------------------- | ----------------------- | ------------------------------- | --------------------------------------------------------------------------------------------------- |
| **Clock**    | Native                     | `workspace/all/clock/`  | `workspace/all/paks/Clock/src/` | First migration, 11 duplicates eliminated                                                           |
| **Input**    | Native                     | `workspace/all/minput/` | `workspace/all/paks/Input/src/` | Second native pak, validates pattern                                                                |
| **Bootlogo** | Hybrid                     | (was duplicated ×7)     | `workspace/all/paks/Bootlogo/`  | Native for miyoomini, shell for others; platform-specific resources; launcher-presenter integration |
| **Files**    | Platform-Specific Binaries | (was duplicated ×10)    | `workspace/all/paks/Files/`     | Multiple file managers (DinguxCommander, 351Files); `bin/<platform>/` pattern                       |

### Migration Steps (Template)

For future pak migrations:

1. **Examine**: Check all skeleton copies to understand platform differences
2. **Create structure**: `mkdir -p workspace/all/paks/<Name>/src` (if native code)
3. **Move files**: Source code, resources, binaries to unified location
4. **Create pak.json**: Define name, platforms, build configuration
5. **Create launch.sh**: Single cross-platform launcher (use `case "$PLATFORM"` for branching)
6. **Update makefiles**: Remove old build/copy rules from workspace/Makefile and Makefile.copy files
7. **Test**: `make clean && make setup && make build PLATFORM=<platform> && make system PLATFORM=<platform>`

## Relationship to Emulator Paks

The emulator pak templates in `workspace/all/paks/Emus/` remain largely unchanged because:

1. **High uniformity** - All emulator paks have identical structure
2. **Template efficiency** - One template generates ~200+ paks
3. **Core-based** - Variation is in libretro core, not pak structure

However, emulator pak metadata could move to individual pak directories:

```
paks/
├── GB.pak/
│   ├── pak.json      # Contains core info
│   ├── launch.sh.template
│   └── configs/
└── GBA.pak/
    ├── pak.json
    ├── launch.sh.template
    └── configs/
```

This is optional and can be evaluated after tool paks are migrated.

## Examples

### Example 1: Adding a New Shell-Only Pak

```bash
# Create pak directory
mkdir -p paks/MyTool.pak

# Create pak.json
cat > paks/MyTool.pak/pak.json << 'EOF'
{
  "name": "MyTool",
  "type": "tool",
  "platforms": ["miyoomini", "rg35xxplus", "tg5040"],
  "launch": "launch.sh"
}
EOF

# Create launch.sh
cat > paks/MyTool.pak/launch.sh << 'EOF'
#!/bin/sh
cd "$(dirname "$0")"
echo "Hello from $PLATFORM!"
EOF
chmod +x paks/MyTool.pak/launch.sh
```

### Example 2: Adding a Native Code Pak

```bash
# Create pak directory
mkdir -p paks/MyApp.pak/src

# Create pak.json
cat > paks/MyApp.pak/pak.json << 'EOF'
{
  "name": "MyApp",
  "type": "tool",
  "platforms": ["all"],
  "build": {
    "type": "native",
    "source": "src/",
    "makefile": "src/makefile"
  },
  "launch": "launch.sh"
}
EOF

# Create launch.sh
cat > paks/MyApp.pak/launch.sh << 'EOF'
#!/bin/sh
cd "$(dirname "$0")"
./myapp.elf
EOF

# Copy source and makefile (based on existing patterns)
cp workspace/all/clock/clock.c paks/MyApp.pak/src/myapp.c
cp workspace/all/clock/makefile paks/MyApp.pak/src/makefile
# Edit makefile to change TARGET = myapp
```

## Open Questions

1. **Emulator integration** - Should emulator paks fully migrate to this structure?
2. **Dependency caching** - Should downloaded binaries be cached across builds?
3. **Version pinning** - How to handle pak versioning for updates?
4. **External paks** - How to support third-party paks (like Jose's)?

## References

- Jose Gonzalez's Launcher paks: https://github.com/josegonzalez?tab=repositories&q=launcher
- Emulator pak templates: `workspace/all/paks/Emus/`
- Pak generation script: `scripts/generate-paks.sh`
