# TG5050 Platform (Trimui Smart Pro S)

This document describes the hardware and software requirements for supporting the Trimui Smart Pro S (TG5050) device.

## Overview

| Property  | Value                                        |
| --------- | -------------------------------------------- |
| Device    | Trimui Smart Pro S                           |
| Model ID  | TG5050                                       |
| SoC       | Allwinner A523 (sun55iw3)                    |
| CPU       | 8x Cortex-A55 (dual cluster: cpu0-3, cpu4-7) |
| Display   | 1280x720, DSI panel                          |
| Toolchain | `tg5040` (shared with Smart Pro)             |

**Important:** Despite sharing the same form factor and toolchain as the Smart Pro (TG5040/T527), the Smart Pro S uses completely different hardware (A523 SoC) with different drivers and control interfaces.

## Hardware Detection

### Device Identification

The device can be identified by checking the MainUI binary:

```bash
# Check for TG5050
if strings /usr/trimui/bin/MainUI | grep -q "TG5050"; then
    DEVICE="tg5050"
fi
```

Or by reading the hardware serial:

```bash
cat /sys/class/sunxi_info/sys_info | grep hwserial
# Returns: TG5050XXXXXXXXXX
```

### SoC Identification

```bash
cat /sys/firmware/devicetree/base/compatible
# Returns: allwinner,a523 arm,sun55iw3p1
```

## Display

### Backlight Control

Uses standard sysfs backlight interface (NOT `/dev/disp` ioctl):

```bash
# Path
/sys/class/backlight/backlight0/brightness

# Range: 0-255 (stock OS clamps to 10-220)
echo 128 > /sys/class/backlight/backlight0/brightness
```

### Display Enhancement (optional)

```bash
/sys/devices/virtual/disp/disp/attr/enhance_contrast    # 0-100
/sys/devices/virtual/disp/disp/attr/enhance_saturation  # 0-100
/sys/devices/virtual/disp/disp/attr/enhance_bright      # 0-100 (exposure)
/sys/devices/virtual/disp/disp/attr/color_temperature   # color temp adjustment
```

### Screen Rotation

DSI panel rotation (if needed):

```bash
/sys/class/drm/card0-DSI-1/rotate
```

## Audio

### ALSA Mixer Controls

The A523 uses different mixer control names than T527:

| Control      | Purpose                                                    |
| ------------ | ---------------------------------------------------------- |
| `DAC Volume` | Main volume control (use tinyalsa `mixer_ctl_set_percent`) |
| `HPOUT`      | Headphone output (unmute on init)                          |
| `SPK`        | Speaker output (unmute on init)                            |
| `LINEOUTL`   | Line out left (unmute on init)                             |
| `LINEOUTR`   | Line out right (unmute on init)                            |

### Speaker Mute

Hardware speaker mute via sysfs:

```bash
# Mute speaker (also stops hissing)
echo 1 > /sys/class/speaker/mute

# Unmute speaker
echo 0 > /sys/class/speaker/mute
```

### Initialization Sequence

```bash
# Unmute all outputs on init
amixer sset 'HPOUT' unmute
amixer sset 'SPK' unmute
amixer sset 'LINEOUTL' unmute
amixer sset 'LINEOUTR' unmute
```

### Volume Control (tinyalsa)

```c
#include <tinyalsa/mixer.h>

void SetRawVolume(int val) {  // 0-100
    struct mixer *mixer = mixer_open(0);
    if (!mixer) return;

    struct mixer_ctl *ctl = mixer_get_ctl_by_name(mixer, "DAC Volume");
    if (ctl) {
        mixer_ctl_set_percent(ctl, 0, val);
    }
    mixer_close(mixer);

    // Full mute requires sysfs
    putInt("/sys/class/speaker/mute", val == 0 ? 1 : 0);
}
```

### Headphone Jack Detection

```bash
# TODO: Verify path on actual hardware
/sys/bus/platform/devices/singleadc-joypad/hp
```

## CPU Frequency Scaling

> **Note:** CPU scaling is **disabled (no-op)** for initial tg5050 implementation. The dual-cluster architecture requires a broader overhaul to properly support big/little SoC configurations. Use `schedutil` governor and let the kernel handle scaling for now.

### Hardware Reference (for future implementation)

The A523 has two CPU clusters with separate frequency policies:

| Cluster | CPUs | Policy Path                                | Frequency Range |
| ------- | ---- | ------------------------------------------ | --------------- |
| Little  | 0-3  | `/sys/devices/system/cpu/cpufreq/policy0/` | 408-1416 MHz    |
| Big     | 4-7  | `/sys/devices/system/cpu/cpufreq/policy4/` | 408-2160 MHz    |

Available frequencies (big cluster):

```
408000 672000 840000 1008000 1200000 1344000 1488000 1584000 1680000 1800000 1992000 2088000 2160000
```

### Current Implementation (No-Op)

```c
void PLAT_setCPUSpeed(int speed) {
    (void)speed;  // No-op for now - using schedutil governor
}

int PLAT_getAvailableCPUFrequencies(int* frequencies, int max_count) {
    (void)frequencies;
    (void)max_count;
    return 0;  // Return 0 to disable auto-CPU scaling
}
```

## Input

### Rumble/Vibration

Uses GPIO 236 (different from T527's GPIO 227):

```bash
# Enable rumble
echo 1 > /sys/class/gpio/gpio236/value

# Disable rumble
echo 0 > /sys/class/gpio/gpio236/value

# Set rumble intensity (optional)
echo <0-65535> > /sys/class/motor/level
```

### Buttons

Device has L3/R3 (stick click) buttons. Uses `trimui_inputd` for turbo and input remapping.

## Power Management

### Battery Status

```bash
# Capacity (0-100)
cat /sys/class/power_supply/axp2202-battery/capacity

# Charging status
cat /sys/class/power_supply/axp2202-usb/online        # 1 = charger connected
cat /sys/class/power_supply/axp2202-battery/time_to_full_now  # >0 = charging
```

### CPU Temperature

```bash
cat /sys/devices/virtual/thermal/thermal_zone0/temp
# Returns millidegrees, divide by 1000 for Celsius
```

### Fan Control

```bash
# Set fan speed (0-31, or use thermal daemon for auto)
echo <0-31> > /sys/class/thermal/cooling_device0/cur_state
```

## Graphics

### OpenGL ES

Request GLES 3.2 context:

```c
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
```

### SDL Hints

```c
SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
```

## Networking

### WiFi Module

Uses `aic8800_fdrv.ko` driver:

```bash
modprobe aic8800_fdrv.ko
/etc/wifi/wifi_init.sh start
```

### Bluetooth

Uses `aic8800_btlpm.ko` driver:

```bash
modprobe aic8800_btlpm.ko
hciattach -n ttyAS1 aic &
/etc/bluetooth/bluetoothd start
```

## LED Control

### LED Animation Paths

```bash
/sys/class/led_anim/max_scale           # Global brightness
/sys/class/led_anim/effect_l            # Left joystick LED
/sys/class/led_anim/effect_r            # Right joystick LED
/sys/class/led_anim/effect_m            # Logo LED
/sys/class/led_anim/effect_duration_*   # Animation speed
/sys/class/led_anim/effect_rgb_hex_*    # Color (hex)
```

## Platform Constants

```c
#define PLATFORM "tg5050"
#define SDCARD_PATH "/mnt/SDCARD"
#define FIXED_WIDTH 1280
#define FIXED_HEIGHT 720
#define FIXED_PITCH (FIXED_WIDTH * 4)

// Uses tg5040 toolchain
// #define TOOLCHAIN "tg5040"
```

## Implementation Checklist

- [ ] Create `workspace/tg5050/` directory structure
- [ ] Create `platform/platform.h` with constants
- [ ] Create `platform/platform.c` with hardware abstraction
- [ ] Create `libmsettings/msettings.c` with A523-specific controls
- [ ] Create `skeleton/SYSTEM/tg5050/` with system files
- [ ] Create `workspace/all/paks/LessUI/platforms/tg5050/` init scripts
- [ ] Add tg5050 to `toolchains.json` (uses tg5040 toolchain)
- [ ] Add tg5050 to Makefile PLATFORMS
- [ ] Update pak platform lists

## References

- NextUI tg5050 branch: https://github.com/shauninman/NextUI (tg5050 branch)
- System report: `/Volumes/LESSUI_DEV/system_report_trimuismartpro_*.md`
