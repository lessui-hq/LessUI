#!/bin/bash
#
# run-tests.sh - Test runner with summary output
#
# Runs all unit tests and displays a clean summary at the end.
# Designed to be run inside the Docker container.
#
# Usage:
#   ./scripts/run-tests.sh           # Run all tests
#   ./scripts/run-tests.sh -v        # Verbose (show all output)
#   ./scripts/run-tests.sh -q        # Quiet (summary only)
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
VERBOSE=0
QUIET=0
while getopts "vq" opt; do
	case $opt in
		v) VERBOSE=1 ;;
		q) QUIET=1 ;;
		*) echo "Usage: $0 [-v] [-q]"; exit 1 ;;
	esac
done

# Track results
declare -a SUITE_NAMES
declare -a SUITE_TESTS
declare -a SUITE_FAILURES
declare -a SUITE_STATUS
TOTAL_TESTS=0
TOTAL_FAILURES=0
TOTAL_SUITES=0
FAILED_SUITES=0

# Temp file for capturing output
TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"' EXIT

# Build all tests first
echo -e "${BOLD}Building tests...${RESET}"
make -f makefile.qa clean-tests >/dev/null 2>&1 || true

# Get list of test executables from makefile
TEST_EXECUTABLES=$(grep '^TEST_EXECUTABLES' makefile.qa | sed 's/TEST_EXECUTABLES = //' | tr ' ' '\n' | grep -v '^$')

# Count total suites
TOTAL_SUITES=$(echo "$TEST_EXECUTABLES" | wc -l | tr -d ' ')

echo -e "${BOLD}Running $TOTAL_SUITES test suites...${RESET}"
echo ""

# Run each test
SUITE_NUM=0
for test_exe in $TEST_EXECUTABLES; do
	SUITE_NUM=$((SUITE_NUM + 1))

	# Extract suite name from path (e.g., tests/utils_test -> utils)
	suite_name=$(basename "$test_exe" | sed 's/_test$//')

	# Build the test
	if ! make -f makefile.qa "$test_exe" >"$TMPFILE" 2>&1; then
		echo -e "  ${RED}[FAIL]${RESET} $suite_name ${DIM}(build failed)${RESET}"
		SUITE_NAMES+=("$suite_name")
		SUITE_TESTS+=(0)
		SUITE_FAILURES+=(1)
		SUITE_STATUS+=("BUILD_FAIL")
		FAILED_SUITES=$((FAILED_SUITES + 1))
		if [ $VERBOSE -eq 1 ]; then
			cat "$TMPFILE"
		fi
		continue
	fi

	# Run the test
	if "./$test_exe" >"$TMPFILE" 2>&1; then
		status="PASS"
	else
		status="FAIL"
	fi

	# Parse results from Unity output
	# Look for line like: "34 Tests 0 Failures 0 Ignored"
	results_line=$(grep -E "^[0-9]+ Tests [0-9]+ Failures" "$TMPFILE" | tail -1)
	if [ -n "$results_line" ]; then
		tests=$(echo "$results_line" | awk '{print $1}')
		failures=$(echo "$results_line" | awk '{print $3}')
	else
		tests=0
		failures=1
	fi

	SUITE_NAMES+=("$suite_name")
	SUITE_TESTS+=("$tests")
	SUITE_FAILURES+=("$failures")
	SUITE_STATUS+=("$status")

	TOTAL_TESTS=$((TOTAL_TESTS + tests))
	TOTAL_FAILURES=$((TOTAL_FAILURES + failures))

	if [ "$failures" -gt 0 ] || [ "$status" = "FAIL" ]; then
		FAILED_SUITES=$((FAILED_SUITES + 1))
		echo -e "  ${RED}[FAIL]${RESET} $suite_name ${DIM}($tests tests, $failures failures)${RESET}"
		if [ $QUIET -eq 0 ]; then
			# Show failed test names
			grep ":FAIL" "$TMPFILE" | while read -r line; do
				test_name=$(echo "$line" | cut -d: -f2)
				echo -e "         ${RED}-${RESET} $test_name"
			done
		fi
		if [ $VERBOSE -eq 1 ]; then
			echo ""
			cat "$TMPFILE"
			echo ""
		fi
	else
		if [ $QUIET -eq 0 ]; then
			echo -e "  ${GREEN}[PASS]${RESET} $suite_name ${DIM}($tests tests)${RESET}"
		fi
	fi
done

# Print summary
echo ""
echo -e "${BOLD}═══════════════════════════════════════════════════════════════${RESET}"
echo -e "${BOLD}                        TEST SUMMARY${RESET}"
echo -e "${BOLD}═══════════════════════════════════════════════════════════════${RESET}"
echo ""

# Stats
PASSED_SUITES=$((TOTAL_SUITES - FAILED_SUITES))
PASSED_TESTS=$((TOTAL_TESTS - TOTAL_FAILURES))

printf "  %-20s %s\n" "Test Suites:" "$PASSED_SUITES/$TOTAL_SUITES passed"
printf "  %-20s %s\n" "Tests:" "$PASSED_TESTS/$TOTAL_TESTS passed"
echo ""

# Show failed suites if any
if [ $FAILED_SUITES -gt 0 ]; then
	echo -e "  ${RED}${BOLD}Failed Suites:${RESET}"
	for i in "${!SUITE_NAMES[@]}"; do
		if [ "${SUITE_FAILURES[$i]}" -gt 0 ] || [ "${SUITE_STATUS[$i]}" = "FAIL" ] || [ "${SUITE_STATUS[$i]}" = "BUILD_FAIL" ]; then
			echo -e "    ${RED}-${RESET} ${SUITE_NAMES[$i]} (${SUITE_FAILURES[$i]} failures)"
		fi
	done
	echo ""
fi

# Final status
if [ $TOTAL_FAILURES -eq 0 ] && [ $FAILED_SUITES -eq 0 ]; then
	echo -e "  ${GREEN}${BOLD}All tests passed!${RESET} ${GREEN}✓${RESET}"
	echo ""
	exit 0
else
	echo -e "  ${RED}${BOLD}Some tests failed.${RESET} ${RED}✗${RESET}"
	echo ""
	exit 1
fi
