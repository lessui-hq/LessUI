#!/bin/sh
# Retroid Pocket SM8250 platform initialization (LessOS)
# Devices: Pocket 5, Pocket Flip 2, Pocket Mini V2
# OS: LessOS (ROCKNIX-based)

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
