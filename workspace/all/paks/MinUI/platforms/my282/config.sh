# shellcheck shell=bash
# shellcheck disable=SC2034  # Variables sourced by generator
# Platform: my282 (Miyoo Mini 4)

PLATFORM="my282"
PLATFORM_ARCH="arm"
SDCARD_PATH="/mnt/SDCARD"
CORES_SUBPATH=".system/cores/a7"

# Extra library/binary paths
EXTRA_LIB_PATHS=""
EXTRA_BIN_PATHS=""

# CPU configuration (uses reclock function)
CPU_GOVERNOR=""
CPU_GOVERNOR_PATH=""
CPU_SPEED_METHOD="reclock"
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

# Hooks available
HAS_PRE_INIT="false"
HAS_POST_ENV="true"
HAS_DAEMONS="true"
HAS_LOOP_HOOK="false"
HAS_POWEROFF_HANDLER="false"
