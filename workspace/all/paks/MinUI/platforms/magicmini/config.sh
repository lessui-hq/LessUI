# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: magicmini (MagicX XU Mini M)

PLATFORM="magicmini"
PLATFORM_ARCH="arm64"
SDCARD_PATH="/storage/TF2"
CORES_SUBPATH=".system/cores/a53"

# Extra library/binary paths
EXTRA_LIB_PATHS=""
EXTRA_BIN_PATHS=""

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
GPU_GOVERNOR="performance"
GPU_GOVERNOR_PATH="/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor"
DMC_GOVERNOR="performance"
DMC_GOVERNOR_PATH="/sys/devices/platform/dmc/devfreq/dmc/governor"
CPU_SPEED_METHOD="direct"
CPU_SPEED_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
CPU_SPEED_PERF=1608000

# Features
HAS_DATETIME_RESTORE="false"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command (none - device handles it)
SHUTDOWN_CMD=""

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="false"
