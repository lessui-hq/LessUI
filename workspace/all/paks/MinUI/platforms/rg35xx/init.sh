#!/bin/sh
# rg35xx initialization

# CPU setup
echo userspace >/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

export CPU_SPEED_MENU=504000
export CPU_SPEED_GAME=1296000
export CPU_SPEED_PERF=1488000

cpu_restore() {
	overclock.elf $CPU_SPEED_PERF
}
cpu_restore

# Storage scheduler optimization
echo noop >/sys/devices/b0238000.mmc/mmc_host/mmc0/emmc_boot_card/block/mmcblk0/queue/scheduler
echo noop >/sys/devices/b0230000.mmc/mmc_host/mmc1/sd_card/block/mmcblk1/queue/scheduler
echo on >/sys/devices/b0238000.mmc/mmc_host/mmc0/power/control
echo on >/sys/devices/b0230000.mmc/mmc_host/mmc1/power/control

# Start keymon
keymon.elf &
