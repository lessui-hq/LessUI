# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: rg35xxplus (Anbernic RG35XX Plus / H / 2024 / SP / Cube / RG34XX)

PLATFORM="rg35xxplus"
PLATFORM_ARCH="arm"
SDCARD_PATH="/mnt/sdcard"
CORES_SUBPATH=".system/cores/a7"

# Extra library/binary paths
EXTRA_LIB_PATHS=""
EXTRA_BIN_PATHS=""

# CPU configuration
CPU_GOVERNOR="userspace"
CPU_GOVERNOR_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
CPU_SPEED_METHOD="none"
CPU_SPEED_PATH=""
CPU_SPEED_PERF=""

# Features
HAS_DATETIME_RESTORE="false"
HAS_RTC_CHECK="false"
CREATE_BIOS_DIR="false"
CREATE_ROMS_DIR="false"
CREATE_SAVES_DIR="false"

# Shutdown command
SHUTDOWN_CMD="shutdown"

# Extra exports
HDMI_EXPORT_PATH="/tmp/hdmi_export.sh"

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LATE_INIT="true"
HAS_LOOP_HOOK="true"
HAS_POWEROFF_HANDLER="false"
