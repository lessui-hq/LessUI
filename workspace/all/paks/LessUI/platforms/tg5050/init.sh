#!/bin/sh
# tg5050 initialization (Trimui Smart Pro S - Allwinner A523)

# Extra paths (appended so system paths have priority)
export PATH="$PATH:/usr/trimui/bin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/trimui/lib"

# Create standard directories
mkdir -p "$BIOS_PATH"
mkdir -p "$SDCARD_PATH/Roms"
mkdir -p "$SAVES_PATH"

# Detect model
TRIMUI_MODEL=$(strings /usr/trimui/bin/MainUI | grep ^Trimui)
export TRIMUI_MODEL

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="tg5050"
export LESSUI_VARIANT=""
export LESSUI_DEVICE="smartpros"

# Rumble motor GPIO (different from tg5040)
echo 236 >/sys/class/gpio/export 2>/dev/null
printf "%s" out >/sys/class/gpio/gpio236/direction
printf "%s" 0 >/sys/class/gpio/gpio236/value

# FN switch GPIO (for mute toggle detection)
echo 363 >/sys/class/gpio/export 2>/dev/null
printf "%s" in >/sys/class/gpio/gpio363/direction

# Turn off LEDs
echo 0 >/sys/class/led_anim/max_scale 2>/dev/null

# Set default USB mode
usb_device.sh 2>/dev/null

# A523 audio initialization - unmute all outputs
amixer sset 'HPOUT' unmute 2>/dev/null
amixer sset 'SPK' unmute 2>/dev/null
amixer sset 'LINEOUTL' unmute 2>/dev/null
amixer sset 'LINEOUTR' unmute 2>/dev/null

# Start GPIO input daemon
mkdir -p /tmp/trimui_inputd
trimui_inputd &

# CPU governor - use schedutil for A523 dual-cluster
echo schedutil >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null
echo schedutil >/sys/devices/system/cpu/cpufreq/policy4/scaling_governor 2>/dev/null

# Disable network (enable via WiFi tool)
killall MtpDaemon 2>/dev/null
killall wpa_supplicant 2>/dev/null
killall udhcpc 2>/dev/null
rfkill block bluetooth 2>/dev/null
rfkill block wifi 2>/dev/null

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
