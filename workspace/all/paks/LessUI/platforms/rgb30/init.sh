#!/bin/sh
# RGB30 platform initialization (LessOS)
# Device: PowKiddy RGB30 (Rockchip RK3566)
# OS: LessOS (ROCKNIX-based)

type log_info >/dev/null 2>&1 && log_info "RGB30 platform init starting"

# CPU setup
echo userspace >/sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null
CPU_PATH=/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed
CPU_SPEED_PERF=1992000

cpu_restore() {
	echo "$CPU_SPEED_PERF" >"$CPU_PATH" 2>/dev/null
}
cpu_restore
type log_info >/dev/null 2>&1 && log_info "CPU governor set to userspace"

# SDL environment
export SDL_AUDIODRIVER=alsa
type log_info >/dev/null 2>&1 && log_info "SDL_AUDIODRIVER=$SDL_AUDIODRIVER"

# Start keymon
type log_info >/dev/null 2>&1 && log_info "Starting keymon..."
LOG_FILE="$LOGS_PATH/keymon.log" keymon.elf &
KEYMON_PID=$!
type log_info >/dev/null 2>&1 && log_info "keymon started (PID: $KEYMON_PID)"
