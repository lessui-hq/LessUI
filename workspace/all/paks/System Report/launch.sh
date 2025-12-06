#!/bin/sh
#
# System Report - Entry point
#
# Generates a comprehensive system report for platform development.
# Includes hardware discovery and CPU performance benchmarking.
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

cleanup() {
    rm -f /tmp/stay_awake
    shui stop 2>/dev/null || true
}

run_report() {
    cd "$SDCARD_PATH" || return 1

    # Confirm before generating
    if ! shui message "Generate system report?\n\nThis includes CPU benchmarking\nand may take a minute." \
        --confirm "Generate" --cancel "Cancel"; then
        exit 0
    fi

    # Run the report generator with progress updates
    {
        shui progress "Collecting system info..." --value 10

        # Run the report generator
        "$PAK_DIR/bin/generate-report" > "$REPORT_FILE" 2>&1 &
        REPORT_PID=$!

        # Update progress while running
        progress=20
        while kill -0 "$REPORT_PID" 2>/dev/null; do
            shui progress "Generating report..." --value "$progress"
            sleep 2
            progress=$((progress + 10))
            if [ "$progress" -gt 90 ]; then
                progress=90
            fi
        done

        wait "$REPORT_PID"

        shui progress "Complete!" --value 100
        sleep 0.5
    }

    # Verify report was created and show result
    if [ -s "$REPORT_FILE" ]; then
        LINES=$(wc -l < "$REPORT_FILE" | tr -d ' ')
        shui message "Report generated!\n\n$LINES lines saved to:\nsystem_report_${PLATFORM}_${TIMESTAMP}.md" --confirm "Done"
    else
        shui message "Report generation failed.\n\nCheck logs for details." --confirm "Dismiss"
    fi
}

main() {
    echo "1" >/tmp/stay_awake
    trap "cleanup" EXIT INT TERM HUP QUIT

    run_report
}

main "$@"
