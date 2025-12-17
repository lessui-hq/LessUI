#!/bin/bash
#
# run-coverage.sh - Test coverage analysis
#
# Builds tests with coverage instrumentation, runs them, and generates
# an HTML coverage report using lcov/genhtml.
#
# Designed to be run inside the Docker container.
#
# Usage:
#   ./scripts/run-coverage.sh           # Generate coverage report
#   ./scripts/run-coverage.sh -q        # Quiet (less output)
#   ./scripts/run-coverage.sh -o DIR    # Output to DIR (default: coverage)
#
# Output:
#   coverage/index.html - HTML coverage report
#   coverage/coverage.info - Raw lcov data
#

set -e

# Colors (disabled if not a terminal)
if [ -t 1 ]; then
	GREEN='\033[0;32m'
	RED='\033[0;31m'
	YELLOW='\033[0;33m'
	BLUE='\033[0;34m'
	BOLD='\033[1m'
	DIM='\033[2m'
	RESET='\033[0m'
else
	GREEN=''
	RED=''
	YELLOW=''
	BLUE=''
	BOLD=''
	DIM=''
	RESET=''
fi

# Parse arguments
QUIET=0
COVERAGE_DIR="coverage"
while getopts "qo:" opt; do
	case $opt in
		q) QUIET=1 ;;
		o) COVERAGE_DIR="$OPTARG" ;;
		*) echo "Usage: $0 [-q] [-o DIR]"; exit 1 ;;
	esac
done

# Coverage compile flags
COVERAGE_CFLAGS="--coverage -fprofile-arcs -ftest-coverage"

# Test configuration (from Makefile.qa)
TEST_CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter $COVERAGE_CFLAGS"
TEST_INCLUDES="-I tests/support -I tests/vendor/unity -I workspace/all/common -I workspace/all/launcher -I workspace/all/player -I workspace/all/vendor/stb"
TEST_UNITY="tests/vendor/unity/unity.c"

# Source directories to include in coverage (only our code, not tests or third-party)
SOURCE_DIRS="workspace/all/common workspace/all/launcher workspace/all/player"

echo -e "${BOLD}LessUI Test Coverage Analysis${RESET}"
echo ""

###########################################################
# Step 1: Clean old coverage data
###########################################################
echo -e "${BOLD}[1/4] Cleaning old coverage data...${RESET}"
rm -rf "$COVERAGE_DIR"
find . -name "*.gcno" -delete 2>/dev/null || true
find . -name "*.gcda" -delete 2>/dev/null || true
find . -name "*.gcov" -delete 2>/dev/null || true
make -f Makefile.qa clean-tests >/dev/null 2>&1 || true
mkdir -p "$COVERAGE_DIR"

###########################################################
# Step 2: Build and run tests with coverage
###########################################################
echo -e "${BOLD}[2/4] Building and running tests with coverage...${RESET}"

# Track results
TOTAL_TESTS=0
TOTAL_FAILURES=0
FAILED_SUITES=0

# Temp file for output
TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"' EXIT

# Define test builds (synced from Makefile.qa, with coverage flags)
# Each entry: "test_name:sources:extra_flags"
# Test paths: common/* -> tests/unit/all/common/, launcher/* -> tests/unit/all/launcher/, player/* -> tests/unit/all/player/
declare -a TEST_BUILDS=(
	"utils_test:tests/unit/all/common/test_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"nointro_parser_test:tests/unit/all/common/test_nointro_parser.c workspace/all/common/nointro_parser.c workspace/all/common/utils.c workspace/all/common/log.c:"
	"pad_test:tests/unit/all/common/test_api_pad.c workspace/all/common/pad.c:"
	"gfx_text_test:tests/unit/all/common/test_gfx_text.c workspace/all/common/gfx_text.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/sdl_fakes.c:-I tests/vendor/fff -DUNIT_TEST_BUILD"
	"audio_resampler_test:tests/unit/all/common/test_audio_resampler.c workspace/all/common/audio_resampler.c:"
	"player_paths_test:tests/unit/all/player/test_player_paths.c workspace/all/player/player_paths.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"launcher_utils_test:tests/unit/all/launcher/test_launcher_utils.c workspace/all/launcher/launcher_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"m3u_parser_test:tests/unit/all/launcher/test_m3u_parser.c workspace/all/launcher/launcher_m3u.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"launcher_file_utils_test:tests/unit/all/launcher/test_launcher_file_utils.c workspace/all/launcher/launcher_file_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"map_parser_test:tests/unit/all/launcher/test_map_parser.c workspace/all/launcher/launcher_map.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=access -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"collection_parser_test:tests/unit/all/launcher/test_collection_parser.c workspace/all/launcher/collection_parser.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"recent_parser_test:tests/unit/all/launcher/test_recent_parser.c workspace/all/launcher/recent_file.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"recent_writer_test:tests/unit/all/launcher/test_recent_writer.c workspace/all/launcher/recent_file.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/test_temp.c:-D_GNU_SOURCE"
	"recent_runtime_test:tests/unit/all/launcher/test_recent_runtime.c workspace/all/launcher/recent_file.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"directory_utils_test:tests/unit/all/launcher/test_directory_utils.c workspace/all/launcher/launcher_file_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"binary_file_utils_test:tests/unit/all/common/test_binary_file_utils.c workspace/all/common/binary_file_utils.c workspace/all/common/log.c tests/support/test_temp.c:-D_GNU_SOURCE"
	"ui_layout_test:tests/unit/all/common/test_ui_layout.c:"
	"str_compare_test:tests/unit/all/launcher/test_str_compare.c workspace/all/launcher/launcher_str_compare.c:"
	"effect_system_test:tests/unit/all/common/test_effect_system.c:"
	"player_utils_test:tests/unit/all/player/test_player_utils.c workspace/all/player/player_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"player_config_test:tests/unit/all/player/test_player_config.c workspace/all/player/player_config.c workspace/all/player/player_paths.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"player_options_test:tests/unit/all/player/test_player_options.c workspace/all/player/player_options.c:-D_POSIX_C_SOURCE=200809L"
	"platform_variant_test:tests/unit/all/common/test_platform_variant.c workspace/all/common/platform_variant.c:"
	"launcher_entry_test:tests/unit/all/launcher/test_launcher_entry.c workspace/all/launcher/launcher_entry.c workspace/all/common/stb_ds_impl.c workspace/all/launcher/launcher_str_compare.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"directory_index_test:tests/unit/all/launcher/test_directory_index.c workspace/all/launcher/directory_index.c workspace/all/launcher/launcher_entry.c workspace/all/launcher/launcher_map.c workspace/all/common/stb_ds_impl.c workspace/all/launcher/launcher_str_compare.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"player_archive_test:tests/unit/all/player/test_player_archive.c workspace/all/player/player_archive.c workspace/all/player/player_game.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"player_memory_test:tests/unit/all/player/test_player_memory.c workspace/all/player/player_memory.c tests/support/libretro_mocks.c tests/support/test_temp.c:-D_GNU_SOURCE"
	"player_state_test:tests/unit/all/player/test_player_state.c workspace/all/player/player_state.c workspace/all/player/player_paths.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/libretro_mocks.c:-D_DEFAULT_SOURCE"
	"launcher_launcher_test:tests/unit/all/launcher/test_launcher_launcher.c workspace/all/launcher/launcher_launcher.c:"
	"player_cpu_test:tests/unit/all/player/test_player_cpu.c workspace/all/player/player_cpu.c:"
	"frame_pacer_test:tests/unit/all/player/test_frame_pacer.c workspace/all/player/frame_pacer.c:-lm"
	"player_input_test:tests/unit/all/player/test_player_input.c workspace/all/player/player_input.c:"
	"launcher_state_test:tests/unit/all/launcher/test_launcher_state.c workspace/all/launcher/launcher_state.c workspace/all/common/stb_ds_impl.c:"
	"player_menu_test:tests/unit/all/player/test_player_menu.c workspace/all/player/player_context.c tests/support/menu_state_stub.c tests/support/sdl_fakes.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I tests/support/SDL -I workspace/all/player/libretro-common/include -I tests/vendor/fff -D_DEFAULT_SOURCE"
	"player_env_test:tests/unit/all/player/test_player_env.c workspace/all/player/player_env.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I workspace/all/player/libretro-common/include -D_DEFAULT_SOURCE"
	"player_game_test:tests/unit/all/player/test_player_game.c workspace/all/player/player_game.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-DPLAYER_GAME_TEST"
	"player_scaler_test:tests/unit/all/player/test_player_scaler.c workspace/all/player/player_scaler.c:"
	"player_core_test:tests/unit/all/player/test_player_core.c workspace/all/player/player_core.c:-I workspace/all/player/libretro-common/include"
	"launcher_directory_test:tests/unit/all/launcher/test_launcher_directory.c workspace/all/launcher/launcher_directory.c workspace/all/launcher/launcher_file_utils.c workspace/all/launcher/launcher_entry.c workspace/all/launcher/launcher_str_compare.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"launcher_navigation_test:tests/unit/all/launcher/test_launcher_navigation.c workspace/all/launcher/launcher_navigation.c workspace/all/launcher/launcher_context.c workspace/all/launcher/launcher_launcher.c workspace/all/launcher/launcher_entry.c workspace/all/launcher/launcher_str_compare.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"launcher_thumbnail_test:tests/unit/all/launcher/test_launcher_thumbnail.c workspace/all/launcher/launcher_thumbnail.c:"
	"launcher_context_test:tests/unit/all/launcher/test_launcher_context.c workspace/all/launcher/launcher_context.c:"
	"emu_cache_test:tests/unit/all/launcher/test_emu_cache.c workspace/all/launcher/launcher_emu_cache.c workspace/all/common/stb_ds_impl.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"render_common_test:tests/unit/all/common/test_render_common.c workspace/all/common/render_common.c:"
	"integration_workflows_test:tests/integration/test_workflows.c tests/integration/integration_support.c workspace/all/launcher/launcher_m3u.c workspace/all/launcher/launcher_map.c workspace/all/launcher/collection_parser.c workspace/all/launcher/recent_file.c workspace/all/launcher/launcher_file_utils.c workspace/all/common/binary_file_utils.c workspace/all/common/stb_ds_impl.c workspace/all/player/player_paths.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I tests/integration -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE"
	"log_test:tests/unit/all/common/test_log.c workspace/all/common/log.c:-D_DEFAULT_SOURCE -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS -lpthread"
)

TOTAL_SUITES=${#TEST_BUILDS[@]}
SUITE_NUM=0

for build_spec in "${TEST_BUILDS[@]}"; do
	SUITE_NUM=$((SUITE_NUM + 1))

	# Parse build spec
	IFS=':' read -r test_name sources extra_flags <<< "$build_spec"

	# Build with coverage
	if [ $QUIET -eq 0 ]; then
		echo -ne "  Building $test_name... "
	fi

	# shellcheck disable=SC2086
	if ! gcc -o "tests/$test_name" $sources $TEST_UNITY $TEST_INCLUDES $TEST_CFLAGS $extra_flags >"$TMPFILE" 2>&1; then
		echo -e "${RED}[BUILD FAIL]${RESET}"
		FAILED_SUITES=$((FAILED_SUITES + 1))
		if [ $QUIET -eq 0 ]; then
			cat "$TMPFILE"
		fi
		continue
	fi

	# Run test
	if "./tests/$test_name" >"$TMPFILE" 2>&1; then
		status="PASS"
	else
		status="FAIL"
	fi

	# Parse results
	results_line=$(grep -E "^[0-9]+ Tests [0-9]+ Failures" "$TMPFILE" | tail -1)
	if [ -n "$results_line" ]; then
		tests=$(echo "$results_line" | awk '{print $1}')
		failures=$(echo "$results_line" | awk '{print $3}')
	else
		tests=0
		failures=1
	fi

	TOTAL_TESTS=$((TOTAL_TESTS + tests))
	TOTAL_FAILURES=$((TOTAL_FAILURES + failures))

	if [ "$failures" -gt 0 ] || [ "$status" = "FAIL" ]; then
		FAILED_SUITES=$((FAILED_SUITES + 1))
		if [ $QUIET -eq 0 ]; then
			echo -e "${RED}[FAIL]${RESET} ($failures failures)"
		fi
	else
		if [ $QUIET -eq 0 ]; then
			echo -e "${GREEN}[PASS]${RESET}"
		fi
	fi
done

echo ""
PASSED_TESTS=$((TOTAL_TESTS - TOTAL_FAILURES))
PASSED_SUITES=$((TOTAL_SUITES - FAILED_SUITES))
echo -e "  Tests: ${PASSED_TESTS}/${TOTAL_TESTS} passed, Suites: ${PASSED_SUITES}/${TOTAL_SUITES} passed"

###########################################################
# Step 3: Collect coverage data with lcov
###########################################################
echo ""
echo -e "${BOLD}[3/4] Collecting coverage data...${RESET}"

# Capture coverage data from tests directory (where .gcda files are generated)
lcov --capture \
	--directory tests \
	--output-file "$COVERAGE_DIR/coverage.info" \
	--quiet \
	--ignore-errors empty,gcov

# Extract only our source files (remove test files and third-party code)
lcov --extract "$COVERAGE_DIR/coverage.info" \
	'*/workspace/all/common/*' \
	'*/workspace/all/launcher/*' \
	'*/workspace/all/player/*' \
	--output-file "$COVERAGE_DIR/coverage.info" \
	--quiet \
	--ignore-errors empty

# Remove any remaining unwanted paths (ignore if patterns don't match)
lcov --remove "$COVERAGE_DIR/coverage.info" \
	'*/libretro-common/*' \
	--output-file "$COVERAGE_DIR/coverage.info" \
	--quiet \
	--ignore-errors empty,unused

###########################################################
# Step 4: Generate HTML report
###########################################################
echo -e "${BOLD}[4/4] Generating HTML report...${RESET}"

genhtml "$COVERAGE_DIR/coverage.info" \
	--output-directory "$COVERAGE_DIR" \
	--title "LessUI Test Coverage" \
	--legend \
	--show-details \
	--quiet

###########################################################
# Summary
###########################################################
echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════════════${RESET}"
echo -e "${BOLD}                     COVERAGE SUMMARY${RESET}"
echo -e "${BOLD}═══════════════════════════════════════════════════════════════${RESET}"
echo ""

# Extract overall coverage percentage from lcov summary
SUMMARY=$(lcov --summary "$COVERAGE_DIR/coverage.info" 2>&1)
LINES_PCT=$(echo "$SUMMARY" | grep "lines" | sed 's/.*: //' | sed 's/%.*//')
FUNCS_PCT=$(echo "$SUMMARY" | grep "functions" | sed 's/.*: //' | sed 's/%.*//')
BRANCHES_PCT=$(echo "$SUMMARY" | grep "branches" | sed 's/.*: //' | sed 's/%.*//' || echo "N/A")

printf "  %-20s %s%%\n" "Line coverage:" "$LINES_PCT"
printf "  %-20s %s%%\n" "Function coverage:" "$FUNCS_PCT"
if [ "$BRANCHES_PCT" != "N/A" ]; then
	printf "  %-20s %s%%\n" "Branch coverage:" "$BRANCHES_PCT"
fi
echo ""

# Show per-file summary (top 10 lowest coverage, excluding headers)
# Note: lcov --list has bugs in lcov 2.0, so we parse coverage.info directly
echo -e "  ${DIM}Lowest coverage files:${RESET}"
awk '
/^SF:/ {
    # Extract file path (remove /lessui/workspace/all/ prefix)
    file = $0
    sub(/^SF:.*workspace\/all\//, "", file)
    lf = 0
    lh = 0
}
/^LF:/ { lf = substr($0, 4) + 0 }
/^LH:/ { lh = substr($0, 4) + 0 }
/^end_of_record/ {
    if (lf > 0 && file !~ /\.h$/) {
        pct = (lh * 100.0) / lf
        printf "%s %.1f\n", file, pct
    }
}
' "$COVERAGE_DIR/coverage.info" | \
	sort -t' ' -k2 -n | \
	head -10 | \
	while read -r file pct; do
		printf "    %-35s %s%%\n" "$file" "$pct"
	done

echo ""
echo -e "  ${GREEN}${BOLD}Report:${RESET} ${COVERAGE_DIR}/index.html"
echo ""

if [ $TOTAL_FAILURES -eq 0 ] && [ $FAILED_SUITES -eq 0 ]; then
	echo -e "  ${GREEN}${BOLD}All tests passed!${RESET} ${GREEN}✓${RESET}"
else
	echo -e "  ${YELLOW}${BOLD}Some tests failed.${RESET} Coverage may be incomplete."
fi
echo ""
