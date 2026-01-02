# Retroid Platform

Platform implementation for Retroid Pocket devices running LessOS.

## Devices

| Device | Display | Resolution | Notes |
|--------|---------|------------|-------|
| **Pocket 5** | 5.5" AMOLED | 1920x1080 | Flagship model |
| **Pocket Flip 2** | 5.5" AMOLED | 1920x1080 | Clamshell design |
| **Pocket Mini V1** | 3.7" AMOLED | 1280x960 | 4:3 compact |
| **Pocket Mini V2** | 3.92" AMOLED | 1240x1080 | Near-square compact |

**OS**: LessOS (ROCKNIX-based)

## Hardware

| Feature | Specification |
|---------|---------------|
| SoC | Qualcomm Snapdragon 865 (SM8250) |
| CPU | 1×A77@2.84G, 3×A77@2.4G, 4×A55@1.8G |
| GPU | Adreno 650 (Vulkan, OpenGL ES 3.2) |
| RAM | 6-8GB LPDDR4x |
| Storage | 128GB UFS 3.1 + microSD |
| HDMI | USB-C DisplayPort (1080p output) |
| Input | D-pad, A/B/X/Y, L1/R1, analog L2/R2, dual hall-effect sticks (L3/R3) |
| Audio | ALSA, USB-C audio |
| Connectivity | WiFi 6, Bluetooth 5.1 |

## Input

Uses `retroid-pocket-gamepad` kernel driver via serdev/evdev.

| Combination | Function |
|-------------|----------|
| PLUS/MINUS | Volume |
| MENU + PLUS/MINUS | Brightness |
| POWER | Sleep/wake |
| HOME (MODE) | Menu |

## Directory Structure

```
retroid/
├── platform/       Platform definitions and render backend
├── keymon/         Button monitoring daemon (prebuilt)
├── libmsettings/   Settings library (volume, brightness, HDMI)
├── show/           Boot splash display
└── install/        Boot script
```

## Building

```bash
make PLATFORM=retroid shell
cd /root/workspace/retroid
make
```

Uses the `sm8250` toolchain (shared via `toolchains.json`).

## Installation

LessOS handles boot via standard ROCKNIX init. LessUI installs to:

```
/storage/
├── .system/retroid/     Platform binaries and paks
├── .userdata/retroid/   User settings
└── Roms/                ROM files
```

## Environment Variables

LessOS provides these environment variables:

| Variable | Description | Example |
|----------|-------------|---------|
| `LESSOS_PLATFORM` | SoC/platform family | `SM8250` |
| `LESSOS_DEVICE` | Device model | `Retroid Pocket 5` |
| `LESSOS_STORAGE` | Writable storage path | `/storage` |

## Platform Features

- **OpenGL ES**: Hardware-accelerated rendering via Adreno 650
- **Vulkan**: Full Vulkan support via Turnip driver
- **HDMI**: USB-C DisplayPort with auto-detection
- **Overscan**: Supported on Mini V2 (near-square aspect)
- **Effects**: Grid/line overlays via OpenGL
- **Rumble**: Haptic feedback via pmi8998_haptics

## Device Variants

The platform auto-detects device variant via `LESSOS_DEVICE`:

| Variant | Devices | Resolution | Aspect |
|---------|---------|------------|--------|
| `VARIANT_RETROID_FHD` | Pocket 5, Flip 2 | 1920x1080 | 16:9 |
| `VARIANT_RETROID_MINI_V1` | Pocket Mini V1 | 1280x960 | 4:3 |
| `VARIANT_RETROID_MINI_V2` | Pocket Mini V2 | 1240x1080 | ~31:27 |

## LessOS/ROCKNIX References

For platform implementation reference (in LessOS repository):

**Kernel & Drivers:**
- Retroid gamepad driver: `projects/ROCKNIX/devices/SM8250/patches/linux/0008-retroid-gamepad.patch`
- Device tree (common): `projects/ROCKNIX/devices/SM8250/patches/linux/0000-sm8250-retroidpocket-common.patch`
- Device tree (RP5): `projects/ROCKNIX/devices/SM8250/patches/linux/0002-sm8250-retroidpocket-rp5.patch`
- Device tree (Mini): `projects/ROCKNIX/devices/SM8250/patches/linux/0003-sm8250-retroidpocket-rpmini.patch`

**Input Configuration:**
- RetroArch joypad config: `packages/emulators/libretro/retroarch/retroarch-joypads/gamepads/Retroid Pocket Gamepad.cfg`
- SDL2 gamepad mapping: `packages/apps/gamecontrollerdb/config/gamecontrollerdb.txt`
- Input discovery script: `packages/sysutils/system-utils/sources/scripts/input_sense`
- UDEV rules: `devices/SM8250/filesystem/usr/lib/udev/rules.d/99-retroid-pocket.rules`

**Platform Configuration:**
- Device options: `devices/SM8250/options`
- Audio (ALSA UCM): `packages/audio/alsa-ucm-conf/patches/SM8250/0002_Add-Retroid-Pocket-SM8250-configuration.patch`

**Button Codes from Kernel Driver:**
- D-Pad: BTN_DPAD_UP/DOWN/LEFT/RIGHT (544-547)
- Face: BTN_SOUTH/EAST/WEST/NORTH (304,305,308,307)
- Shoulders: BTN_TL/TR (310,311), analog triggers ABS_HAT2X/Y
- Thumbsticks: BTN_THUMBL/THUMBR (317,318)
- System: BTN_MODE (316), BTN_BACK (158)

## Related

- Platform header: `platform/platform.h`
- Shared code: `../all/launcher/`, `../all/player/`
- LessOS repository: `~/Code/lessui-hq/LessOS`
- Upstream ROCKNIX: https://github.com/ROCKNIX/distribution
