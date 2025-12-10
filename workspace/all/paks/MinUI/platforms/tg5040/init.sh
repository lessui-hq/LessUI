#!/bin/sh
# tg5040 initialization

# Extra paths (appended so system paths have priority)
export PATH="$PATH:/usr/trimui/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/trimui/lib"

# Create standard directories (tg5040 needs these created explicitly)
mkdir -p "$BIOS_PATH"
mkdir -p "$SDCARD_PATH/Roms"
mkdir -p "$SAVES_PATH"

# Detect model
TRIMUI_MODEL=$(strings /usr/trimui/bin/MainUI | grep ^Trimui)
export TRIMUI_MODEL
if [ "$TRIMUI_MODEL" = "Trimui Smart Pro" ]; then
	export DEVICE="smartpro"
elif [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
	export DEVICE="brick"
fi

# Rumble motor GPIO (PH3)
echo 227 >/sys/class/gpio/export
printf "%s" out >/sys/class/gpio/gpio227/direction
printf "%s" 0 >/sys/class/gpio/gpio227/value

# Left/Right Pad GPIO for Smart Pro (PD14/PD18)
if [ "$TRIMUI_MODEL" = "Trimui Smart Pro" ]; then
	echo 110 >/sys/class/gpio/export
	printf "%s" out >/sys/class/gpio/gpio110/direction
	printf "%s" 1 >/sys/class/gpio/gpio110/value

	echo 114 >/sys/class/gpio/export
	printf "%s" out >/sys/class/gpio/gpio114/direction
	printf "%s" 1 >/sys/class/gpio/gpio114/value
fi

# DIP Switch GPIO (PH19)
echo 243 >/sys/class/gpio/export
printf "%s" in >/sys/class/gpio/gpio243/direction

# Turn off LEDs
echo 0 >/sys/class/led_anim/max_scale
if [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
	echo 0 >/sys/class/led_anim/max_scale_lr
	echo 0 >/sys/class/led_anim/max_scale_f1f2
fi

# Set default USB mode
usb_device.sh

# Match stock audio settings
tinymix set 9 1
tinymix set 1 0

# Run stock keymon briefly
(
	keymon &
	PID=$!
	sleep 1
	kill -s TERM $PID
) &

# Start stock GPIO input daemon
mkdir -p /tmp/trimui_inputd
trimui_inputd &

# Start stock hardware daemon
hardwareservice &

# CPU setup
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed
CPU_SPEED_PERF=2000000

cpu_restore() {
	echo $CPU_SPEED_PERF >$CPU_PATH
}
cpu_restore

# Disable network
killall MtpDaemon
killall wpa_supplicant
killall udhcpc
rfkill block bluetooth
rfkill block wifi

# Start keymon
keymon.elf &
