# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: m17 (Powkiddy X55)

PLATFORM="m17"
PLATFORM_ARCH="arm"
SDCARD_PATH="/sdcard"
CORES_SUBPATH=".system/cores/a7"

# Extra library/binary paths
EXTRA_LIB_PATHS="/usr/lib"
EXTRA_BIN_PATHS="/usr/bin"

# CPU configuration (uses performance governor, no speed control)
CPU_GOVERNOR="performance"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
GPU_GOVERNOR="performance"
GPU_GOVERNOR_PATH="/sys/devices/platform/10091000.gpu/devfreq/10091000.gpu/governor"
CPU_SPEED_METHOD="none"
CPU_SPEED_PATH=""
CPU_SPEED_PERF=""

# Features
HAS_DATETIME_RESTORE="true"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command (none - poweroff handler handles it)
SHUTDOWN_CMD=""

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="true"
