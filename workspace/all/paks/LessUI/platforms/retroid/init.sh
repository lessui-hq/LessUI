#!/bin/sh
# Retroid Pocket SM8250 platform initialization (LessOS)
# Devices: Pocket 5, Pocket Flip 2, Pocket Mini V1, Pocket Mini V2
# OS: LessOS (ROCKNIX-based)

# Export LESSUI_* variables for device identification
export LESSUI_PLATFORM="retroid"

case "$LESSOS_DEVICE" in
	*"Pocket 5"*)
		export LESSUI_VARIANT="fhd"
		export LESSUI_DEVICE="pocket5"
		;;
	*"Flip 2"*)
		export LESSUI_VARIANT="fhd"
		export LESSUI_DEVICE="flip2"
		;;
	*"Mini V1"*)
		export LESSUI_VARIANT="4x3"
		export LESSUI_DEVICE="miniv1"
		;;
	*"Mini V2"*)
		export LESSUI_VARIANT="tall"
		export LESSUI_DEVICE="miniv2"
		;;
	*)
		export LESSUI_VARIANT="fhd"
		export LESSUI_DEVICE="pocket5"
		;;
esac

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
