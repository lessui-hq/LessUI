# shellcheck shell=bash
# Sourced by generated launch.sh
# rg35xx post-env hook
# Storage scheduler optimization

echo noop > /sys/devices/b0238000.mmc/mmc_host/mmc0/emmc_boot_card/block/mmcblk0/queue/scheduler
echo noop > /sys/devices/b0230000.mmc/mmc_host/mmc1/sd_card/block/mmcblk1/queue/scheduler
echo on > /sys/devices/b0238000.mmc/mmc_host/mmc0/power/control
echo on > /sys/devices/b0230000.mmc/mmc_host/mmc1/power/control

# Export CPU speed variables for minarch
export CPU_SPEED_MENU
export CPU_SPEED_GAME
export CPU_SPEED_PERF
