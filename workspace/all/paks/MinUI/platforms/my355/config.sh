# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: my355 (Miyoo A30 / Flip)

PLATFORM="my355"
PLATFORM_ARCH="arm64"
SDCARD_PATH="/mnt/SDCARD"
CORES_SUBPATH=".system/cores/a53"

# Extra library/binary paths
EXTRA_LIB_PATHS="/usr/miyoo/lib"
EXTRA_BIN_PATHS="/usr/miyoo/bin:/usr/miyoo/sbin"

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
CPU_SPEED_METHOD="direct"
CPU_SPEED_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
CPU_SPEED_PERF=1992000

# Features
HAS_DATETIME_RESTORE="false"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command
SHUTDOWN_CMD="shutdown"

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="false"
