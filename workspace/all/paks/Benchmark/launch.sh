#!/bin/sh
#
# CPU Frequency Benchmark
# Sweeps through CPU frequencies and measures performance at each level.
# Outputs CSV data for tuning auto CPU scaling levels.
#
# NOTE: Do not run presenter in background during benchmarks - it interferes
# with CPU frequency scaling on some platforms.
#

DIR="$(dirname "$0")"
cd "$DIR"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"
BENCHMARK="$DIR/benchmark.elf"

# Benchmark parameters
DURATION_MS=1500      # Measurement duration per frequency
WARMUP_MS=500         # Warmup before each measurement
COOLDOWN_S=0.5        # Cooldown sleep between frequencies (helps with thermal)

# Output file
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
CSV_FILE="$USERDATA_PATH/logs/cpu_benchmark_${PLATFORM}_${TIMESTAMP}.csv"
mkdir -p "$(dirname "$CSV_FILE")"

# Temperature reading (if available)
get_temp() {
	if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
		cat /sys/class/thermal/thermal_zone0/temp
	else
		echo "-1"
	fi
}

# Read actual CPU frequency
# Must use the same path base as set_freq for each platform
get_actual_freq() {
	case "$PLATFORM" in
		tg5040|trimuismart|zero28)
			# These platforms use cpu0 path
			cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null && return
			cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq 2>/dev/null && return
			;;
		my355|rgb30|magicmini)
			# These platforms use policy0 path
			cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq 2>/dev/null && return
			cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq 2>/dev/null && return
			;;
		*)
			# Fallback: try all known paths
			for path in \
				/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq \
				/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq \
				/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq \
				/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq
			do
				if [ -f "$path" ]; then
					cat "$path" 2>/dev/null && return
				fi
			done
			;;
	esac
	echo "-1"
}

# Set CPU frequency (platform-specific)
# Returns freq in kHz for CSV normalization
set_freq() {
	FREQ=$1
	case "$PLATFORM" in
		miyoomini|rg35xx)
			# Uses overclock.elf (freq in kHz)
			overclock.elf "$FREQ" >/dev/null 2>&1
			FREQ_KHZ=$FREQ
			;;
		my282)
			# MY282 uses overclock.elf with MHz values, 6 args: governor cores cpu gpu ddr swap
			# Use performance governor, 1 core for consistent benchmarking
			overclock.elf performance 1 "$FREQ" 384 1080 0 >/dev/null 2>&1
			FREQ_KHZ=$((FREQ * 1000))  # Convert MHz to kHz for CSV
			;;
		tg5040|trimuismart|zero28)
			# Uses cpu0 path
			echo "$FREQ" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2>/dev/null
			FREQ_KHZ=$FREQ
			;;
		my355|rgb30|magicmini)
			# Uses policy0 path
			echo "$FREQ" > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed 2>/dev/null
			FREQ_KHZ=$FREQ
			;;
		*)
			return 1
			;;
	esac
}

# Generate frequency range from min to max in 50MHz increments
# For sysfs platforms, reads limits from kernel
# For overclock.elf platforms, must use exact supported frequencies
get_freq_range() {
	case "$PLATFORM" in
		# overclock.elf platforms - can use any frequency, rounds to nearest supported
		miyoomini)
			# Discovered via 50MHz sweep: 6 discrete steps with 500MHz gap above 1100
			echo "400000 600000 800000 1000000 1100000 1600000"
			;;
		rg35xx)
			# Not yet profiled - may have different frequency steps than miyoomini
			echo "240000 504000 720000 840000 1008000 1104000 1200000 1296000 1392000 1488000"
			;;
		my282)
			# MY282 uses MHz values (see workspace/my282/overclock/overclock.c)
			echo "576 720 864 1008 1056 1152 1296 1344 1440 1512"
			;;
		# sysfs platforms - use discovered supported frequencies
		tg5040)
			# Discovered via benchmark: 9 discrete steps
			echo "408000 600000 816000 1008000 1200000 1416000 1608000 1800000 2000000"
			;;
		zero28)
			# Discovered via benchmark: 8 discrete steps (max 1800, no 2000)
			echo "408000 600000 816000 1008000 1200000 1416000 1608000 1800000"
			;;
		trimuismart)
			# Read limits from cpu0 and generate range (not yet profiled)
			MIN=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq 2>/dev/null || echo "400000")
			MAX=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || echo "2000000")
			generate_freq_range "$MIN" "$MAX" 50000
			;;
		my355|rgb30|magicmini)
			# Read limits from policy0 and generate range (not yet profiled)
			MIN=$(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq 2>/dev/null || echo "400000")
			MAX=$(cat /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq 2>/dev/null || echo "2000000")
			generate_freq_range "$MIN" "$MAX" 50000
			;;
		*)
			echo ""
			;;
	esac
}

# Generate frequencies from min to max with given step (all in kHz)
generate_freq_range() {
	MIN=$1
	MAX=$2
	STEP=$3
	FREQ=$MIN
	FREQS=""
	while [ "$FREQ" -le "$MAX" ]; do
		FREQS="$FREQS $FREQ"
		FREQ=$((FREQ + STEP))
	done
	# Make sure we include max if we didn't land on it
	LAST=$(echo "$FREQS" | awk '{print $NF}')
	if [ "$LAST" -lt "$MAX" ]; then
		FREQS="$FREQS $MAX"
	fi
	echo "$FREQS"
}

# Ensure governor is set to userspace for manual control
# Must use the same path base as the setspeed path for each platform
set_userspace_governor() {
	case "$PLATFORM" in
		miyoomini|rg35xx|tg5040|trimuismart|zero28)
			# These platforms use cpu0 path for frequency control
			GOV_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
			;;
		my355|rgb30|magicmini)
			# These platforms use policy0 path for frequency control
			GOV_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
			;;
		*)
			# Fallback: try policy0 first, then cpu0
			GOV_PATH="/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
			if [ ! -f "$GOV_PATH" ]; then
				GOV_PATH="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
			fi
			;;
	esac

	if [ -f "$GOV_PATH" ]; then
		# Save original governor
		ORIG_GOVERNOR=$(cat "$GOV_PATH")
		echo "userspace" > "$GOV_PATH" 2>/dev/null
	fi
}

restore_governor() {
	if [ -n "$ORIG_GOVERNOR" ] && [ -f "$GOV_PATH" ]; then
		echo "$ORIG_GOVERNOR" > "$GOV_PATH" 2>/dev/null
	fi
}

# Main benchmark routine
run_benchmark() {
	FREQS=$(get_freq_range)

	if [ -z "$FREQS" ]; then
		$PRESENTER --message "Platform $PLATFORM not supported for benchmarking" --timeout 4
		exit 1
	fi

	# Count frequencies for progress
	FREQ_COUNT=$(echo "$FREQS" | wc -w)

	# Write CSV header
	echo "timestamp,platform,freq_khz,actual_khz,iterations,duration_ms,temp_mC" > "$CSV_FILE"

	set_userspace_governor

	# Estimate time: ~2.2s per frequency (1.5s test + 0.5s cooldown + 0.2s settle)
	EST_SECONDS=$((FREQ_COUNT * 22 / 10))

	# Show initial message (blocking, not background)
	$PRESENTER --message "CPU Benchmark\n\nTesting $FREQ_COUNT frequencies...\nEstimated time: ~${EST_SECONDS}s" --timeout 2

	for FREQ in $FREQS; do
		# Set frequency (also sets FREQ_KHZ for CSV normalization)
		if ! set_freq "$FREQ"; then
			continue
		fi

		# Small delay for frequency to stabilize
		sleep 0.2

		# Run benchmark (warmup + measurement)
		RESULT=$($BENCHMARK $DURATION_MS $WARMUP_MS)
		ITERATIONS=$(echo "$RESULT" | cut -d' ' -f1)
		ACTUAL_DURATION=$(echo "$RESULT" | cut -d' ' -f2)

		# Get temperature and actual frequency
		TEMP=$(get_temp)
		ACTUAL_FREQ=$(get_actual_freq)

		# Write to CSV (always use kHz for consistency)
		TS=$(date +%Y-%m-%dT%H:%M:%S)
		echo "$TS,$PLATFORM,$FREQ_KHZ,$ACTUAL_FREQ,$ITERATIONS,$ACTUAL_DURATION,$TEMP" >> "$CSV_FILE"

		# Cooldown between frequencies to reduce thermal carryover
		sleep $COOLDOWN_S
	done

	restore_governor

	# Restore normal CPU speed (NORMAL level from platform.c)
	case "$PLATFORM" in
		miyoomini|rg35xx)
			set_freq 1296000  # kHz
			;;
		trimuismart)
			set_freq 1344000  # kHz
			;;
		tg5040|my355|rgb30)
			set_freq 1608000  # kHz
			;;
		zero28|magicmini)
			set_freq 1416000  # kHz
			;;
		my282)
			set_freq 1344     # MHz (my282 uses MHz)
			;;
	esac
}

# Run the benchmark
run_benchmark

# Show results summary
LINES=$(wc -l < "$CSV_FILE")
DATA_LINES=$((LINES - 1))

# Show completion message
if [ "$DATA_LINES" -gt 0 ]; then
	$PRESENTER --message "Benchmark Complete!\n\n$DATA_LINES frequencies tested\n\nResults saved to logs/" --timeout 4
else
	$PRESENTER --message "Benchmark failed!\n\nNo data collected." --timeout 4
fi
