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

    # BMP variants (for dd platforms - 24-bit, standard 54-byte header)
    echo "  Generating BMP variants..."

    # @1x: 320×240 (4:3) - gkdpixel
    $MAGICK $SRC/$ASSET.png -resize 320x240! \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@1x.bmp

    # @1x-wide: 480×272 (16:9) - m17
    # Scale to 480 width, then crop/pad to 480×272
    $MAGICK $SRC/$ASSET.png -resize 480x360! -gravity center \
        -crop 480x272+0+0 +repage -background black -flatten \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@1x-wide.bmp

    # @2x: 640×480 (4:3) - magicmini, rg35xxplus standard
    $MAGICK $SRC/$ASSET.png -resize 640x480! \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@2x.bmp

    # @2x-rotated: 480×640 (portrait) - rg35xxplus 28xx
    $MAGICK $SRC/$ASSET.png -resize 640x480! -rotate 90 \
        -gravity center -crop 480x640+0+0 +repage \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@2x-rotated.bmp

    # @2x-square: 720×720 (1:1) - rg35xxplus cubexx
    $MAGICK $SRC/$ASSET.png -resize 960x720! -gravity center \
        -crop 720x720+0+0 +repage \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@2x-square.bmp

    # @2x-wide: 720×480 (3:2) - rg35xxplus 34xx
    $MAGICK $SRC/$ASSET.png -resize 720x540! -gravity center \
        -crop 720x480+0+0 +repage -background black -flatten \
        -type TrueColor -define bmp:format=bmp3 \
        $OUT/$ASSET@2x-wide.bmp

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

echo "All assets generated successfully!"
echo ""
echo "Generated variants:"
ls -lh $OUT/*@*.png $OUT/*@*.bmp | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Next steps:"
echo "1. Review the generated files"
echo "2. Commit to repository: git add skeleton/SYSTEM/res/ skeleton/SYSTEM/res-src/"
echo "3. Normal builds will use these pre-generated variants (no ImageMagick needed)"
