# Analog Input Architecture

This document explains how LessUI handles analog stick input and the challenges for devices with limited controls.

## How Analog Input Works

LessUI has two separate input paths:

```
┌─────────────────────┐     ┌──────────────────┐
│   Config File       │     │   Hardware       │
│   (digital only)    │     │   (if present)   │
│                     │     │                  │
│  bind A = B         │     │  Analog stick    │
│  bind Up = UP       │     │  events          │
└─────────┬───────────┘     └────────┬─────────┘
          │                          │
          ▼                          ▼
   ┌──────────────┐          ┌──────────────┐
   │   buttons    │          │  pad.laxis   │
   │   bitmask    │          │  pad.raxis   │
   └──────┬───────┘          └──────┬───────┘
          │                          │
          └──────────┬───────────────┘
                     ▼
              input_state_callback()
                     │
                     ▼
                   Core
```

**Digital buttons** (d-pad, face buttons, triggers) flow through the config system. The `.cfg` files map core buttons to device buttons.

**Analog sticks** bypass the config system entirely. Hardware events write directly to `pad.laxis` and `pad.raxis`, which are returned to cores when they request `RETRO_DEVICE_ANALOG` input.

### Supported Config Mappings

The config file syntax only supports digital buttons:

```
bind <core_button> = <device_button>
```

Available device buttons:
```
UP, DOWN, LEFT, RIGHT, A, B, X, Y, START, SELECT, L1, R1, L2, R2, L3, R3
```

Plus modifier variants: `MENU+A`, `MENU+B`, etc.

There is no config syntax for analog stick mapping (e.g., no `L_STICK_UP`).

## Device Categories

### No Analog Sticks

**Devices:** miyoomini, trimuismart, rg35xx, my355, tg5040, zero28

These devices have no hardware analog sticks. The `pad.laxis` and `pad.raxis` values remain at zero, meaning cores receive no analog input.

| System | Impact |
|--------|--------|
| N64 | Unplayable - analog stick required for 3D games |
| PSP | Unplayable - analog nub required for many games |
| Dreamcast | Degraded - d-pad works but analog preferred |
| Atari 5200 | May have issues - original used analog joystick |
| All others | Fine - d-pad based systems |

### One Analog Stick

**Devices:** rgb30, my282

These devices have a single (left) analog stick. The left stick works automatically for cores that need it.

| System | Impact |
|--------|--------|
| N64 | Works - left stick maps to N64 analog |
| PSP | Works - left stick maps to PSP nub |
| Dreamcast | Partial - left stick works, no right stick |
| PS (DualShock) | Partial - only left stick available |

### Two Analog Sticks

**Devices:** rg35xxplus, m17, magicmini

These devices have full analog capability. Both sticks pass through to cores automatically.

| System | Impact |
|--------|--------|
| N64 | Full support |
| PSP | Full support |
| Dreamcast | Full support |
| PS (DualShock) | Full support |

## The Problem

The current architecture assumes hardware capability matches core requirements. There is no translation layer.

- **Have the hardware?** Analog sticks automatically work. No config needed.
- **Missing the hardware?** Cores receive zeros. No workaround exists.

This means N64 and PSP are effectively broken on devices without analog sticks.

## Proposed Solution

Add a d-pad to analog emulation option:

```
dpad_to_analog = left    # d-pad simulates left stick
dpad_to_analog = right   # d-pad simulates right stick
dpad_to_analog = none    # default, current behavior
```

When enabled, `input_state_callback` would return full-deflection analog values (±32767) when d-pad directions are pressed.

This provides digital-only control (no gradual movement) but makes analog-dependent games **playable** rather than broken.

### Implementation Location

The change would be in `workspace/all/player/player.c` in the `input_state_callback` function, which currently returns raw `pad.laxis`/`pad.raxis` values for `RETRO_DEVICE_ANALOG` requests.

## Platform Analog Support Reference

Platforms define analog axes in their `platform.h`:

| Platform | AXIS_LX/LY | AXIS_RX/RY | Analog Support |
|----------|------------|------------|----------------|
| miyoomini | AXIS_NA | AXIS_NA | None |
| trimuismart | AXIS_NA | AXIS_NA | None |
| rg35xx | AXIS_NA | AXIS_NA | None |
| rg35xxplus | Defined | Defined | 2 sticks |
| my355 | Defined | Defined | Hardware present but not wired |
| tg5040 | Defined | Defined | Hardware present but not wired |
| zero28 | Defined | Defined | Hardware present but not wired |
| rgb30 | Defined | Defined | 1 stick (left only in practice) |
| m17 | Defined | Defined | 2 sticks (currently commented out) |
| my282 | Defined | AXIS_NA | 1 stick |
| magicmini | Defined | Defined | 2 sticks |

`AXIS_NA = -1` means the axis is not available. When an axis equals `AXIS_NA`, the comparison in `api.c` never matches, so `pad.laxis`/`pad.raxis` stay at zero.
