# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: trimuismart (TrimUI Smart / Model S)

PLATFORM="trimuismart"
PLATFORM_ARCH="arm"
SDCARD_PATH="/mnt/SDCARD"
CORES_SUBPATH=".system/cores/a7"

# Extra library/binary paths
EXTRA_LIB_PATHS="/usr/trimui/lib"
EXTRA_BIN_PATHS="/usr/trimui/bin"

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
CPU_SPEED_METHOD="direct"
CPU_SPEED_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
CPU_SPEED_PERF=1536000

# Features
HAS_DATETIME_RESTORE="true"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command
SHUTDOWN_CMD="poweroff"

# Hooks available
HAS_PRE_INIT="true"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="true"
