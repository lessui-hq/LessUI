#!/bin/sh
# RGB30 platform initialization (LessOS)
# Device: PowKiddy RGB30 (Rockchip RK3566)
# OS: LessOS (ROCKNIX-based)

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="rgb30"
export LESSUI_VARIANT="square"
export LESSUI_DEVICE="rgb30"

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
