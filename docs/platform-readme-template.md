# Platform README Template

This document defines the ideal structure for platform README files in LessUI. Use this template when creating or updating platform documentation.

## Template Structure

```markdown
# {Platform Name} ({Device Names})

Platform implementation for the {Device Name(s)} retro handheld device(s).

> [!WARNING]
> **This platform is deprecated and will be removed in a future LessUI release.**
>
> **Reason**: {Brief explanation of why this platform is deprecated}
>
> While the platform will continue to work with current LessUI releases, it will not receive new features or platform-specific bug fixes.

(Include deprecation notice only if applicable)

## Hardware Specifications

### Display
- **Resolution**: {Width}x{Height} ({Description, e.g., VGA, HD, square})
- **HDMI Output**: {Resolution} (if applicable, note "runtime detection" if supported)
- **Color Depth**: {Bit depth and format, e.g., 16-bit RGB565}
- **UI Scale**: {Scale factor}x (uses `assets{@scale}.png`)
- **Orientation**: {Note rotation if applicable}
- **Variant Resolutions**: {If multiple variants, list each}

### Input
- **D-Pad**: Up, Down, Left, Right {(via HAT if applicable)}
- **Face Buttons**: A, B, X, Y {note any label swap quirks}
- **Shoulder Buttons**: L1, R1 {, L2, R2 if present}
- **Analog Sticks**: {L3, R3 if present, note if clickable}
- **System Buttons**:
  - MENU button
  - POWER button {(if present)}
  - SELECT and START buttons
  - {PLUS/MINUS volume buttons if dedicated hardware buttons}

### Input Method
- **Primary**: {SDL Keyboard / SDL Joystick API / Hybrid}
- **Secondary**: {Evdev codes for specific buttons if applicable}
- **D-Pad Implementation**: {HAT / discrete buttons / keyboard}
- **Notable**: {Any quirks like Japanese key mappings}

### CPU & Performance
- {CPU type, e.g., ARM Cortex-A53}
- {NEON SIMD support: Yes/No}
- {Overclocking support: Yes/No with frequency profiles if applicable}
- {CPU frequency profiles with speeds if relevant}

### Power Management
- **Battery Monitoring**: {Method - PMIC, ADC, sysfs path}
- **Charging Detection**: {Yes/No, method}
- **Power Button**: {Sleep/wake behavior}
- {Auto-sleep features if present}

### Storage
- SD card mounted at `{mount path}`
- {Note if dual SD card setup}

### Audio
- **Volume Range**: {User scale, e.g., 0-20}
- **Mute Value**: {Raw value for mute}
- **Headphone Jack**: {Detection method if applicable}
- **Notable**: {Any audio quirks like inverted scale, channel swap}

## Platform Variants

(Include this section only if platform supports multiple hardware variants)

This platform supports {N} hardware variants detected at runtime:

### {Variant 1 Name}
- {Key differentiating specs}
- {Detection method}
- {Variant-specific UI parameters}

### {Variant 2 Name}
- {Key differentiating specs}
- {Detection method}
- {Variant-specific UI parameters}

**Detection**: {Explain how variants are detected at runtime}

## Directory Structure

```
{platform}/
├── platform/          Platform-specific hardware definitions
│   └── platform.h     Button mappings, display specs, paths
{If platform.c exists:}
│   └── platform.c     Platform implementation ({line count} lines)
├── keymon/            Hardware button monitoring daemon
│   └── keymon.c       Volume/brightness control {+ other features}
├── libmsettings/      Settings library (volume, brightness, {other})
│   ├── msettings.c    {Brief description}
│   └── msettings.h    Settings API header
{If special utilities exist:}
├── {utility}/         {Description}
│   └── {files}        {Purpose}
├── show/              Boot splash screen display utility
│   └── show.c         {SDL/SDL2}-based image display {with rotation if applicable}
├── install/           Installation assets and boot script
│   ├── boot.sh        Boot/update handler
│   ├── installing.{ext}  {Resolution} boot splash for fresh install
│   └── updating.{ext}    {Resolution} boot splash for updates
├── cores/             Libretro cores
└── other/             Third-party dependencies
    └── {dependencies with descriptions}
```

## Input System

The {Platform} uses a **{architecture description}**:

1. **{Primary method}**: {Description}
2. **{Secondary method}**: {Description}
3. **Notable**: {Any special considerations}

### Button Mappings

(Include mapping table if platform uses non-standard mappings)

| Physical Button | {Mapping Type} | Notes |
|-----------------|----------------|-------|
| {Button} | {Code/Index} | {Notes} |

### Button Combinations

| Combination | Function |
|-------------|----------|
| PLUS | Increase volume |
| MINUS | Decrease volume |
| {MENU/START} + {L1/R1 or PLUS/MINUS} | {Brightness control} |
| {Button combo} | {Shutdown if applicable} |
| POWER | Sleep/wake device |
| X (in launcher) | Resume from save state |

## Building

### Prerequisites
Requires Docker with {platform} cross-compilation toolchain.

### Build Commands

```bash
# Enter platform build environment
make PLATFORM={platform} shell

# Inside container: build all platform components
cd /root/workspace/{platform}
make

# This builds:
# - {component list with descriptions}
```

### Dependencies
The platform automatically clones required dependencies on first build:
- **{Dependency}**: `{repo URL}` (branch: `{branch}`)

## Installation

### File System Layout

LessUI installs to the SD card with the following structure:

```
{SD_CARD_PATH}/
├── .system/
│   ├── {platform}/          Platform-specific binaries
│   │   ├── bin/            Utilities ({list})
│   │   │   └── install.sh  Post-update installation script
│   │   └── paks/           Applications and emulators
│   │       └── LessUI.pak/  Main launcher
│   └── res/                Shared UI assets
│       ├── assets{@scale}.png  UI sprite sheet
│       └── InterTight-Bold.ttf
├── .tmp_update/            Update staging area
│   └── {platform}/         Platform boot components
├── .userdata/              User settings and saves
│   └── {platform}/         Platform-specific settings
├── Roms/                   ROM files organized by system
└── LessUI.zip               Update package (if present)
```

### Boot Process

1. Device boots and runs {boot script path}
2. {Ordered list of boot steps}
3. If `LessUI.zip` exists:
   - {Update steps}
4. Launch LessUI via {launch path}
5. {Exit behavior - reboot/poweroff}

### Update Process

To update LessUI on device:
1. Place `LessUI.zip` in {location}
2. Reboot device
3. Boot script auto-detects ZIP and performs update
4. ZIP is deleted after successful extraction

## Platform-Specific Features

### {Feature Name}

{Description of feature and implementation details}

```c
// Example code snippet if helpful
{code}
```

{Include relevant paths, values, or technical details}

(Repeat for each notable platform feature such as:)
- HDMI detection and switching
- Brightness/volume control implementation
- Headphone jack detection
- CPU frequency control
- Special hardware features (rumble, LEDs, lid sensor)
- Display rotation
- Visual effects
- Settings persistence

## UI Layout Adjustments

(Include if platform has variant-specific or display-specific UI adjustments)

| Parameter | {Variant/Mode 1} | {Variant/Mode 2} |
|-----------|------------------|------------------|
| `FIXED_WIDTH` | {value} | {value} |
| `FIXED_HEIGHT` | {value} | {value} |
| `MAIN_ROW_COUNT` | {value} | {value} |
| `PADDING` | {value} | {value} |
| `FIXED_SCALE` | {value} | {value} |

## Included Tools

### Files.pak
{File manager name}-based file manager with:
- File operations (copy, cut, paste, delete, rename)
- Directory navigation
- {Other features}

(List other included tools with brief descriptions)

## Known Issues / Quirks

### Hardware Quirks
1. **{Issue}**: {Description and workaround if applicable}

### Development Notes
1. **{Note}**: {Description}
2. **NEON Support**: {HAS_NEON status and implications}

### Input Limitations
- {Limitations list}

### Volume/Brightness Quirks
- {Quirks list}

## Testing

When testing changes:
1. {Test scenarios relevant to platform features}
2. Verify volume/brightness controls
3. Test boot process with and without LessUI.zip
4. {Variant-specific tests if applicable}
5. {Platform-specific tests for unique features}

## Related Documentation

- Main project docs: `../../README.md`
- Platform abstraction: `../../all/common/defines.h`
- Shared code: `../../all/minui/minui.c` (launcher), `../../all/minarch/minarch.c` (libretro frontend)
- Build system: `../../Makefile` (host), `./makefile` (platform)
- Platform header: `./platform/platform.h` (all hardware definitions)
{Add additional platform-specific documentation references}

## Maintainer Notes

{Brief summary of what makes this platform notable:}
- {Unique characteristics}
- {Reference implementation value}
- {Complexity considerations}

{Special warnings or considerations for maintainers:}
- {Areas requiring extra testing}
- {Dependencies between features}
- {Potential gotchas}
```

---

## Section Guidelines

### Hardware Specifications

**Purpose**: Provide quick reference for device capabilities.

**Guidelines**:
- Use consistent formatting for resolution: `{Width}x{Height}`
- Always note color depth and UI scale
- List all physical buttons available
- Specify input method clearly (Keyboard vs Joystick vs Hybrid)
- Note NEON support for performance-critical code decisions

### Platform Variants

**Purpose**: Document hardware variants supported by single platform binary.

**Guidelines**:
- Only include if platform has runtime-detected variants
- Clearly explain detection mechanism
- Document variant-specific UI parameters
- List resolution and aspect ratio differences

### Directory Structure

**Purpose**: Help developers navigate platform codebase.

**Guidelines**:
- Include all significant directories
- Note line counts for complex implementations
- List third-party dependencies with their purpose

### Input System

**Purpose**: Document complete input handling architecture.

**Guidelines**:
- Clearly state primary input method (SDL Keyboard, Joystick, or Hybrid)
- Include button mapping tables for non-standard platforms
- Document all button combinations
- Note any evdev codes used by keymon

### Installation

**Purpose**: Document file system layout and boot process.

**Guidelines**:
- Show complete file system tree
- Number boot process steps clearly
- Document update process separately
- Note any migration paths from older platforms

### Platform-Specific Features

**Purpose**: Document unique hardware features and their implementations.

**Guidelines**:
- Include code snippets where helpful
- Document sysfs paths and ioctl codes
- Explain non-linear mappings (brightness curves, etc.)
- Note runtime detection mechanisms

### Known Issues / Quirks

**Purpose**: Warn developers and users about platform-specific gotchas.

**Guidelines**:
- Separate hardware quirks from development notes
- Include workarounds where applicable
- Note deprecated or broken features

### Testing

**Purpose**: Ensure platform changes are properly validated.

**Guidelines**:
- List scenarios specific to platform features
- Include variant-specific tests
- Note HDMI/headphone tests if applicable
- Cover both install and update paths

### Maintainer Notes

**Purpose**: Provide context for platform maintenance decisions.

**Guidelines**:
- Highlight unique aspects of platform
- Note reference value for other platforms
- Warn about high-risk areas
- Document complexity considerations

---

## Current Platform README Status

| Platform | Status | Variants | Key Features |
|----------|--------|----------|--------------|
| miyoomini | Complete | Mini, Mini Plus, 560p | Reference implementation, dual-variant |
| trimuismart | Complete | None | DE2 display engine, ION memory |
| rg35xx | Complete | None | Chroot environment, Japanese keys |
| rg35xxplus | Complete | Plus, H, SP | Runtime adaptation, HDMI |
| tg5040 | Complete | Standard, Brick | HAT D-pad, analog triggers |
| rgb30 | Complete | None | Square display, HDMI |
| m17 | Complete (Deprecated) | None | Hybrid input, CPU pinning |
| my282 | Complete | None | Joystick input, analog sticks |
| my355 | Complete | None | Clamshell, HDMI hotplug, effects |
| magicmini | Complete (Deprecated) | None | Dual SD cards |
| zero28 | Complete | None | SDL2, visual effects, rotation |

---

## Checklist for New Platforms

When adding a new platform, ensure README includes:

- [ ] Hardware specifications (all subsections)
- [ ] Variant documentation (if applicable)
- [ ] Complete directory structure
- [ ] Input system documentation with mappings
- [ ] Build commands and dependencies
- [ ] File system layout
- [ ] Boot process steps
- [ ] Update process
- [ ] All platform-specific features
- [ ] UI layout adjustments (if variant/display-dependent)
- [ ] Known issues and quirks
- [ ] Testing scenarios
- [ ] Maintainer notes

## Checklist for Updating Existing READMEs

When updating existing platform documentation:

- [ ] Verify hardware specifications are current
- [ ] Update directory structure if changed
- [ ] Check button mappings match platform.h
- [ ] Verify sysfs paths and ioctl codes
- [ ] Update build dependencies
- [ ] Review known issues for resolved items
- [ ] Add new features/quirks discovered
- [ ] Verify testing scenarios cover new functionality
