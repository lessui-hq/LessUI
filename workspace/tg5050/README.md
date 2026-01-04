# TG5050 (Trimui Smart Pro S)

Platform implementation for the TG5050 Trimui Smart Pro S handheld device.

## Overview

The TG5050 is a single-device platform with no variants, using the Allwinner A523 SoC.

**Important:** Despite sharing the same form factor as the Smart Pro (TG5040/T527), the Smart Pro S uses completely different hardware (A523 SoC) with different drivers and control interfaces.

## Hardware Specifications

### SoC
- **Processor**: Allwinner A523 (sun55iw3)
- **CPU**: 8x Cortex-A55 (dual cluster: cpu0-3, cpu4-7)
- **GPU**: Mali (supports OpenGL ES 3.2)

### Display
- **Resolution**: 1280x720 (HD widescreen)
- **Panel**: DSI
- **Color Depth**: 16-bit RGB565
- **UI Scale**: 2x standard (uses `assets@2x.png`)
- **Backlight**: sysfs control (NOT `/dev/disp` ioctl like TG5040)

### Input
- **D-Pad**: Up, Down, Left, Right (via HAT)
- **Face Buttons**: A, B, X, Y
- **Shoulder Buttons**: L1, R1
- **Analog Triggers**: L2, R2 (axis-based)
- **Analog Sticks**: Left stick (LX/LY) and Right stick (RX/RY)
- **L3/R3 Buttons**: Clickable analog sticks
- **System Buttons**:
  - SELECT and START buttons
  - MENU button
  - POWER button (HOME key, code 102)
  - PLUS/MINUS volume buttons

### Input Method
- **Primary**: SDL Joystick API (not keyboard events)
- **D-Pad**: Implemented via joystick HAT
- **Analog**: 6 analog axes (left/right sticks, L2/R2 triggers)
- **Evdev Codes**: Volume/power buttons only

### CPU & Performance
- 8x Cortex-A55 with dual-cluster architecture
- CPU scaling disabled (using schedutil governor)
- Dual policies: policy0 (cpu0-3), policy4 (cpu4-7)

### Power Management
- AXP2202 power management IC
- Battery monitoring
- Auto-sleep and power-off features

### Storage
- SD card mounted at `/mnt/SDCARD`

### Audio
- **Mixer Control**: `DAC Volume` via tinyalsa
- **Outputs**: HPOUT, SPK, LINEOUTL, LINEOUTR (all unmuted on init)
- **Speaker Mute**: `/sys/class/speaker/mute` sysfs
- **Headphone Detection**: `/sys/bus/platform/devices/singleadc-joypad/hp`
- **Volume Range**: 0-20 (raw: 0-100)

## Key Differences from TG5040

| Feature | TG5040 (T527) | TG5050 (A523) |
|---------|---------------|---------------|
| SoC | Allwinner T527 (A133P) | Allwinner A523 |
| CPU | 4x Cortex-A53 | 8x Cortex-A55 (dual cluster) |
| Backlight | `/dev/disp` ioctl | sysfs `/sys/class/backlight/backlight0/brightness` |
| Volume | `digital volume` control | `DAC Volume` control |
| Rumble GPIO | 227 | 236 |
| Speaker Mute | GPIO 243 | `/sys/class/speaker/mute` sysfs |
| CPU Scaling | userspace governor | schedutil governor (no-op in LessUI) |

## Directory Structure

```
tg5050/
├── platform/          Platform-specific hardware definitions
│   ├── platform.h     Button mappings, display specs
│   ├── platform.c     Hardware abstraction implementation
│   ├── Makefile.env   CPU architecture flags
│   └── Makefile.copy  Build system integration
├── libmsettings/      Settings library (volume, brightness)
│   └── msettings.c    A523-specific hardware controls
├── show/              Boot splash screen display (SDL2)
│   └── show.c         PNG image loader for install/update screens
├── install/           Installation and boot scripts
│   ├── boot.sh        Boot/install/update handler
│   └── update.sh      Post-install script
└── Makefile           Platform build orchestration
```

## Building

### Prerequisites
Uses TG5040 cross-compilation toolchain (shared).

### Build Commands

```bash
# Enter platform build environment
make PLATFORM=tg5050 shell

# Inside container: build platform components
cd /root/workspace/tg5050
make

# Build everything from host
make build PLATFORM=tg5050
make system PLATFORM=tg5050
```

## Installation

### Boot Process

1. Device boots and runs `.tmp_update/tg5050.sh` (boot.sh)
2. Remounts SD card as read-write
3. Sets CPU governor to schedutil
4. If `LessUI.7z` exists:
   - Disables LED animations
   - Displays install/update splash screen
   - Extracts `LessUI.7z` to SD card
   - Runs `.system/tg5050/bin/install.sh`
   - Reboots if fresh install
5. Launches LessUI via `.system/tg5050/paks/LessUI.pak/launch.sh`

## Platform-Specific Features

### Audio Configuration

The A523 requires different audio initialization than T527:

```bash
# Unmute all outputs
amixer sset 'HPOUT' unmute
amixer sset 'SPK' unmute
amixer sset 'LINEOUTL' unmute
amixer sset 'LINEOUTR' unmute

# Volume control via 'DAC Volume' (0-100%)
```

### Backlight Control

Uses standard sysfs interface (range 0-255):

```bash
echo 128 > /sys/class/backlight/backlight0/brightness
```

### Rumble Motor

GPIO 236 controls the rumble motor:

```bash
echo 1 > /sys/class/gpio/gpio236/value  # Enable
echo 0 > /sys/class/gpio/gpio236/value  # Disable
```

### CPU Frequency

CPU scaling is disabled for now due to dual-cluster architecture.
Using schedutil governor to let the kernel handle scaling.

```bash
echo schedutil > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
echo schedutil > /sys/devices/system/cpu/cpufreq/policy4/scaling_governor
```

### LED Control

```bash
/sys/class/led_anim/max_scale       # Global brightness
/sys/class/led_anim/effect_l        # Left joystick LED
/sys/class/led_anim/effect_r        # Right joystick LED
/sys/class/led_anim/effect_m        # Logo LED
```

## Known Issues / Quirks

### Hardware Quirks
1. **Dual-Cluster CPU**: The A55 dual-cluster architecture requires special handling for proper CPU scaling
2. **Different Audio Stack**: Uses `DAC Volume` instead of `digital volume`
3. **Sysfs Backlight**: Uses standard sysfs instead of `/dev/disp` ioctl

### Development Notes
1. **Shared Toolchain**: Uses TG5040 toolchain for cross-compilation
2. **No Variants**: Single device configuration (simplified from TG5040)
3. **CPU Scaling**: Currently disabled, uses schedutil governor

## Related Documentation

- Platform specification: `../../docs/tg5050-platform.md`
- TG5040 platform (similar form factor): `../tg5040/README.md`
- Main project docs: `../../README.md`
