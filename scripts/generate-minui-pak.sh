#!/bin/bash
# Generate MinUI.pak launch.sh for specified platforms
#
# Usage:
#   ./scripts/generate-minui-pak.sh all           # Generate for all platforms
#   ./scripts/generate-minui-pak.sh miyoomini     # Generate for specific platform
#   ./scripts/generate-minui-pak.sh rg35xx rg35xxplus  # Generate for multiple

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MINUI_DIR="$PROJECT_ROOT/workspace/all/paks/MinUI"
BUILD_DIR="$PROJECT_ROOT/build"

# All supported platforms
ALL_PLATFORMS="miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 my282 magicmini"

# Generate launch.sh for a single platform
generate_platform() {
	local platform="$1"
	local platform_dir="$MINUI_DIR/platforms/$platform"
	local platform_config="$platform_dir/config.sh"
	local output_dir="$BUILD_DIR/SYSTEM/$platform/paks/MinUI.pak"

	if [ ! -f "$platform_config" ]; then
		echo "  Warning: No config for $platform, skipping"
		return
	fi

	echo "  Generating MinUI.pak for $platform"

	# Clear config variables from previous platform
	unset PLATFORM PLATFORM_ARCH SDCARD_PATH CORES_SUBPATH
	unset EXTRA_LIB_PATHS EXTRA_BIN_PATHS
	unset CPU_GOVERNOR CPU_GOVERNOR_PATH GPU_GOVERNOR GPU_GOVERNOR_PATH DMC_GOVERNOR DMC_GOVERNOR_PATH
	unset CPU_SPEED_METHOD CPU_SPEED_PATH CPU_SPEED_PERF CPU_SPEED_MENU CPU_SPEED_GAME
	unset HAS_DATETIME_RESTORE HAS_RTC_CHECK CREATE_BIOS_DIR CREATE_ROMS_DIR CREATE_SAVES_DIR
	unset SHUTDOWN_CMD HDMI_EXPORT_PATH
	unset HAS_PRE_INIT HAS_POST_ENV HAS_DAEMONS HAS_LATE_INIT HAS_LOOP_HOOK HAS_POWEROFF_HANDLER

	# Source the platform config
	# shellcheck source=/dev/null
	. "$platform_config"

	mkdir -p "$output_dir"

	# Generate the launch.sh
	cat > "$output_dir/launch.sh" << 'LAUNCH_HEADER'
#!/bin/sh
# MinUI.pak - Generated launch script
# DO NOT EDIT - regenerate with scripts/generate-minui-pak.sh

LAUNCH_HEADER

	# Add pre-init hook if present
	if [ "$HAS_PRE_INIT" = "true" ] && [ -f "$platform_dir/pre-init.sh" ]; then
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Pre-init hook" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/pre-init.sh" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
	fi

	# Add environment exports
	cat >> "$output_dir/launch.sh" << LAUNCH_ENV
#######################################
# Environment setup

export PLATFORM="$PLATFORM"
export PLATFORM_ARCH="$PLATFORM_ARCH"
export SDCARD_PATH="$SDCARD_PATH"
export BIOS_PATH="\$SDCARD_PATH/Bios"
export SAVES_PATH="\$SDCARD_PATH/Saves"
export SYSTEM_PATH="\$SDCARD_PATH/.system/\$PLATFORM"
export CORES_PATH="\$SDCARD_PATH/$CORES_SUBPATH"
export USERDATA_PATH="\$SDCARD_PATH/.userdata/\$PLATFORM"
export SHARED_USERDATA_PATH="\$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="\$USERDATA_PATH/logs"
export DATETIME_PATH="\$SHARED_USERDATA_PATH/datetime.txt"
LAUNCH_ENV

	# Add extra exports for specific platforms
	if [ "$CREATE_ROMS_DIR" = "true" ]; then
		echo 'export ROMS_PATH="$SDCARD_PATH/Roms"' >> "$output_dir/launch.sh"
	fi
	if [ -n "$HDMI_EXPORT_PATH" ]; then
		echo "export HDMI_EXPORT_PATH=\"$HDMI_EXPORT_PATH\"" >> "$output_dir/launch.sh"
	fi

	# Add directory creation
	cat >> "$output_dir/launch.sh" << 'LAUNCH_DIRS'

#######################################
# Create directories

LAUNCH_DIRS

	if [ "$CREATE_BIOS_DIR" = "true" ]; then
		echo 'mkdir -p "$BIOS_PATH"' >> "$output_dir/launch.sh"
	fi
	if [ "$CREATE_ROMS_DIR" = "true" ]; then
		echo 'mkdir -p "$SDCARD_PATH/Roms"' >> "$output_dir/launch.sh"
	fi
	if [ "$CREATE_SAVES_DIR" = "true" ]; then
		echo 'mkdir -p "$SAVES_PATH"' >> "$output_dir/launch.sh"
	fi

	cat >> "$output_dir/launch.sh" << 'LAUNCH_DIRS2'
mkdir -p "$USERDATA_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"

# Source logging library with rotation (if available)
if [ -f "$SDCARD_PATH/.system/common/log.sh" ]; then
	. "$SDCARD_PATH/.system/common/log.sh"
	log_init "$LOGS_PATH/minui.log"
fi
LAUNCH_DIRS2

	# Add PATH setup
	local path_setup="export PATH=\$SYSTEM_PATH/bin:\$SDCARD_PATH/.system/common/bin/\$PLATFORM_ARCH"
	if [ -n "$EXTRA_BIN_PATHS" ]; then
		path_setup="$path_setup:$EXTRA_BIN_PATHS"
	fi
	path_setup="$path_setup:\$PATH"

	local ld_setup="export LD_LIBRARY_PATH=\$SYSTEM_PATH/lib"
	if [ -n "$EXTRA_LIB_PATHS" ]; then
		ld_setup="$ld_setup:$EXTRA_LIB_PATHS"
	fi
	ld_setup="$ld_setup:\$LD_LIBRARY_PATH"

	cat >> "$output_dir/launch.sh" << LAUNCH_PATHS

#######################################
# PATH setup

$path_setup
$ld_setup
LAUNCH_PATHS

	# Add CPU setup
	cat >> "$output_dir/launch.sh" << 'LAUNCH_CPU_HEADER'

#######################################
# CPU/Governor setup

LAUNCH_CPU_HEADER

	if [ -n "$CPU_GOVERNOR" ] && [ -n "$CPU_GOVERNOR_PATH" ]; then
		echo "echo $CPU_GOVERNOR > $CPU_GOVERNOR_PATH" >> "$output_dir/launch.sh"
	fi
	if [ -n "$GPU_GOVERNOR" ] && [ -n "$GPU_GOVERNOR_PATH" ]; then
		echo "echo $GPU_GOVERNOR > $GPU_GOVERNOR_PATH" >> "$output_dir/launch.sh"
	fi
	if [ -n "$DMC_GOVERNOR" ] && [ -n "$DMC_GOVERNOR_PATH" ]; then
		echo "echo $DMC_GOVERNOR > $DMC_GOVERNOR_PATH" >> "$output_dir/launch.sh"
	fi

	if [ "$CPU_SPEED_METHOD" = "direct" ]; then
		cat >> "$output_dir/launch.sh" << LAUNCH_CPU_DIRECT
CPU_PATH=$CPU_SPEED_PATH
CPU_SPEED_PERF=$CPU_SPEED_PERF
echo \$CPU_SPEED_PERF > \$CPU_PATH
LAUNCH_CPU_DIRECT
	elif [ "$CPU_SPEED_METHOD" = "overclock" ]; then
		cat >> "$output_dir/launch.sh" << LAUNCH_CPU_OVERCLOCK
export CPU_SPEED_MENU=$CPU_SPEED_MENU
export CPU_SPEED_GAME=$CPU_SPEED_GAME
export CPU_SPEED_PERF=$CPU_SPEED_PERF
overclock.elf \$CPU_SPEED_PERF
LAUNCH_CPU_OVERCLOCK
	fi

	# Add post-env hook if present
	if [ "$HAS_POST_ENV" = "true" ] && [ -f "$platform_dir/post-env.sh" ]; then
		echo "" >> "$output_dir/launch.sh"
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Post-env hook" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/post-env.sh" >> "$output_dir/launch.sh"
	fi

	# Add daemons hook
	if [ "$HAS_DAEMONS" = "true" ] && [ -f "$platform_dir/daemons.sh" ]; then
		echo "" >> "$output_dir/launch.sh"
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Daemons" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/daemons.sh" >> "$output_dir/launch.sh"
	else
		cat >> "$output_dir/launch.sh" << 'LAUNCH_KEYMON'

#######################################
# Daemons

keymon.elf &
LAUNCH_KEYMON
	fi

	# Add datetime restore if needed
	if [ "$HAS_DATETIME_RESTORE" = "true" ]; then
		cat >> "$output_dir/launch.sh" << 'LAUNCH_DATETIME'

#######################################
# Datetime restore

LAUNCH_DATETIME
		if [ "$HAS_RTC_CHECK" = "true" ]; then
			cat >> "$output_dir/launch.sh" << 'LAUNCH_DATETIME_RTC'
if [ -f "$DATETIME_PATH" ] && [ ! -f "$USERDATA_PATH/enable-rtc" ]; then
LAUNCH_DATETIME_RTC
		else
			cat >> "$output_dir/launch.sh" << 'LAUNCH_DATETIME_NORTC'
if [ -f "$DATETIME_PATH" ]; then
LAUNCH_DATETIME_NORTC
		fi
		cat >> "$output_dir/launch.sh" << 'LAUNCH_DATETIME_BODY'
	DATETIME=$(cat "$DATETIME_PATH")
	date +'%F %T' -s "$DATETIME"
	DATETIME=$(date +'%s')
	date -u -s "@$DATETIME"
fi
LAUNCH_DATETIME_BODY
	fi

	# Add auto.sh execution
	cat >> "$output_dir/launch.sh" << 'LAUNCH_AUTO'

#######################################
# User auto.sh

AUTO_PATH="$USERDATA_PATH/auto.sh"
if [ -f "$AUTO_PATH" ]; then
	"$AUTO_PATH"
fi

cd $(dirname "$0")
LAUNCH_AUTO

	# Add late-init hook if present (runs after auto.sh)
	if [ "$HAS_LATE_INIT" = "true" ] && [ -f "$platform_dir/late-init.sh" ]; then
		echo "" >> "$output_dir/launch.sh"
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Late initialization" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/late-init.sh" >> "$output_dir/launch.sh"
	fi

	# Add loop hook functions if present
	if [ "$HAS_LOOP_HOOK" = "true" ] && [ -f "$platform_dir/loop-hook.sh" ]; then
		echo "" >> "$output_dir/launch.sh"
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Loop hook functions" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/loop-hook.sh" >> "$output_dir/launch.sh"
	fi

	# Add poweroff handler function if present
	if [ "$HAS_POWEROFF_HANDLER" = "true" ] && [ -f "$platform_dir/poweroff-handler.sh" ]; then
		echo "" >> "$output_dir/launch.sh"
		echo "#######################################" >> "$output_dir/launch.sh"
		echo "# Poweroff handler function" >> "$output_dir/launch.sh"
		echo "" >> "$output_dir/launch.sh"
		cat "$platform_dir/poweroff-handler.sh" >> "$output_dir/launch.sh"
	fi

	# Add main loop
	cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_START'

#######################################
# Main execution loop

EXEC_PATH="/tmp/minui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync
while [ -f "$EXEC_PATH" ]; do
LAUNCH_LOOP_START

	# CPU speed restore before minui (platform-specific)
	if [ "$CPU_SPEED_METHOD" = "overclock" ]; then
		echo '	overclock.elf $CPU_SPEED_PERF' >> "$output_dir/launch.sh"
	elif [ "$CPU_SPEED_METHOD" = "direct" ]; then
		echo '	echo $CPU_SPEED_PERF > $CPU_PATH' >> "$output_dir/launch.sh"
	elif [ "$CPU_SPEED_METHOD" = "reclock" ]; then
		echo '	reclock' >> "$output_dir/launch.sh"
	fi

	# Loop hook call before minui
	if [ "$HAS_LOOP_HOOK" = "true" ]; then
		cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_HOOK'
	if type pre_minui_hook >/dev/null 2>&1; then
		pre_minui_hook
	fi
LAUNCH_LOOP_HOOK
	fi

	# Run minui and save datetime
	cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_MINUI'
	minui.elf > $LOGS_PATH/minui.log 2>&1
	echo $(date +'%F %T') > "$DATETIME_PATH"
	sync

	if [ -f $NEXT_PATH ]; then
LAUNCH_LOOP_MINUI

	# Loop hook before pak
	if [ "$HAS_LOOP_HOOK" = "true" ]; then
		cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_HOOK2'
		if type pre_pak_hook >/dev/null 2>&1; then
			pre_pak_hook
		fi
LAUNCH_LOOP_HOOK2
	fi

	# Execute next pak
	cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_PAK'
		CMD=$(cat $NEXT_PATH)
		# Start shui in background for tool paks (not minui/minarch)
		echo "$CMD" | grep -q "/Tools/" && shui start &
		# Extract pak name for logging (e.g., "Clock" from ".../Clock.pak/launch.sh")
		PAK_NAME=$(echo "$CMD" | sed -n 's|.*/\([^/]*\)\.pak/.*|\1|p')
		[ -z "$PAK_NAME" ] && PAK_NAME="pak"
		eval $CMD > "$LOGS_PATH/${PAK_NAME}.log" 2>&1
		shui stop 2>/dev/null
		rm -f $NEXT_PATH
LAUNCH_LOOP_PAK

	# Swap cleanup (miyoomini specific but doesn't hurt others)
	cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_SWAP'
		if [ -f "/tmp/using-swap" ]; then
			swapoff "$USERDATA_PATH/swapfile"
			rm -f "/tmp/using-swap"
		fi
LAUNCH_LOOP_SWAP

	# CPU speed restore after pak
	if [ "$CPU_SPEED_METHOD" = "overclock" ]; then
		echo '		overclock.elf $CPU_SPEED_PERF' >> "$output_dir/launch.sh"
	elif [ "$CPU_SPEED_METHOD" = "direct" ]; then
		echo '		[ -f $EXEC_PATH ] && echo $CPU_SPEED_PERF > $CPU_PATH' >> "$output_dir/launch.sh"
	elif [ "$CPU_SPEED_METHOD" = "reclock" ]; then
		echo '		reclock' >> "$output_dir/launch.sh"
	fi

	# Save datetime and close if block
	cat >> "$output_dir/launch.sh" << 'LAUNCH_LOOP_END_IF'
		echo $(date +'%F %T') > "$DATETIME_PATH"
		sync
	fi
LAUNCH_LOOP_END_IF

	# Add poweroff handler check
	if [ "$HAS_POWEROFF_HANDLER" = "true" ]; then
		cat >> "$output_dir/launch.sh" << 'LAUNCH_POWEROFF_CHECK'

	# Physical powerswitch handling
	if [ -f "/tmp/poweroff" ]; then
		poweroff_handler
	fi
LAUNCH_POWEROFF_CHECK
	fi

	# Close while loop
	echo "done" >> "$output_dir/launch.sh"
	echo "" >> "$output_dir/launch.sh"

	# Add shutdown command
	if [ -n "$SHUTDOWN_CMD" ]; then
		echo "$SHUTDOWN_CMD # just in case" >> "$output_dir/launch.sh"
	fi

	# Make executable
	chmod +x "$output_dir/launch.sh"
}

# Main
if [ "$1" = "all" ]; then
	platforms="$ALL_PLATFORMS"
elif [ -n "$1" ]; then
	platforms="$*"
else
	echo "Usage: $0 all | <platform> [<platform> ...]"
	echo "Platforms: $ALL_PLATFORMS"
	exit 1
fi

echo "Generating MinUI paks..."
for platform in $platforms; do
	generate_platform "$platform"
done
echo "Done."
