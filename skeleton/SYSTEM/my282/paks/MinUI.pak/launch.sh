#!/bin/sh

export PLATFORM="my282"
export PLATFORM_ARCH="arm"
export SDCARD_PATH="/mnt/SDCARD"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SDCARD_PATH/.system/cores/a7"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

#######################################

export PATH=$SYSTEM_PATH/bin:$SDCARD_PATH/.system/common/bin/$PLATFORM_ARCH:$PATH
export LD_LIBRARY_PATH=$SYSTEM_PATH/lib:$LD_LIBRARY_PATH

#######################################

reclock()
{
	overclock.elf userspace 2 1344 384 1080 0
}

echo 0 > /sys/class/leds/led1/brightness
reclock

killall -9 tee
rm -f "$SDCARD_PATH/update.log"
while :; do
	killall -9 wpa_supplicant
	killall -9 MtpDaemon
	ifconfig wlan0 down
	sleep 2
done &

#######################################

keymon.elf & #> $LOGS_PATH/keymon.log 2>&1 &

#######################################

mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"

# Source logging library with rotation (if available)
if [ -f "$SDCARD_PATH/.system/common/log.sh" ]; then
	. "$SDCARD_PATH/.system/common/log.sh"
	log_init "$LOGS_PATH/minui.log"
fi

AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH" # > $LOGS_PATH/auto.log 2>&1
fi

cd $(dirname "$0")

#######################################

EXEC_PATH="/tmp/minui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
	minui.elf > $LOGS_PATH/minui.log 2>&1
	reclock
	echo `date +'%F %T'` > "$DATETIME_PATH"
	sync

	if [ -f $NEXT_PATH ]; then
		CMD=`cat $NEXT_PATH`
		# Start shui in background for tool paks (not minui/minarch)
		echo "$CMD" | grep -q "/Tools/" && shui start &
		eval $CMD
		shui stop 2>/dev/null
		rm -f $NEXT_PATH
		reclock
		echo `date +'%F %T'` > "$DATETIME_PATH"
		sync
	fi
done

shutdown
