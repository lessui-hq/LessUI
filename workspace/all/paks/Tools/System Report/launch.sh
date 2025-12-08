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
    if ! shui message "Generate system report?" \
        --subtext "This includes CPU benchmarking\nand may take a minute." \
        --confirm "Generate" --cancel "Cancel"; then
        exit 0
    fi

    # Run the report generator with progress updates
    {
        # Run the report generator in background
        "$PAK_DIR/bin/generate-report" > "$REPORT_FILE" 2>&1 &
        REPORT_PID=$!

        # Show progress with step information based on timing estimates
        # Steps: device tree, cpu scaling, cpu benchmark (slow), gpu, display, audio,
        #        arch test, sdl libs, system, modules, hardware, pwm, buses, special hw,
        #        environment, network, dmesg

        shui progress "Generating report..." --value 5 --subtext "Device tree detection"
        sleep 1

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 10 --subtext "CPU frequency analysis"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 20 --subtext "CPU benchmark (this takes a while)"
            sleep 8
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 40 --subtext "GPU and display info"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 50 --subtext "Audio subsystem"
            sleep 1
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 55 --subtext "Architecture detection"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 65 --subtext "System information"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 70 --subtext "Hardware paths"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 80 --subtext "Power management detection"
            sleep 2
        fi

        if kill -0 "$REPORT_PID" 2>/dev/null; then
            shui progress "Generating report..." --value 88 --subtext "Network and environment"
            sleep 2
        fi

        # Wait for completion if still running
        while kill -0 "$REPORT_PID" 2>/dev/null; do
            shui progress "Generating report..." --value 90 --subtext "Finishing up..."
            sleep 1
        done

        wait "$REPORT_PID"

        shui progress "Complete!" --value 100
        sleep 0.5
    }

    # Verify report was created and show result
    if [ -s "$REPORT_FILE" ]; then
        LINES=$(wc -l < "$REPORT_FILE" | tr -d ' ')
        shui message "Report generated!" \
            --subtext "$LINES lines saved to:\nsystem_report_${PLATFORM}_${TIMESTAMP}.md" --confirm "Done"
    else
        shui message "Report generation failed." \
            --subtext "Check logs for details." --confirm "Dismiss"
    fi
}

main() {
    echo "1" >/tmp/stay_awake
    trap "cleanup" EXIT INT TERM HUP QUIT

    run_report
}

main "$@"
