#!/bin/sh
#
# Kitchen Sink - Visual utility test pak
#
# Tests all visual helpers to ensure they're working correctly:
# - show.elf: Image display utility
# - shui: Shell UI daemon (message, list, keyboard, progress)
#
# This pak is only included in debug builds.
#

cd "$(dirname "$0")"

# Helper to record test results
RESULTS=""
record() {
	RESULTS="${RESULTS}${1}\n"
}

# Test show.elf - displays a system image for 2 seconds
test_show() {
	# Use bundled test image
	PAK_DIR="$(dirname "$0")"
	TEST_IMAGE="$PAK_DIR/res/test.png"

	if [ ! -f "$TEST_IMAGE" ]; then
		record "show.elf: SKIP (no test image)"
		return
	fi

	# Stop shui to release display buffer before show.elf takes control
	# (shui will auto-start when next called)
	shui stop

	# show.elf displays the image and auto-exits after 2 seconds
	# Enable logging to help debug centering issues
	LOG_FILE="$USERDATA_PATH/logs/Kitchen Sink.log" show.elf "$TEST_IMAGE" 2 >>"$USERDATA_PATH/logs/Kitchen Sink.log" 2>&1
	result=$?
	if [ $result -eq 0 ]; then
		record "show.elf: PASS"
	else
		record "show.elf: FAIL (exit $result)"
	fi
}

# Test shui message
test_shui_message() {
	# Fire-and-forget message
	shui message "Testing shui message (fire-and-forget)..." --timeout 1
	sleep 1.5
	record "shui message (fire-and-forget): PASS"

	# Message with subtext
	shui message "Testing subtext" --subtext "This is subtext below the main message" --timeout 1
	sleep 1.5
	record "shui message (subtext): PASS"

	# Confirmation dialog (user must press A)
	if shui message "Press A to confirm this test" --confirm "Confirm"; then
		record "shui message (confirm): PASS"
	else
		record "shui message (confirm): CANCELLED"
	fi
}

# Test shui list
test_shui_list() {
	# Create test menu
	cat >/tmp/kitchen_sink_menu.json <<'EOF'
{
  "items": [
    {"name": "Section Header", "is_header": true},
    "Plain Item 1",
    "Plain Item 2",
    {"name": "Item with value", "value": "custom_value"},
    {"name": "Disabled Item", "disabled": true},
    {"name": "Toggle Option", "options": ["Off", "On"], "selected": 0}
  ]
}
EOF

	choice=$(shui list --file /tmp/kitchen_sink_menu.json --title "Test List Menu")
	exit_code=$?
	rm -f /tmp/kitchen_sink_menu.json

	if [ $exit_code -eq 0 ]; then
		record "shui list: PASS (selected: $choice)"
	elif [ $exit_code -eq 2 ]; then
		record "shui list: CANCELLED (B pressed)"
	else
		record "shui list: FAIL (exit code $exit_code)"
	fi
}

# Test shui keyboard
test_shui_keyboard() {
	text=$(shui keyboard --title "Type 'test' and press OK" --initial "")
	exit_code=$?

	if [ $exit_code -eq 0 ]; then
		record "shui keyboard: PASS (entered: '$text')"
	elif [ $exit_code -eq 2 ]; then
		record "shui keyboard: CANCELLED"
	else
		record "shui keyboard: FAIL (exit code $exit_code)"
	fi
}

# Test shui progress
test_shui_progress() {
	shui message "Testing progress bar..." --timeout 1
	sleep 1

	# Determinate progress
	for i in 0 20 40 60 80 100; do
		shui progress "Determinate progress..." --value $i
		sleep 0.3
	done
	sleep 0.5
	record "shui progress (determinate): PASS"

	# Indeterminate progress
	shui progress "Indeterminate progress..." --indeterminate
	sleep 2
	record "shui progress (indeterminate): PASS"

	# Progress with title and subtext
	shui progress "With title and subtext" --value 50 --title "Test Title" --subtext "Do not power off"
	sleep 2
	record "shui progress (title+subtext): PASS"
}

# Main test sequence
main() {
	# Welcome message
	if ! shui message "Kitchen Sink Test Suite" \
		--subtext "This will test visual utilities.\nPress A to begin or B to cancel." \
		--confirm "Begin" --cancel "Cancel"; then
		exit 0
	fi

	# Run all tests
	test_show
	test_shui_message
	test_shui_list
	test_shui_keyboard
	test_shui_progress

	# Show results
	shui message "Test Results" \
		--subtext "$(printf "$RESULTS")" \
		--confirm "Done"
}

main "$@"
