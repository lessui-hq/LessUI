#!/bin/sh

SDCARD_PATH=/mnt/SDCARD

# --------------------------------------
# update runtrimui.sh
OLD_PATH=/usr/trimui/bin/runtrimui.sh
NEW_PATH=${SDCARD_PATH}/.system/tg5050/dat/runtrimui.sh
echo "check for outdated $OLD_PATH"
if [ -f $NEW_PATH ] && ! grep -q exec $OLD_PATH; then
	echo "replacing with updated version"
	rm -f $OLD_PATH
	cp $NEW_PATH $OLD_PATH
fi
