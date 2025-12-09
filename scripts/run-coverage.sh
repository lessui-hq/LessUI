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

# Test configuration (from makefile.qa)
TEST_CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter $COVERAGE_CFLAGS"
TEST_INCLUDES="-I tests/support -I tests/support/unity -I workspace/all/common -I workspace/all/minui -I workspace/all/minarch"
TEST_UNITY="tests/support/unity/unity.c"

# Source directories to include in coverage (only our code, not tests or third-party)
SOURCE_DIRS="workspace/all/common workspace/all/minui workspace/all/minarch"

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
make -f makefile.qa clean-tests >/dev/null 2>&1 || true
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

# Define test builds (simplified from makefile.qa, with coverage flags)
# Each entry: "test_name:sources:extra_flags"
declare -a TEST_BUILDS=(
	"utils_test:tests/unit/all/common/test_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"nointro_parser_test:tests/unit/all/common/test_nointro_parser.c workspace/all/common/nointro_parser.c workspace/all/common/utils.c workspace/all/common/log.c:"
	"pad_test:tests/unit/all/common/test_api_pad.c workspace/all/common/pad.c:"
	"collections_test:tests/unit/all/common/test_collections.c workspace/all/common/collections.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"gfx_text_test:tests/unit/all/common/test_gfx_text.c workspace/all/common/gfx_text.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/sdl_fakes.c:-I tests/support/fff -DUNIT_TEST_BUILD"
	"audio_resampler_test:tests/unit/all/common/test_audio_resampler.c workspace/all/common/audio_resampler.c:"
	"minarch_paths_test:tests/unit/all/common/test_minarch_paths.c workspace/all/minarch/minarch_paths.c:"
	"minui_utils_test:tests/unit/all/common/test_minui_utils.c workspace/all/minui/minui_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:"
	"m3u_parser_test:tests/unit/all/common/test_m3u_parser.c workspace/all/minui/minui_m3u.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"minui_file_utils_test:tests/unit/all/common/test_minui_file_utils.c workspace/all/minui/minui_file_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"map_parser_test:tests/unit/all/common/test_map_parser.c workspace/all/minui/minui_map.c workspace/all/common/collections.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"collection_parser_test:tests/unit/all/common/test_collection_parser.c workspace/all/minui/collection_parser.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"recent_parser_test:tests/unit/all/common/test_recent_parser.c workspace/all/minui/recent_file.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c tests/support/fs_mocks.c:-D_POSIX_C_SOURCE=200809L -Wl,--wrap=exists -Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fgets"
	"recent_writer_test:tests/unit/all/common/test_recent_writer.c workspace/all/minui/recent_file.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"recent_runtime_test:tests/unit/all/common/test_recent_runtime.c workspace/all/minui/recent_file.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"directory_utils_test:tests/unit/all/common/test_directory_utils.c workspace/all/minui/minui_file_utils.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"binary_file_utils_test:tests/unit/all/common/test_binary_file_utils.c workspace/all/common/binary_file_utils.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"ui_layout_test:tests/unit/all/common/test_ui_layout.c workspace/all/common/log.c:-lm"
	"str_compare_test:tests/unit/all/common/test_str_compare.c workspace/all/minui/minui_str_compare.c:"
	"effect_system_test:tests/unit/all/common/test_effect_system.c:"
	"minarch_utils_test:tests/unit/all/common/test_minarch_utils.c workspace/all/minarch/minarch_utils.c:"
	"minarch_config_test:tests/unit/all/common/test_minarch_config.c workspace/all/minarch/minarch_config.c workspace/all/minarch/minarch_paths.c:"
	"minarch_options_test:tests/unit/all/common/test_minarch_options.c workspace/all/minarch/minarch_options.c:-D_POSIX_C_SOURCE=200809L"
	"platform_variant_test:tests/unit/all/common/test_platform_variant.c workspace/all/common/platform_variant.c:"
	"minui_entry_test:tests/unit/all/common/test_minui_entry.c workspace/all/minui/minui_entry.c workspace/all/common/collections.c workspace/all/minui/minui_str_compare.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"directory_index_test:tests/unit/all/common/test_directory_index.c workspace/all/minui/directory_index.c workspace/all/minui/minui_entry.c workspace/all/common/collections.c workspace/all/minui/minui_str_compare.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_POSIX_C_SOURCE=200809L"
	"minarch_zip_test:tests/unit/all/common/test_minarch_zip.c workspace/all/minarch/minarch_zip.c:-lz"
	"minarch_memory_test:tests/unit/all/common/test_minarch_memory.c workspace/all/minarch/minarch_memory.c tests/support/libretro_mocks.c:"
	"minarch_state_test:tests/unit/all/common/test_minarch_state.c workspace/all/minarch/minarch_state.c workspace/all/minarch/minarch_paths.c tests/support/libretro_mocks.c:-D_DEFAULT_SOURCE"
	"minui_launcher_test:tests/unit/all/common/test_minui_launcher.c workspace/all/minui/minui_launcher.c:"
	"minarch_cpu_test:tests/unit/all/common/test_minarch_cpu.c workspace/all/minarch/minarch_cpu.c:"
	"minarch_input_test:tests/unit/all/common/test_minarch_input.c workspace/all/minarch/minarch_input.c:"
	"minui_state_test:tests/unit/all/common/test_minui_state.c workspace/all/minui/minui_state.c:"
	"minarch_menu_test:tests/unit/all/common/test_minarch_menu.c workspace/all/minarch/minarch_context.c tests/support/menu_state_stub.c tests/support/sdl_fakes.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I tests/support/SDL -I workspace/all/minarch/libretro-common/include -I tests/support/fff -D_DEFAULT_SOURCE"
	"minarch_env_test:tests/unit/all/common/test_minarch_env.c workspace/all/minarch/minarch_env.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I workspace/all/minarch/libretro-common/include -D_DEFAULT_SOURCE"
	"minarch_game_test:tests/unit/all/common/test_minarch_game.c workspace/all/minarch/minarch_game.c:-DMINARCH_GAME_TEST"
	"minarch_scaler_test:tests/unit/all/common/test_minarch_scaler.c workspace/all/minarch/minarch_scaler.c:"
	"minarch_core_test:tests/unit/all/common/test_minarch_core.c workspace/all/minarch/minarch_core.c:-I workspace/all/minarch/libretro-common/include"
	"minui_directory_test:tests/unit/all/common/test_minui_directory.c workspace/all/minui/minui_directory.c workspace/all/minui/minui_file_utils.c workspace/all/minui/minui_entry.c workspace/all/minui/minui_str_compare.c workspace/all/common/collections.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"minui_navigation_test:tests/unit/all/common/test_minui_navigation.c workspace/all/minui/minui_navigation.c workspace/all/minui/minui_context.c workspace/all/minui/minui_launcher.c workspace/all/minui/minui_entry.c workspace/all/minui/minui_str_compare.c workspace/all/common/collections.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-D_DEFAULT_SOURCE"
	"minui_thumbnail_test:tests/unit/all/common/test_minui_thumbnail.c workspace/all/minui/minui_thumbnail.c:"
	"minui_context_test:tests/unit/all/common/test_minui_context.c workspace/all/minui/minui_context.c:"
	"render_common_test:tests/unit/all/common/test_render_common.c workspace/all/common/render_common.c:"
	"integration_workflows_test:tests/integration/test_workflows.c tests/integration/integration_support.c workspace/all/minui/minui_m3u.c workspace/all/minui/minui_map.c workspace/all/minui/collection_parser.c workspace/all/minui/recent_file.c workspace/all/minui/minui_file_utils.c workspace/all/common/binary_file_utils.c workspace/all/common/collections.c workspace/all/minarch/minarch_paths.c workspace/all/common/utils.c workspace/all/common/nointro_parser.c workspace/all/common/log.c:-I tests/integration -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE"
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
	'*/workspace/all/minui/*' \
	'*/workspace/all/minarch/*' \
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
echo -e "  ${DIM}Lowest coverage files:${RESET}"
lcov --list "$COVERAGE_DIR/coverage.info" 2>/dev/null | \
	grep -E "^common/|^minarch/|^minui/" | \
	grep -v "\.h$" | \
	sort -t'|' -k2 -n | \
	head -10 | \
	while read -r line; do
		file=$(echo "$line" | awk '{print $1}')
		pct=$(echo "$line" | awk -F'|' '{print $2}' | awk '{print $1}')
		printf "    %-35s %s\n" "$file" "$pct"
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
