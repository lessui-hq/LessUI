#!/bin/sh
#
# System Report - Entry point
#
# Generates a comprehensive system report for platform development.
# Report is saved to SDCARD_PATH/system_report_<platform>_<timestamp>.md
#

PAK_DIR="$(dirname "$0")"
PAK_NAME="$(basename "$PAK_DIR")"
PAK_NAME="${PAK_NAME%.*}"

# Set up logging
LOG_FILE="$LOGS_PATH/$PAK_NAME.txt"
mkdir -p "$(dirname "$LOG_FILE")"
rm -f "$LOG_FILE"
exec >>"$LOG_FILE" 2>&1

echo "$0" "$@"
cd "$PAK_DIR" || exit 1

# Create userdata directory for pak
mkdir -p "$USERDATA_PATH/$PAK_NAME"

# Set up environment
export HOME="$USERDATA_PATH/$PAK_NAME"
export PATH="$PAK_DIR:$PATH"

# Output report file (markdown format)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="$SDCARD_PATH/system_report_${PLATFORM}_${TIMESTAMP}.md"

PRESENTER="$SYSTEM_PATH/bin/minui-presenter"

show_message() {
    message="$1"
    seconds="$2"

    if [ -z "$seconds" ]; then
        seconds="3"
    fi

    echo "$message" 1>&2
    $PRESENTER --message "$message" --timeout "$seconds"
}

cleanup() {
    rm -f /tmp/stay_awake
    killall minui-presenter >/dev/null 2>&1 || true
}

run_report() {
    cd "$SDCARD_PATH" || return 1

    show_message "Generating System Report...\n\nThis may take a minute." 3

    # Run the device report script
    "$PAK_DIR/bin/device-report" > "$REPORT_FILE" 2>&1

    # Verify report was created and show result
    if [ -s "$REPORT_FILE" ]; then
        LINES=$(wc -l < "$REPORT_FILE" | tr -d ' ')
        show_message "Report Complete!\n\n$LINES lines\n\nSaved to:\nsystem_report_${PLATFORM}_${TIMESTAMP}.md" 4
    else
        show_message "Report generation failed.\n\nCheck logs for details." 4
    fi
}

main() {
    echo "1" >/tmp/stay_awake
    trap "cleanup" EXIT INT TERM HUP QUIT

    run_report
}

main "$@"
