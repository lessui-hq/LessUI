#!/bin/sh
# rg35xx initialization

# CPU governor (speed controlled by frontend)
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# Storage scheduler optimization
echo noop >/sys/devices/b0238000.mmc/mmc_host/mmc0/emmc_boot_card/block/mmcblk0/queue/scheduler
echo noop >/sys/devices/b0230000.mmc/mmc_host/mmc1/sd_card/block/mmcblk1/queue/scheduler
echo on >/sys/devices/b0238000.mmc/mmc_host/mmc0/power/control
echo on >/sys/devices/b0230000.mmc/mmc_host/mmc1/power/control

# Start keymon
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
