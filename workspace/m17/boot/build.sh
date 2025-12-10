#!/bin/sh

# based on rg35xx implementation

TARGET=em_ui.sh

# Boot assets are copied to this directory during HOST setup phase
# (see Makefile setup target - copies from skeleton/SYSTEM/res/)
# Rename from @1x-wide format to platform-specific names
cp installing@1x-wide.bmp installing.bmp
cp updating@1x-wide.bmp updating.bmp

# Skip standard 54-byte BMP header (now using 24-bit BMPs)
mkdir -p output
dd skip=54 iflag=skip_bytes if=installing.bmp of=output/installing
dd skip=54 iflag=skip_bytes if=updating.bmp of=output/updating

cd output

# Zip headerless bitmaps (always regenerate to pick up asset changes)
zip -r data.zip installing updating
mv data.zip data

# encode and add to end of boot.sh
cat ../boot.sh >$TARGET
echo BINARY >>$TARGET
uuencode data data >>$TARGET

# tar missing libs from toolchain
if [ ! -f libpng16.so.16 ] || [ ! -f libSDL2_ttf-2.0.so.0 ]; then
	cp /opt/m17-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/libpng16.so.16.34.0 libpng16.so.16
	cp /opt/m17-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/libSDL2_ttf-2.0.so.0.14.0 libSDL2_ttf-2.0.so.0
fi
