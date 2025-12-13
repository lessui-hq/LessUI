#!/bin/sh

TF1_PATH=/mnt/mmc    # TF1/NO NAME partition
TF2_PATH=/mnt/sdcard # TF2
LOG_FILE="$TF1_PATH/lessui-install.log"

# Embedded logging (simplified for early boot environment)
# NOTE: Inlined because this script runs from internal storage during early boot,
# before SD card is mounted and .system is accessible. Cannot source shared log.sh.
# Keep format in sync with skeleton/SYSTEM/common/log.sh
log_write() {
	echo "[$1] $2" >>"$LOG_FILE"
}
log_info() { log_write "INFO" "$*"; }
log_error() { log_write "ERROR" "$*"; }

RGXX_MODEL=$(strings /mnt/vendor/bin/dmenu.bin | grep ^RG)

FLAG_PATH=$TF1_PATH/.minstalled
SDCARD_PATH=$TF1_PATH
SYSTEM_DIR=/.system
SYSTEM_FRAG=$SYSTEM_DIR/rg35xxplus
UPDATE_FRAG=/LessUI.7z
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

# rm /mnt/sdcard
# mkdir -p /mnt/sdcard
# poweroff

if [ -h $TF2_PATH ] && [ "$(readlink -f "$TF2_PATH")" = "$(readlink -f "$TF1_PATH")" ]; then
	echo "deleting old TF2 -> TF1 symlink" >>$TF1_PATH/boot.log
	unlink $TF2_PATH
fi

if mountpoint -q $TF2_PATH; then
	echo "TF2 already mounted" >>$TF1_PATH/boot.log
else
	echo "mount TF2" >>$TF1_PATH/boot.log
	mkdir -p $TF2_PATH
	SDCARD_DEVICE=/dev/mmcblk1p1
	mount -t vfat -o rw,utf8,noatime $SDCARD_DEVICE $TF2_PATH
	if [ $? -ne 0 ]; then
		echo "mount failed, symlink to TF1" >>$TF1_PATH/boot.log
		rm -d $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

if [ -d ${TF1_PATH}${SYSTEM_FRAG} ] || [ -f ${TF1_PATH}${UPDATE_FRAG} ]; then
	echo "found LessUI on TF1" >>$TF1_PATH/boot.log
	if [ ! -h $TF2_PATH ]; then
		echo "no system on TF2, unmount and symlink to TF1" >>$TF1_PATH/boot.log
		umount $TF2_PATH
		rm -rf $TF2_PATH
		ln -s $TF1_PATH $TF2_PATH
	fi
fi

SDCARD_PATH=$TF2_PATH
SYSTEM_PATH=${SDCARD_PATH}${SYSTEM_FRAG}
UPDATE_PATH=${SDCARD_PATH}${UPDATE_FRAG}

# is there an update available?
if [ -f $UPDATE_PATH ]; then
	echo "zip detected" >>$TF1_PATH/boot.log

	MODES=$(cat /sys/class/graphics/fb0/modes)
	case $MODES in
		*"480x640"*)
			SUFFIX="-r"
			echo "rotated framebuffer" >>$TF1_PATH/boot.log
			;;
	esac

	case "$RGXX_MODEL" in
		RGcubexx)
			SUFFIX="-s"
			;;
		RG34xx*)
			SUFFIX="-w"
			;;
	esac

	if [ ! -d $SYSTEM_PATH ]; then
		ACTION=installing
		ACTION_NOUN="installation"
	else
		ACTION=updating
		ACTION_NOUN="update"
	fi

	# extract tar.gz from this sh file
	LINE=$(($(grep -na '^BINARY' $0 | cut -d ':' -f 1 | tail -1) + 1))
	tail -n +$LINE "$0" >/tmp/data
	tar -xf /tmp/data -C /tmp >/dev/null 2>&1

	# show action
	dd if=/tmp/$ACTION$SUFFIX of=/dev/fb0
	sync
	echo 0,0 >/sys/class/graphics/fb0/pan

	# install/update bootlogo.bmp
	echo "replace bootlogo" >>$TF1_PATH/boot.log
	if [ $ACTION = "installing" ]; then
		touch $FLAG_PATH
	fi
	BOOT_DEVICE=/dev/mmcblk0p2
	BOOT_PATH=/mnt/boot
	mkdir -p $BOOT_PATH
	mount -t vfat -o rw,utf8,noatime $BOOT_DEVICE $BOOT_PATH
	cp /tmp/bootlogo$SUFFIX.bmp $BOOT_PATH/bootlogo.bmp
	umount $BOOT_PATH
	rm -rf $BOOT_PATH

	log_info "Starting LessUI $ACTION_NOUN..."
	if $SDCARD_PATH/bin/arm64/7z x -y -o$SDCARD_PATH $UPDATE_PATH >>"$LOG_FILE" 2>&1; then
		log_info "Extraction complete"
	else
		EXIT_CODE=$?
		log_error "7z extraction failed with exit code $EXIT_CODE"
	fi
	rm -f $UPDATE_PATH

	# ls -la $SDCARD_PATH >> $TF1_PATH/boot.log

	# cd /tmp
	# rm data installing updating bootlogo.bmp installing-r updating-r bootlogo-r.bmp unzip

	# the updated system finishes the install/update
	if [ -f $SYSTEM_PATH/bin/install.sh ]; then
		log_info "Running install.sh..."
		if $SYSTEM_PATH/bin/install.sh >>"$LOG_FILE" 2>&1; then
			log_info "Installation complete"
		else
			EXIT_CODE=$?
			log_error "install.sh failed with exit code $EXIT_CODE"
		fi
	fi
fi

if [ -f $SYSTEM_PATH/paks/MinUI.pak/launch.sh ]; then
	echo "launch LessUI" >>$TF1_PATH/boot.log
	$SYSTEM_PATH/paks/MinUI.pak/launch.sh
else
	echo "couldn't find launch.sh" >>$TF1_PATH/boot.log
	if [ -h $TF2_PATH ] && [ "$(readlink -f "$TF2_PATH")" = "$(readlink -f "$TF1_PATH")" ]; then
		echo "deleting old TF2 -> TF1 symlink" >>$TF1_PATH/boot.log
		unlink $TF2_PATH
	fi
fi

sync && poweroff
sleep 60 # Wait for shutdown to complete; prevents mid-poweroff resume on newer OFW

exit 0
