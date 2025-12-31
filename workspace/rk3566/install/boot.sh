#!/bin/sh
# NOTE: becomes .tmp_update/rk3566.sh

PLATFORM="rk3566"
# Use LessOS storage path if set, otherwise default
SDCARD_PATH="${LESSOS_STORAGE:-/storage}"
UPDATE_PATH="$SDCARD_PATH/LessUI.7z"
SYSTEM_PATH="$SDCARD_PATH/.system"
LOG_FILE="$SDCARD_PATH/lessui-install.log"
BOOT_LOG="$SDCARD_PATH/lessui-boot.log"

# Source shared update functions
. "$(dirname "$0")/install/update-functions.sh"

# Boot logging helper
boot_log() {
	echo "[$(date '+%H:%M:%S')] $*" >>"$BOOT_LOG"
}
echo "=== LessUI boot started: $(date) ===" >"$BOOT_LOG"
boot_log "PLATFORM=$PLATFORM"
boot_log "SDCARD_PATH=$SDCARD_PATH"
boot_log "LESSOS_PLATFORM=$LESSOS_PLATFORM"
boot_log "LESSOS_STORAGE=$LESSOS_STORAGE"

# CPU setup
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000
echo $CPU_SPEED_PERF >$CPU_PATH
boot_log "CPU governor set"

# install/update
if [ -f "$UPDATE_PATH" ]; then
	cd $(dirname "$0")/$PLATFORM
	boot_log "Changed to $(pwd)"

	if [ -d "$SYSTEM_PATH" ]; then
		ACTION="update"
		boot_log "Running: ./show.elf ./updating.png"
		./show.elf ./updating.png 2>>"$BOOT_LOG"
		boot_log "show.elf exit code: $?"
	else
		ACTION="installation"
		boot_log "Running: ./show.elf ./installing.png"
		./show.elf ./installing.png 2>>"$BOOT_LOG"
		boot_log "show.elf exit code: $?"
	fi

	log_info "Starting LessUI $ACTION..."

	# Perform atomic update with automatic rollback
	atomic_system_update "$UPDATE_PATH" "$SDCARD_PATH" "$SYSTEM_PATH" "$LOG_FILE"
	boot_log "atomic_system_update completed: $?"

	# Run platform-specific install script
	run_platform_install "$SYSTEM_PATH/$PLATFORM/bin/install.sh" "$LOG_FILE"
	boot_log "run_platform_install completed: $?"

	# Return to valid directory (atomic_system_update may have deleted .tmp_update)
	cd "$SDCARD_PATH"
	boot_log "Returned to $SDCARD_PATH"
fi

LAUNCH_PATH="$SYSTEM_PATH/$PLATFORM/paks/LessUI.pak/launch.sh"
boot_log "LAUNCH_PATH=$LAUNCH_PATH"
boot_log "File exists: $([ -f "$LAUNCH_PATH" ] && echo yes || echo no)"

while [ -f "$LAUNCH_PATH" ]; do
	boot_log "Starting launch.sh..."
	"$LAUNCH_PATH" 2>>"$BOOT_LOG"
	LAUNCH_EXIT=$?
	boot_log "launch.sh exited with code: $LAUNCH_EXIT"
done

boot_log "Exiting boot loop, rebooting..."
reboot
