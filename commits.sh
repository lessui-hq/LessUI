#!/bin/bash
#
# commits.sh - Generate dependency version report for LessUI
#
# This script produces a comprehensive list of git commit information for:
# - Main LessUI repo
# - Toolchains (cross-compilation environments)
# - libretro-common (shared libretro code)
# - Platform-specific third-party dependencies (DinguxCommander, SDL, etc.)
# - Libretro cores (emulator implementations)
#
# Output format: NAME          HASH      DATE        USER/REPO
#

set -euo pipefail  # Exit on error, undefined variables, pipe failures

# Global counters for summary
TOTAL_DEPS=0
TOTAL_TOOLCHAINS=0
TOTAL_CORES=0

# Show git info for a single directory
# Args: $1 = directory path
show() {
	local dir="$1"

	# Skip if directory doesn't exist
	if [ ! -d "$dir" ]; then
		return 0
	fi

	# Skip if not a git repository
	if [ ! -d "$dir/.git" ]; then
		return 0
	fi

	# Change to directory and extract git info
	if ! pushd "$dir" >> /dev/null 2>&1; then
		return 0
	fi

	local hash=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "unknown")
	local name=$(basename "$PWD")
	local date=$(git log -1 --pretty='%ad' --date=format:'%Y-%m-%d' 2>/dev/null || echo "unknown")
	local repo=$(git config --get remote.origin.url 2>/dev/null || echo "")

	# Clean up repo URL (remove git@github.com:, https://github.com/, .git suffix)
	repo=$(sed -E "s,(^git@github.com:)|(^https?://github.com/)|(.git$)|(/$),,g" <<<"$repo")

	popd >> /dev/null

	printf '\055 %-24s%-10s%-12s%s\n' "$name" "$hash" "$date" "$repo"

	# Increment counter
	((TOTAL_DEPS++)) || true
}

# List all git repos in a directory
# Args: $1 = directory path containing subdirectories
list() {
	local dir="$1"

	# Skip if directory doesn't exist
	if [ ! -d "$dir" ]; then
		return 0
	fi

	if ! pushd "$dir" >> /dev/null 2>&1; then
		return 0
	fi

	# Iterate over subdirectories only (not files)
	for subdir in ./*; do
		if [ -d "$subdir" ]; then
			show "$subdir"
		fi
	done

	popd >> /dev/null
}

# Print separator line
rule() {
	echo '----------------------------------------------------------------'
}

# Print section header with separator
# Args: $1 = section name
tell() {
	echo "$1"
	rule
}

# Show cores for a platform
# Args: $1 = platform name (e.g., "rg35xx", "miyoomini")
cores() {
	local platform="$1"
	local cores_dir="./workspace/$platform/cores/src"

	# Only show cores section if the directory exists
	if [ -d "$cores_dir" ]; then
		echo "CORES"
		local before=$TOTAL_DEPS
		list "$cores_dir"
		local cores_count=$((TOTAL_DEPS - before))
		TOTAL_CORES=$((TOTAL_CORES + cores_count))
		bump
	fi
}

# Show a note about shared cores instead of listing them
# Args: $1 = source platform (where cores are copied from)
cores_note() {
	local source_platform="$1"
	echo "(Uses $source_platform cores)"
	bump
}

# Print blank line
bump() {
	printf '\n'
}

# Count toolchains for summary
count_toolchains() {
	if [ -d "./toolchains" ]; then
		TOTAL_TOOLCHAINS=$(find ./toolchains -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')
	fi
}

# Main output block (piped through sed to clean up any stray newlines)
{
	# Main repository
	printf '%-26s%-10s%-12s%s\n' MINUI HASH DATE USER/REPO
	rule
	show ./
	bump

	# Cross-compilation toolchains
	tell TOOLCHAINS
	count_toolchains
	list ./toolchains
	bump

	# Shared libretro code
	tell LIBRETRO
	show ./workspace/all/minarch/libretro-common
	bump

	# RG35XX platform
	tell RG35XX
	show ./workspace/rg35xx/other/DinguxCommander
	show ./workspace/rg35xx/other/evtest
	cores rg35xx

	# Miyoo Mini platform
	tell MIYOOMINI
	show ./workspace/miyoomini/other/DinguxCommander
	show ./workspace/miyoomini/other/sdl
	echo "(Uses shared cores from workspace/shared/)"
	bump

	# Trimui Smart platform
	tell TRIMUISMART
	show ./workspace/trimuismart/other/DinguxCommander
	show ./workspace/trimuismart/other/unzip60
	cores trimuismart

	# RGB30 platform
	tell RGB30
	show ./workspace/rgb30/other/DinguxCommander
	show ./workspace/rgb30/other/sdl12-compat
	cores rgb30

	# TG5040 platform
	tell TG5040
	show ./workspace/tg5040/other/DinguxCommander-sdl2
	show ./workspace/tg5040/other/evtest
	show ./workspace/tg5040/other/jstest
	show ./workspace/tg5040/other/unzip60
	cores tg5040

	# M17 platform
	tell M17
	cores m17

	# RG35XX Plus platform
	tell RG35XXPLUS
	show ./workspace/rg35xxplus/other/dtc
	show ./workspace/rg35xxplus/other/fbset
	show ./workspace/rg35xxplus/other/sdl2
	show ./workspace/rg35xxplus/other/unzip60
	cores_note rg35xx

	# MY282 platform
	tell MY282
	show ./workspace/my282/other/unzip60
	show ./workspace/my282/other/DinguxCommander-sdl2
	show ./workspace/my282/other/squashfs
	cores_note rg35xx

	# Magic Mini platform
	tell MAGICMINI
	show ./workspace/magicmini/other/351files
	cores magicmini

	# Zero 28 platform
	tell ZERO28
	show ./workspace/zero28/other/DinguxCommander-sdl2
	cores_note tg5040

	# MY355 platform
	tell MY355
	show ./workspace/my355/other/evtest
	show ./workspace/my355/other/mkbootimg
	show ./workspace/my355/other/rsce-go
	show ./workspace/my355/other/DinguxCommander-sdl2
	show ./workspace/my355/other/squashfs
	cores my355

	# Summary
	tell SUMMARY
	echo "Total dependencies tracked: $TOTAL_DEPS"
	echo "  - Toolchains: $TOTAL_TOOLCHAINS"
	echo "  - Libretro cores: $TOTAL_CORES"
	echo "  - Other dependencies: $((TOTAL_DEPS - TOTAL_TOOLCHAINS - TOTAL_CORES - 1))"
	bump
} | sed 's/\n/ /g'
