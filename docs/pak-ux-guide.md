# Pak UX Guide

A pragmatic guide to building delightful little utilities using shellui.

## Philosophy

Users should always feel in control. Every action should be confirmed before it happens, every result should be acknowledged, and every transition should be smooth. No blank screens. No mystery waits. No surprises.

## The Four Laws

### 1. Confirm Before Acting

Never modify anything without explicit user consent. Even if the pak name implies the action, ask first.

```bash
# BAD: Runs immediately when pak opens
shellui message "Enabling WiFi..."
enable_wifi

# GOOD: Ask for permission first
if shellui message "Enable WiFi?" --confirm "Enable" --cancel "Cancel"; then
    shellui progress "Enabling WiFi..." --indeterminate
    enable_wifi
fi
```

The only exceptions are:
- **Informational paks** (like IP) that only display data
- **App launchers** (like Files, Clock) that open another application

### 2. Show Progress Always

Users should never see a blank screen or wonder if something is happening.

```bash
# BAD: Long operation with no feedback
flash_firmware
shellui message "Done!"

# GOOD: Continuous feedback
shellui progress "Preparing firmware..." --indeterminate
prepare_firmware

shellui progress "Writing to device..." --value 0
for i in $(seq 1 100); do
    write_chunk "$i"
    shellui progress "Writing to device..." --value "$i"
done

# Always show 100% before transitioning
shellui progress "Writing to device..." --value 100
sleep 0.3  # Brief pause so user sees completion

shellui message "Firmware updated!" --confirm "Done"
```

**Critical:** If using a progress bar, always set it to 100% before moving to the next screen. A bar stuck at 99% feels broken.

### 3. Announce Outcomes Clearly

Every operation needs a clear ending with an action button that tells users what happens next.

```bash
# BAD: Auto-dismissing message
shellui message "Success!" --timeout 3

# GOOD: User controls the transition
shellui message "WiFi enabled successfully!" --confirm "Done"

# GOOD: When something happens after dismissal
shellui message "Firmware updated!" --confirm "Reboot Now"
reboot

# GOOD: With an option to stay
if shellui message "SSH installed!" --confirm "Reboot Now" --cancel "Later"; then
    reboot
fi
```

**Button text should be action words:**
- Use: "Enable", "Install", "Reboot Now", "Continue", "Done"
- Avoid: "OK", "Yes", "No"

### 4. Speak Human

Use friendly, clear language throughout. Avoid technical jargon and programmer-speak.

```bash
# BAD: Programmer language
shellui message "Process terminated with exit code 0"
shellui message "true/false"
shellui message "Operation failed: ENOENT"

# GOOD: Human language
shellui message "Completed successfully!"
shellui message "On/Off"
shellui message "File not found. Check the path and try again."
```

**Toggle values should be:**
- "On" / "Off" - for enabling features
- "Yes" / "No" - for asking questions
- Descriptive options - "Low" / "Medium" / "High"

Never use: "true" / "false", "1" / "0", "enabled" / "disabled"

---

## Patterns

### The Confirmation Pattern

For any action that modifies system state:

```bash
#!/bin/sh

TITLE="Reset Calibration"
DESCRIPTION="This will delete your stick calibration.\nYou'll need to recalibrate afterward."
CONFIRM_TEXT="Reset"
CANCEL_TEXT="Cancel"

if ! shellui message "$DESCRIPTION" --confirm "$CONFIRM_TEXT" --cancel "$CANCEL_TEXT"; then
    exit 0  # User cancelled
fi

shellui progress "Resetting calibration..." --indeterminate
do_the_reset

shellui message "Calibration reset!\n\nMove the stick to recalibrate." --confirm "Done"
```

### The Dangerous Action Pattern

For firmware modifications, destructive operations, or anything irreversible:

```bash
#!/bin/sh

# First confirmation
if ! shellui message "This will modify your device firmware.\n\nAre you sure?" \
    --confirm "Continue" --cancel "Cancel"; then
    exit 0
fi

# Second confirmation for truly dangerous ops
if ! shellui message "This cannot be undone!\n\nProceed anyway?" \
    --confirm "Yes, Proceed" --cancel "Go Back"; then
    exit 0
fi

# Now do the work with progress
shellui progress "Preparing..." --value 0
# ... work ...
shellui progress "Applying changes..." --value 50
# ... work ...
shellui progress "Finalizing..." --value 100
sleep 0.3

# Clear outcome with next action
shellui message "Firmware updated successfully!" --confirm "Reboot Now"
reboot
```

### The Settings Pattern

For toggleable options using a list:

```json
{
    "settings": [
        {
            "name": "WiFi",
            "options": ["Off", "On"],
            "selected": 0
        },
        {
            "name": "Start on boot",
            "options": ["Off", "On"],
            "selected": 0
        }
    ]
}
```

```bash
shellui list --file settings.json --item-key settings \
    --title "Settings" \
    --confirm "Save" --cancel "Back" \
    --write-location /tmp/result --write-value state
```

### The Long Operation Pattern

For operations that take more than a few seconds:

```bash
# Use indeterminate progress when you don't know duration
shellui progress "Scanning for networks..." --indeterminate
scan_networks

# Use determinate progress when you can track it
total=$(count_files)
current=0
for file in $files; do
    percent=$((current * 100 / total))
    shellui progress "Copying files..." --value "$percent"
    copy_file "$file"
    current=$((current + 1))
done

# Always end at 100%
shellui progress "Copying files..." --value 100
sleep 0.3
```

### The Error Pattern

When things go wrong:

```bash
if ! do_operation 2>/dev/null; then
    # Give user a clear error with acknowledgment
    shellui message "Operation failed.\n\nCheck the log file for details." --confirm "Dismiss"
    exit 1
fi
```

For recoverable errors, offer options:

```bash
if ! connect_wifi; then
    if shellui message "Connection failed.\n\nWould you like to try again?" \
        --confirm "Retry" --cancel "Cancel"; then
        # retry logic
    fi
fi
```

---

## Anti-Patterns

### The Silent Runner

```bash
# DON'T: No feedback at all
#!/bin/sh
do_something
```

Fix: Always show what's happening, confirm before actions, announce results.

### The Auto-Dismisser

```bash
# DON'T: Messages that disappear before users can read them
shellui message "Important information!" --timeout 2
```

Fix: Use `--confirm` buttons for anything the user needs to acknowledge.

### The Jargon Dump

```bash
# DON'T: Technical language
shellui message "Process returned SIGTERM" --confirm "OK"
```

Fix: "Operation was cancelled" with confirm button "Dismiss"

### The Blank Pause

```bash
# DON'T: Long operations with no progress
do_long_thing
shellui message "Done!"
```

Fix: Show progress throughout.

### The Stuck Progress Bar

```bash
# DON'T: Transition away while progress shows 87%
for i in $(seq 1 87); do
    shellui progress "Working..." --value "$i"
done
shellui message "Done!"
```

Fix: Always reach 100%, pause briefly, then transition.

### The False Dichotomy

```bash
# DON'T: Confusing toggle options
"options": ["false", "true"]
```

Fix: Use "Off" / "On" or descriptive alternatives.

### The Non-Action Button

```bash
# DON'T: Buttons that don't describe what happens
shellui message "Delete the file?" --confirm "Yes" --cancel "No"
```

Fix: Use action words: `--confirm "Delete" --cancel "Keep"`

---

## Complete Example

Here's a well-structured pak that follows all guidelines:

```bash
#!/bin/sh
# Example: Firmware Update Pak

PAK_DIR="$(dirname "$0")"
cd "$PAK_DIR" || exit 1

# Bail early if update isn't available
if [ ! -f ./update.bin ]; then
    shellui message "No update file found.\n\nPlace update.bin in the pak folder." --confirm "Dismiss"
    exit 1
fi

# Describe what will happen and get confirmation
if ! shellui message "A firmware update is available.\n\nThis will update your device and reboot." \
    --confirm "Install Update" --cancel "Cancel"; then
    exit 0
fi

# Show progress during operation
{
    shellui progress "Preparing update..." --value 0
    prepare_update

    shellui progress "Backing up current firmware..." --value 20
    backup_firmware

    shellui progress "Writing new firmware..." --value 40
    for chunk in $(seq 1 10); do
        write_chunk "$chunk"
        progress=$((40 + chunk * 5))
        shellui progress "Writing new firmware..." --value "$progress"
    done

    shellui progress "Verifying installation..." --value 95
    verify_update

    shellui progress "Completing..." --value 100
    sleep 0.3

} 2>&1 | tee "$LOGS_PATH/update.txt"

# Check result and announce clearly
if verify_success; then
    shellui message "Update installed successfully!\n\nYour device will now reboot." --confirm "Reboot Now"
    reboot
else
    shellui message "Update failed.\n\nYour previous firmware has been restored.\nCheck logs for details." --confirm "Dismiss"
    exit 1
fi
```

---

## Quick Reference

| Situation | Pattern |
|-----------|---------|
| Before modifying anything | Confirm with action button |
| During long operations | Show progress (indeterminate or percentage) |
| After completing | Announce result with action button |
| For toggles | Use "On"/"Off", never "true"/"false" |
| For confirmations | Use action verbs: "Delete", "Install", "Enable" |
| For cancellations | Be specific: "Keep", "Go Back", "Cancel" |
| For errors | Clear message + "Dismiss" or recovery options |
| Before reboot | Let user trigger: "Reboot Now" |
| Progress bar | Always hit 100% before transitioning |

---

## Pak Audit Checklist

Before shipping a pak, verify:

- [ ] No action taken without user confirmation
- [ ] No blank screens during operations
- [ ] Progress shown for anything > 1 second
- [ ] Progress bar reaches 100% before transitioning
- [ ] Success/failure clearly announced
- [ ] All buttons use action words
- [ ] All toggles use human-friendly options
- [ ] Error messages are helpful, not technical
- [ ] User controls all transitions (no auto-dismiss for important info)
- [ ] Reboots require explicit user trigger
