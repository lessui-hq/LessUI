# Platform Variant System

## Overview

The platform variant system allows a single platform build to support multiple hardware devices that differ in screen resolution, battery monitoring method, or other hardware features. At runtime, the system detects which specific device is running and configures itself accordingly.

## Core Concept

**Platform** = Build target (e.g., `rg35xxplus`, `miyoomini`)
**Device** = Physical hardware model (e.g., "RG35XX H", "Mini Plus")
**Variant** = Hardware configuration shared by one or more devices (resolution, features)

Multiple devices can share a single variant configuration if they have identical hardware characteristics.

## Architecture

### Global State

```c
// Defined in platform_variant.h, instantiated in platform_variant.c
typedef struct {
    const char* platform;
    VariantType variant;
    const DeviceInfo* device;
    int screen_width;
    int screen_height;
    float screen_diagonal;
    int has_hdmi;
    int hdmi_active;
    uint32_t hw_features;
    void* platform_data;
} PlatformVariant;

extern PlatformVariant platform_variant;
```

This global struct is populated once during `PLAT_initVideo()` and accessed throughout the codebase.

### Platform-Specific Tables

Each platform with variants implements three static const arrays:

#### 1. Device Registry

Lists all known devices for this platform:

```c
static const DeviceInfo rg35xxplus_devices[] = {
    {.device_id = "rg28xx", .display_name = "RG28XX", .manufacturer = "Anbernic"},
    {.device_id = "rg35xxplus", .display_name = "RG35XX Plus", .manufacturer = "Anbernic"},
    {.device_id = "rgcubexx", .display_name = "RG CubeXX", .manufacturer = "Anbernic"},
    // ...
    {NULL, NULL, NULL} // Sentinel
};
```

#### 2. Variant Configuration Table

Defines hardware characteristics for each variant:

```c
static const VariantConfig rg35xxplus_variants[] = {
    {
        .variant = VARIANT_RG35XX_VGA,
        .screen_width = 640,
        .screen_height = 480,
        .screen_diagonal_default = 3.5f,
        .hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE
    },
    {
        .variant = VARIANT_RG35XX_SQUARE,
        .screen_width = 720,
        .screen_height = 720,
        .screen_diagonal_default = 3.95f,
        .hw_features = HW_FEATURE_NEON | HW_FEATURE_LID | HW_FEATURE_RUMBLE
    },
    {.variant = VARIANT_NONE} // Sentinel
};
```

#### 3. Device-to-Variant Mapping

**The key self-documenting table** - shows which devices share configurations:

```c
static const DeviceVariantMap rg35xxplus_device_map[] = {
    // 640x480 devices - all use VARIANT_RG35XX_VGA
    {"RG28xx",     VARIANT_RG35XX_VGA, &rg35xxplus_devices[0], 2.8f},
    {"RG35xxPlus", VARIANT_RG35XX_VGA, &rg35xxplus_devices[1], 3.5f},
    {"RG35xxH",    VARIANT_RG35XX_VGA, &rg35xxplus_devices[2], 3.5f},

    // 720x720 square device
    {"RGcubexx",   VARIANT_RG35XX_SQUARE, &rg35xxplus_devices[6], 3.95f},

    {NULL, VARIANT_NONE, NULL, 0.0f} // Sentinel
};
```

### Detection Function

Each platform implements `PLAT_detectVariant()`:

```c
void PLAT_detectVariant(PlatformVariant* v) {
    v->platform = PLATFORM;

    // Platform-specific detection (env var, file check, etc.)
    char* model = getenv("RGXX_MODEL");

    // Look up in mapping table
    const DeviceVariantMap* map = findDeviceInMap(model);

    // Apply configuration
    v->device = map->device;
    v->variant = map->variant;
    const VariantConfig* config = getVariantConfig(map->variant);
    v->screen_width = config->screen_width;
    v->screen_height = config->screen_height;
    // ... etc
}
```

## Hardware Feature Flags

```c
#define HW_FEATURE_NEON        (1 << 0)  // ARM NEON SIMD
#define HW_FEATURE_LID         (1 << 1)  // Lid/hall sensor
#define HW_FEATURE_RUMBLE      (1 << 2)  // Vibration motor
#define HW_FEATURE_PMIC        (1 << 3)  // Power management IC
#define HW_FEATURE_ANALOG      (1 << 4)  // Analog sticks
#define HW_FEATURE_VOLUME_HW   (1 << 5)  // Hardware volume buttons
```

Check at runtime:

```c
if (platform_variant.hw_features & HW_FEATURE_PMIC) {
    // Use PMIC-based battery monitoring
}
```

## Usage Patterns

### Accessing Variant Information

**Screen properties** (always use direct access):

```c
#define FIXED_WIDTH (platform_variant.screen_width)
#define FIXED_HEIGHT (platform_variant.screen_height)
#define SCREEN_DIAGONAL (platform_variant.screen_diagonal)
```

**Variant checks** (use `VARIANT_IS()` helper):

```c
// Check if specific variant
if (VARIANT_IS(VARIANT_TG5040_BRICK))
    SetRawBrightness(8);

// Use in ternaries
#define EDGE_PADDING (VARIANT_IS(VARIANT_MINI_PLUS) ? 10 : 5)

// Use in return values
int PLAT_supportsOverscan(void) {
    return VARIANT_IS(VARIANT_RG35XX_SQUARE);
}
```

**Feature checks** (use `HAS_FEATURE()` helper):

```c
// Check for hardware features
if (HAS_FEATURE(HW_FEATURE_PMIC))
    use_pmic_battery();

// Use in button mappings
#define BTN_MOD_VOLUME (HAS_FEATURE(HW_FEATURE_VOLUME_HW) ? BTN_NONE : BTN_SELECT)
```

### Before vs After

**Before (verbose checks):**

```c
if (platform_variant.variant == VARIANT_TG5040_BRICK)
    SetRawBrightness(8);

if (platform_variant.hw_features & HW_FEATURE_PMIC)
    use_pmic_battery();

#define BTN_MOD_VOLUME \
    ((platform_variant.hw_features & HW_FEATURE_VOLUME_HW) ? BTN_NONE : BTN_SELECT)
```

**After (clean helpers):**

```c
if (VARIANT_IS(VARIANT_TG5040_BRICK))
    SetRawBrightness(8);

if (HAS_FEATURE(HW_FEATURE_PMIC))
    use_pmic_battery();

#define BTN_MOD_VOLUME (HAS_FEATURE(HW_FEATURE_VOLUME_HW) ? BTN_NONE : BTN_SELECT)
```

## Detection Methods

**rg35xxplus**: Environment variable `RGXX_MODEL` + HDMI state check
**miyoomini**: File existence (`/customer/app/axp_test` for PMIC) + screen mode parsing + `MY_MODEL` env var
**tg5040**: Environment variable `DEVICE`

## Adding a New Device

To add a device to an **existing variant**:

1. Add to device registry:

   ```c
   {.device_id = "rg40xxv", .display_name = "RG40XX V", .manufacturer = "Anbernic"}
   ```

2. Add to device mapping:
   ```c
   {"RG40xxV", VARIANT_RG35XX_VGA, &rg35xxplus_devices[5], 4.0f}
   ```

Done! If it shares an existing variant, no other changes needed.

## Adding a New Variant

If a device has truly different hardware:

1. Define constant in `platform.h`:

   ```c
   #define VARIANT_NEW (VARIANT_PLATFORM_BASE + 3)
   ```

2. Add to variant config table:

   ```c
   {
     .variant = VARIANT_NEW,
     .screen_width = 800,
     .screen_height = 600,
     .screen_diagonal_default = 4.0f,
     .hw_features = HW_FEATURE_NEON
   }
   ```

3. Update device mapping to reference new variant

## Implementation Status

**Multi-variant platforms** (use full variant system):

- rg35xxplus - 9 devices, 3 variants
- miyoomini - 4 devices, 3 variants
- tg5040 - 2 devices, 2 variants

**Single-device platforms** (no variant detection needed):

- m17, magicmini, my282, my355, rgb30, rg35xx, trimuismart, zero28

## Files

- `workspace/all/common/platform_variant.h` - Core header
- `workspace/all/common/platform_variant.c` - Global instance
- `workspace/<platform>/platform/platform.h` - Variant definitions
- `workspace/<platform>/platform/platform.c` - Detection tables and implementation
