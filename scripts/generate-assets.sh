#!/bin/bash
# Generate all UI and boot asset variants from high-res source PNGs
#
# This script creates PNG and BMP variants at different scales and aspect ratios
# from the master source files in skeleton/SYSTEM/res-src/
#
# Source files:
#   - assets.png (512×512 UI sprite sheet)
#   - installing/updating/bootlogo/charging.png (640×480 boot screens)
#
# Run this script when updating assets, then commit the generated files.
# Normal builds don't need ImageMagick - they use the pre-generated variants.

set -e

SRC=skeleton/SYSTEM/res-src
OUT=skeleton/SYSTEM/res

echo "Generating all assets from sources..."
echo "Source: $SRC"
echo "Output: $OUT"
echo ""

# Check for ImageMagick
if ! command -v magick &> /dev/null && ! command -v convert &> /dev/null; then
    echo "ERROR: ImageMagick is required to generate assets"
    echo "Install with: brew install imagemagick"
    exit 1
fi

# Check for ffmpeg (needed for 16-bit RGB565 BMPs)
if ! command -v ffmpeg &> /dev/null; then
    echo "ERROR: ffmpeg is required to generate 16-bit boot assets"
    echo "Install with: brew install ffmpeg"
    exit 1
fi

# pngquant removed - caused color degradation on boot screens
# PNG files are small enough without compression

# Use magick if available (v7+), fallback to convert (v6)
if command -v magick &> /dev/null; then
    MAGICK="magick"
else
    MAGICK="convert"
fi

# Generate variants for each asset type
for ASSET in installing updating bootlogo charging; do
    echo "Processing $ASSET..."

    # PNG variants (for show.elf platforms - simple scaling, maintain 4:3)
    # IMPORTANT: png:color-type=2 forces RGB (no palette), prevents indexed color
    echo "  Generating PNG variants..."
    $MAGICK $SRC/$ASSET.png -resize 320x240! -define png:color-type=2 $OUT/$ASSET@1x.png
    $MAGICK $SRC/$ASSET.png -resize 640x480! -define png:color-type=2 $OUT/$ASSET@2x.png
    $MAGICK $SRC/$ASSET.png -resize 960x720! -define png:color-type=2 $OUT/$ASSET@3x.png

    # BMP variants (for dd platforms - various bit depths, top-down scanlines)
    # NOTE: Generate normally, then negate height header to make top-down
    echo "  Generating BMP variants..."

    # @1x: 320×240 (4:3) - gkdpixel (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 320x240! \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@1x.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@1x.bmp $OUT/$ASSET@1x.bmp.tmp && mv $OUT/$ASSET@1x.bmp.tmp $OUT/$ASSET@1x.bmp

    # @1x-wide: 480×272 (16:9) - m17 (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 480x360! -gravity center \
        -crop 480x272+0+0 +repage -background black -flatten \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@1x-wide.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@1x-wide.bmp $OUT/$ASSET@1x-wide.bmp.tmp && mv $OUT/$ASSET@1x-wide.bmp.tmp $OUT/$ASSET@1x-wide.bmp

    # @2x: 640×480 (4:3) - rg35xxplus standard, magicmini (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 640x480! \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@2x.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@2x.bmp $OUT/$ASSET@2x.bmp.tmp && mv $OUT/$ASSET@2x.bmp.tmp $OUT/$ASSET@2x.bmp

    # @2x-16bit: 640×480 (4:3) - rg35xx (16-bit RGB565)
    # Note: ImageMagick can't create 16-bit BMPs, use ffmpeg instead
    ffmpeg -i $SRC/$ASSET.png -vf scale=640:480 -pix_fmt rgb565le -y $OUT/$ASSET@2x-16bit.bmp 2>/dev/null
    ./scripts/bmp-topdown.sh $OUT/$ASSET@2x-16bit.bmp $OUT/$ASSET@2x-16bit.bmp.tmp && mv $OUT/$ASSET@2x-16bit.bmp.tmp $OUT/$ASSET@2x-16bit.bmp

    # @2x-rotated: 480×640 (portrait) - rg35xxplus 28xx (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 640x480! -rotate 90 \
        -gravity center -crop 480x640+0+0 +repage \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@2x-rotated.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@2x-rotated.bmp $OUT/$ASSET@2x-rotated.bmp.tmp && mv $OUT/$ASSET@2x-rotated.bmp.tmp $OUT/$ASSET@2x-rotated.bmp

    # @2x-square: 720×720 (1:1) - rg35xxplus cubexx (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 960x720! -gravity center \
        -crop 720x720+0+0 +repage \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@2x-square.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@2x-square.bmp $OUT/$ASSET@2x-square.bmp.tmp && mv $OUT/$ASSET@2x-square.bmp.tmp $OUT/$ASSET@2x-square.bmp

    # @2x-wide: 720×480 (3:2) - rg35xxplus 34xx (32-bit BGRA)
    $MAGICK $SRC/$ASSET.png -resize 720x540! -gravity center \
        -crop 720x480+0+0 +repage -background black -flatten \
        -define bmp3:alpha=true BMP3:$OUT/$ASSET@2x-wide.bmp
    ./scripts/bmp-topdown.sh $OUT/$ASSET@2x-wide.bmp $OUT/$ASSET@2x-wide.bmp.tmp && mv $OUT/$ASSET@2x-wide.bmp.tmp $OUT/$ASSET@2x-wide.bmp

    echo "  ✓ Generated all variants for $ASSET"
    echo ""
done

# Generate UI sprite sheet variants (PNG only, square, simple scaling)
echo "Processing assets (UI sprite sheet)..."
echo "  Generating PNG variants..."
$MAGICK $SRC/assets.png -resize 128x128! $OUT/assets@1x.png
$MAGICK $SRC/assets.png -resize 256x256! $OUT/assets@2x.png
$MAGICK $SRC/assets.png -resize 384x384! $OUT/assets@3x.png
$MAGICK $SRC/assets.png -resize 512x512! $OUT/assets@4x.png
echo "  ✓ Generated all variants for assets"
echo ""

# Generate platform-specific bootlogo variants from square logo
# These are used by Bootlogo.pak tools to update boot partition logos
# Use Point filter for pixel-art scaling (nearest-neighbor, no blur)
echo "Processing bootlogo variants for Bootlogo.pak tools..."

# m17: 100×100, 24-bit, bottom-up, flipped, with 20px padding
# Resize to 60x60 to leave 20px padding on all sides
$MAGICK $SRC/logo.png -filter Point -resize 60x60! -background black -gravity center \
    -extent 100x100 -flip -type TrueColor -define bmp:format=bmp3 \
    skeleton/EXTRAS/Tools/m17/Bootlogo.pak/logo.bmp
echo "  ✓ m17 logo.bmp (100×100, 24-bit, flipped, 20px padding)"

# my282: 480×640 portrait, 32-bit, bottom-up, rotated 90° CCW (use full bootlogo)
$MAGICK $SRC/bootlogo.png -filter Point -resize 640x480! -rotate -90 \
    -background black -gravity center -extent 480x640 \
    -define bmp3:alpha=true BMP3:skeleton/EXTRAS/Tools/my282/Bootlogo.pak/bootlogo.bmp
echo "  ✓ my282 bootlogo.bmp (480×640, 32-bit, 90° CCW)"

# my355: 640×480, 24-bit, bottom-up (use full bootlogo)
$MAGICK $SRC/bootlogo.png -filter Point -resize 640x480! -background black -gravity center \
    -type TrueColor -define bmp:format=bmp3 \
    skeleton/EXTRAS/Tools/my355/Bootlogo.pak/payload/logo.bmp
echo "  ✓ my355 logo.bmp (640×480, 24-bit)"

# tg5040: 128×128, 32-bit, bottom-up
$MAGICK $SRC/logo.png -filter Point -resize 128x128! -background black -gravity center \
    -extent 128x128 -define bmp3:alpha=true BMP3:skeleton/EXTRAS/Tools/tg5040/Bootlogo.pak/bootlogo.bmp
echo "  ✓ tg5040 bootlogo.bmp (128×128, 32-bit)"

# tg5040 brick: 216×237, 24-bit, bottom-up
$MAGICK $SRC/logo.png -filter Point -resize 216x216! -background black -gravity center \
    -extent 216x237 -type TrueColor -define bmp:format=bmp3 \
    skeleton/EXTRAS/Tools/tg5040/Bootlogo.pak/brick/bootlogo.bmp
echo "  ✓ tg5040-brick bootlogo.bmp (216×237, 24-bit)"

# zero28: 120×120, 24-bit, bottom-up, rotated 90° clockwise
$MAGICK $SRC/logo.png -filter Point -resize 120x120! -rotate 90 -background black -gravity center \
    -extent 120x120 -type TrueColor -define bmp:format=bmp3 \
    skeleton/EXTRAS/Tools/zero28/Bootlogo.pak/bootlogo.bmp
echo "  ✓ zero28 bootlogo.bmp (120×120, 24-bit, 90° rotated)"

echo ""
echo "All assets generated successfully!"
echo ""
echo "Generated variants:"
ls -lh $OUT/*@*.png $OUT/*@*.bmp | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Platform-specific bootlogos:"
find skeleton/EXTRAS/Tools/*/Bootlogo.pak -name "*.bmp" ! -name "original*" -exec ls -lh {} \; | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Next steps:"
echo "1. Review the generated files"
echo "2. Commit to repository: git add skeleton/SYSTEM/res/ skeleton/SYSTEM/res-src/ skeleton/EXTRAS/Tools/"
echo "3. Normal builds will use these pre-generated variants (no ImageMagick needed)"
