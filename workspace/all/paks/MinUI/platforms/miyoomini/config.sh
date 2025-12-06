# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: miyoomini (Miyoo Mini / Mini Plus)

PLATFORM="miyoomini"
PLATFORM_ARCH="arm"
SDCARD_PATH="/mnt/SDCARD"
CORES_SUBPATH=".system/cores/a7"

# Extra library/binary paths
EXTRA_LIB_PATHS=""
EXTRA_BIN_PATHS=""

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
CPU_SPEED_METHOD="overclock"
CPU_SPEED_PATH=""
CPU_SPEED_PERF=1488000
CPU_SPEED_MENU=504000
CPU_SPEED_GAME=1296000

# Features
HAS_DATETIME_RESTORE="true"
HAS_RTC_CHECK="true"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command
SHUTDOWN_CMD="shutdown"

# Hooks available
HAS_PRE_INIT="true"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="false"
