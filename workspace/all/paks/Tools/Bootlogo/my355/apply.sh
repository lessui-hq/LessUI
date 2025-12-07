#!/bin/sh
set -x

export PATH=/tmp/bin:$PATH
export LD_LIBRARY_PATH=/tmp/lib:$LD_LIBRARY_PATH

DIR="$(cd "$(dirname "$0")" && pwd)"
PRESENTER="$SYSTEM_PATH/bin/minui-presenter"
PRESENTER_PID=""

hide() {
	if [ -n "$PRESENTER_PID" ]; then
		kill "$PRESENTER_PID" 2>/dev/null
		PRESENTER_PID=""
	fi
}

show() {
	hide
	$PRESENTER --message "$1" --timeout 300 &
	PRESENTER_PID=$!
}

show "Preparing environment..."
echo "preparing environment"
cd "$(dirname "$0")" || exit 1
cp -r payload/* /tmp
cd /tmp || exit 1

show "Extracting boot image..."
echo "extracting boot.img"
dd if=/dev/mtd2ro of=boot.img bs=131072

show "Unpacking boot image..."
echo "unpacking boot.img"
mkdir -p bootimg
unpackbootimg -i boot.img -o bootimg

show "Unpacking resources..."
echo "unpacking resources"
mkdir -p bootres
cp bootimg/boot.img-second bootres/
cd bootres || exit 1
rsce_tool -u boot.img-second

show "Replacing logo..."
echo "replacing logo"
cp -f ../logo.bmp ./
cp -f ../logo.bmp ./logo_kernel.bmp

show "Packing updated resources..."
echo "packing updated resources"
for file in *; do
    [ "$(basename "$file")" != "boot.img-second" ] && set -- "$@" -p "$file"
done
rsce_tool "$@"

show "Packing updated boot image..."
echo "packing updated boot.img"
cp -f boot-second ../bootimg
cd ../ || exit 1
rm boot.img
mkbootimg --kernel bootimg/boot.img-kernel --second bootimg/boot-second --base 0x10000000 --kernel_offset 0x00008000 --ramdisk_offset 0xf0000000 --second_offset 0x00f00000 --pagesize 2048 --hashtype sha1 -o boot.img

show "Flashing boot image..."
echo "flashing updated boot.img"
flashcp boot.img /dev/mtd2 && sync

hide
$PRESENTER --message "Boot logo flashed!\n\nRebooting in 2 seconds..." --timeout 2
echo "done, rebooting"

sleep 2

# self-destruct
mv "$DIR" "$DIR.disabled"
reboot

exit
