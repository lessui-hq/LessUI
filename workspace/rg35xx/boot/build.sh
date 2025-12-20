#!/bin/sh

TARGET=dmenu.bin

# Boot assets are copied to this directory during HOST setup phase
# (see Makefile setup target - copies from skeleton/SYSTEM/res/)
# Rename from @2x format to platform-specific names
cp installing@2x.bmp installing.bmp
cp updating@2x.bmp updating.bmp

mkdir -p output

# Skip standard 54-byte BMP header (now using 24-bit BMPs)
dd skip=54 iflag=skip_bytes if=installing.bmp of=output/installing
dd skip=54 iflag=skip_bytes if=updating.bmp of=output/updating

convert boot_logo.png -type truecolor output/boot_logo.bmp && gzip -f -n output/boot_logo.bmp

cp ../../all/utils/7z/bin/arm/7z ./output/

cd output
# Always regenerate data archive to pick up asset changes
zip -r data.zip installing updating 7z
mv data.zip data

cp ~/buildroot/output/images/rootfs.ext2 ./

cat ../boot.sh >$TARGET
echo BINARY >>$TARGET
uuencode data data >>$TARGET
