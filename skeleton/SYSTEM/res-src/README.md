# Source Assets

This directory contains the master source files for all LessUI visual assets. These high-resolution sources are used to generate all variants (PNG and BMP) at different scales and aspect ratios.

## Source Files

### UI Assets
- **`assets.png`** - UI sprite sheet (icons, buttons, UI elements)
  - **Ideal resolution:** 512×512 (square, RGBA)
  - **Current:** 512×512 ✓
  - Used by launcher launcher for all UI rendering

### Boot Screens
These 4:3 images are displayed during system operations:

- **`installing.png`** - "Installing LessUI" boot screen
- **`updating.png`** - "Updating LessUI" boot screen
- **`bootlogo.png`** - LessUI splash screen/logo
- **`charging.png`** - Battery charging indicator

**Ideal resolution:** 960×720 (4:3 aspect ratio, RGB)
**Current:** 640×480 (adequate, but 960×720 preferred for highest quality on 3x platforms)

## Design Guidelines

### UI Sprite Sheet (`assets.png`)
- **Format:** PNG with alpha channel (RGBA)
- **Size:** 512×512 pixels
- **Content:** Grid of UI icons and elements
- **Style:** Simple, high-contrast, pixel-aligned graphics
- **Background:** Transparent

### Boot Screens
- **Format:** PNG, opaque (RGB, no alpha needed)
- **Aspect ratio:** 4:3 (width:height)
- **Ideal size:** 960×720 pixels
- **Minimum size:** 640×480 pixels (current)
- **Style:** Simple graphics on black background (#000000)
- **Content:**
  - Centered text/graphics
  - High contrast (white/light on black)
  - Minimal detail (displays during early boot, often low-quality rendering)

## Generating Variants

After updating any source file, regenerate all variants:

```bash
./scripts/generate-assets.sh
```

**Requirements:**
- **ImageMagick** (required) - `brew install imagemagick` - For PNG/BMP generation
- **ffmpeg** (required) - `brew install ffmpeg` - For 16-bit RGB565 BMPs

This script automatically creates:
- **UI sprites:** `assets@1x.png` through `assets@4x.png` (128px to 512px)
- **Boot screens (PNG):** `@1x`, `@2x`, `@3x` variants (320×240 to 960×720)
- **Boot screens (BMP):** Multiple format variants for different platforms:
  - `@1x.bmp` - 320×240 32-bit (gkdpixel)
  - `@1x-wide.bmp` - 480×272 32-bit (m17)
  - `@2x.bmp` - 640×480 32-bit (rg35xxplus, magicmini)
  - `@2x-16bit.bmp` - 640×480 16-bit RGB565 (rg35xx)
  - `@2x-rotated.bmp` - 480×640 32-bit (rg35xxplus rotated, magicmini)
  - `@2x-square.bmp` - 720×720 32-bit (rg35xxplus CubeXX)
  - `@2x-wide.bmp` - 720×480 32-bit (rg35xxplus RG34XX)

All generated files are placed in `skeleton/SYSTEM/res/` and should be committed to the repository.

## Workflow

1. **Edit source file** - Modify PNG in this directory using your image editor
2. **Regenerate variants** - Run `./scripts/generate-assets.sh`
3. **Review output** - Check generated files in `skeleton/SYSTEM/res/`
4. **Commit changes** - `git add skeleton/SYSTEM/res/ skeleton/SYSTEM/res-src/`

## Technical Notes

### Why Source + Generated?

This approach provides:
- **Single source of truth** - Update once, regenerate everywhere
- **No build dependencies** - Generated files are committed, builds don't need ImageMagick
- **Platform flexibility** - Platforms pick the variant they need (scale, aspect ratio, format)
- **Easy maintenance** - Designer edits one file, script handles transformations

### Image Transformations

The generation script handles:
- **Scaling** - Precise resize to target dimensions
- **Cropping** - Center-crop for non-4:3 displays
- **Rotation** - 90° rotation for portrait displays
- **Format conversion** - PNG → BMP with proper formats:
  - **32-bit BGRA** (most platforms) - ImageMagick with `-define bmp3:alpha=true`
  - **16-bit RGB565** (rg35xx) - ffmpeg with `-pix_fmt rgb565le`
  - **Top-down scanlines** (all BMPs) - Header patching with `scripts/bmp-topdown.sh`
- **Background handling** - Black background fill for aspect ratio mismatches

### Platform Usage

Platforms declaratively copy the variants they need:

```bash
# Example: rg35xxplus uses @2x variants with rotation/square/wide
ASSETS=../../skeleton/SYSTEM/res
cp $ASSETS/installing@2x.bmp installing.bmp
cp $ASSETS/installing@2x-rotated.bmp installing-r.bmp
cp $ASSETS/installing@2x-square.bmp installing-s.bmp
```

This keeps platform code simple and makes it easy to add new devices.
