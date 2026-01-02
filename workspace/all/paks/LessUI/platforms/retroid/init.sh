#!/bin/sh
# Retroid Pocket SM8250 platform initialization (LessOS)
# Devices: Pocket 5, Pocket Flip 2, Pocket Mini V1, Pocket Mini V2
# OS: LessOS (ROCKNIX-based)

# Detect device variant from LESSOS_DEVICE (set by LessOS)
case "$LESSOS_DEVICE" in
	*"Mini V1"*) export DEVICE="miniv1" ;;
	*"Mini V2"*) export DEVICE="miniv2" ;;
esac

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
