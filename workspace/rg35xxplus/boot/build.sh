#!/bin/sh

SOURCE=boot.sh
TARGET=dmenu.bin

# TODO: test
# SOURCE=test.sh
# TARGET=launch.sh

# Boot assets are copied to this directory during HOST setup phase
# (see Makefile setup target - copies from skeleton/SYSTEM/res/)
# Rename from @2x format to platform-specific names
cp installing@2x.bmp installing.bmp
cp updating@2x.bmp updating.bmp
cp bootlogo@2x.bmp bootlogo.bmp
cp installing@2x-rotated.bmp installing-r.bmp
cp updating@2x-rotated.bmp updating-r.bmp
cp bootlogo@2x-rotated.bmp bootlogo-r.bmp
cp installing@2x-square.bmp installing-s.bmp
cp updating@2x-square.bmp updating-s.bmp
cp bootlogo@2x-square.bmp bootlogo-s.bmp
cp installing@2x-wide.bmp installing-w.bmp
cp updating@2x-wide.bmp updating-w.bmp
cp bootlogo@2x-wide.bmp bootlogo-w.bmp

mkdir -p output

# Standard 640x480 (RG35XX Plus)
dd skip=54 iflag=skip_bytes if=installing.bmp of=output/installing
dd skip=54 iflag=skip_bytes if=updating.bmp of=output/updating
cp bootlogo.bmp ./output/

# Rotated 480x640 (RG35XX Plus with rotated framebuffer)
dd skip=54 iflag=skip_bytes if=installing-r.bmp of=output/installing-r
dd skip=54 iflag=skip_bytes if=updating-r.bmp of=output/updating-r
cp bootlogo-r.bmp ./output/

# Square 720x720 (RG35XX H/CubeXX)
dd skip=54 iflag=skip_bytes if=installing-s.bmp of=output/installing-s
dd skip=54 iflag=skip_bytes if=updating-s.bmp of=output/updating-s
cp bootlogo-s.bmp ./output/

# Widescreen 720x480 (RG35XX SP/RG34XX)
dd skip=54 iflag=skip_bytes if=installing-w.bmp of=output/installing-w
dd skip=54 iflag=skip_bytes if=updating-w.bmp of=output/updating-w
cp bootlogo-w.bmp ./output/

cp ../../all/utils/7z/bin/arm/7z ./output/

cd output

tar -czvf data bootlogo.bmp installing updating bootlogo-r.bmp installing-r updating-r bootlogo-s.bmp installing-s updating-s bootlogo-w.bmp installing-w updating-w 7z

cat ../$SOURCE >$TARGET
{
	echo BINARY
	cat data
	echo
} >>$TARGET
