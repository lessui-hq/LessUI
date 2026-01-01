# RGB30 Platform

Platform implementation for the PowKiddy RGB30 running LessOS.

## Device

- **PowKiddy RGB30** - Rockchip RK3566 (Cortex-A55), 720x720 square display
- **OS**: LessOS (ROCKNIX-based)

## Hardware

| Feature | Specification |
|---------|---------------|
| Display | 720x720 (1:1 square), 16-bit RGB565, 2x UI scale |
| GPU | Mali-G52 (OpenGL ES 2.0) |
| HDMI | 1280x720 output with auto-detection |
| Input | D-pad, A/B/X/Y, L1/R1/L2/R2, dual analog sticks (L3/R3) |
| Audio | ALSA, headphone jack detection, HDMI audio |
| Storage | `/storage` (LessOS default) |

## Input

Uses `rocknix-singleadc-joypad` kernel driver via evdev.

| Combination | Function |
|-------------|----------|
| PLUS/MINUS | Volume |
| MENU + PLUS/MINUS | Brightness |
| POWER | Sleep/wake |

## Directory Structure

```
rgb30/
├── platform/       Platform definitions and render backend
├── keymon/         Button monitoring daemon (prebuilt)
├── libmsettings/   Settings library (volume, brightness, HDMI)
├── show/           Boot splash display
└── install/        Boot script
```

## Building

```bash
make PLATFORM=rgb30 shell
cd /root/workspace/rgb30
make
```

Uses the `rk3566` toolchain (shared via `toolchains.json`).

## Installation

LessOS handles boot via `/storage/lessos/init.sh`. LessUI installs to:

```
/storage/
├── .system/rgb30/     Platform binaries and paks
├── .userdata/rgb30/   User settings
└── Roms/              ROM files
```

## Platform Features

- **OpenGL ES**: Hardware-accelerated rendering (`HAS_OPENGLES 1`)
- **HDMI**: Auto-detection with video/audio routing
- **Overscan**: Supported (`PLAT_supportsOverscan`)
- **Effects**: Grid/line overlays via OpenGL

## Related

- Platform header: `platform/platform.h`
- Shared code: `../all/launcher/`, `../all/player/`
