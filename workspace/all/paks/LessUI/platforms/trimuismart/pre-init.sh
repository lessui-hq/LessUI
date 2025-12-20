#!/bin/sh
# trimuismart pre-init: SD card recovery check

touch /mnt/writetest
sync
if [ -f /mnt/writetest ]; then
	rm -f /mnt/writetest
else
	e2fsck -p /dev/root >/mnt/SDCARD/RootRecovery.txt
	reboot
fi
