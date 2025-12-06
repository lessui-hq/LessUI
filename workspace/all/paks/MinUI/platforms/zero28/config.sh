# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: zero28 (Miyoo A30 variant / GKD Zero28)

PLATFORM="zero28"
PLATFORM_ARCH="arm64"
SDCARD_PATH="/mnt/SDCARD"
CORES_SUBPATH=".system/cores/a53"

# Extra library/binary paths
EXTRA_LIB_PATHS=""
EXTRA_BIN_PATHS=""

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
CPU_SPEED_METHOD="direct"
CPU_SPEED_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
CPU_SPEED_PERF=1800000

# Features
HAS_DATETIME_RESTORE="false"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="true"
CREATE_ROMS_DIR="true"
CREATE_SAVES_DIR="true"

# Shutdown command
SHUTDOWN_CMD="poweroff"

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="false"
