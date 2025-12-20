# Pak UX Guide

A pragmatic guide to building delightful little utilities using shui.

## Philosophy

Users should always feel in control. Every action should be confirmed before it happens, every result should be acknowledged, and every transition should be smooth. No blank screens. No mystery waits. No surprises.

## The Four Laws

### 1. Confirm Before Acting

Never modify anything without explicit user consent. Even if the pak name implies the action, ask first.

```bash
# BAD: Runs immediately when pak opens
shui message "Enabling WiFi..."
enable_wifi

# GOOD: Ask for permission first
if shui message "Enable WiFi?" --confirm "Enable" --cancel "Cancel"; then
    shui progress "Enabling WiFi..." --indeterminate
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
shui message "Done!"

# GOOD: Continuous feedback
shui progress "Preparing firmware..." --indeterminate
prepare_firmware

shui progress "Writing to device..." --value 0
for i in $(seq 1 100); do
    write_chunk "$i"
    shui progress "Writing to device..." --value "$i"
done

# Always show 100% before transitioning
shui progress "Writing to device..." --value 100
sleep 0.3  # Brief pause so user sees completion

shui message "Firmware updated!" --confirm "Done"
```

**Critical:** If using a progress bar, always set it to 100% before moving to the next screen. A bar stuck at 99% feels broken.

### 3. Announce Outcomes Clearly

Every operation needs a clear ending with an action button that tells users what happens next.

```bash
# BAD: Auto-dismissing message
shui message "Success!" --timeout 3

# GOOD: User controls the transition
shui message "WiFi enabled successfully!" --confirm "Done"

# GOOD: When something happens after dismissal
shui message "Firmware updated!" --confirm "Reboot"
reboot

# GOOD: With an option to stay
if shui message "SSH installed!" --confirm "Reboot" --cancel "Later"; then
    reboot
fi
```

**Button text should be single-word action verbs in propercase:**

- Use: "Enable", "Install", "Reboot", "Continue", "Done", "Dismiss"
- Avoid: "OK", "Yes", "No", "Reboot Now", "Install SSH"
- Text is automatically uppercased for display (e.g., "Generate" → "GENERATE")

### 4. Speak Human

Use friendly, clear language throughout. Avoid technical jargon and programmer-speak.

```bash
# BAD: Programmer language
shui message "Process terminated with exit code 0"
shui message "true/false"
shui message "Operation failed: ENOENT"

# GOOD: Human language
shui message "Completed successfully!"
shui message "On/Off"
shui message "File not found. Check the path and try again."
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

if ! shui message "Delete your stick calibration?" \
    --subtext "You'll need to recalibrate afterward." \
    --confirm "Reset" --cancel "Cancel"; then
    exit 0  # User cancelled
fi

shui progress "Resetting calibration..." --indeterminate
do_the_reset

shui message "Calibration reset!" \
    --subtext "Move the stick to recalibrate." \
    --confirm "Done"
```

### The Dangerous Action Pattern

For firmware modifications, destructive operations, or anything irreversible:

```bash
#!/bin/sh

# First confirmation
if ! shui message "Modify device firmware?" \
    --subtext "This will update your device software." \
    --confirm "Continue" --cancel "Cancel"; then
    exit 0
fi

# Second confirmation for truly dangerous ops
if ! shui message "This cannot be undone!" \
    --subtext "Proceed anyway?" \
    --confirm "Proceed" --cancel "Back"; then
    exit 0
fi

# Now do the work with progress (subtext for warnings)
shui progress "Preparing..." --value 0 --subtext "Do not power off"
# ... work ...
shui progress "Applying changes..." --value 50 --subtext "Do not power off"
# ... work ...
shui progress "Finalizing..." --value 100 --subtext "Do not power off"
sleep 0.3

# Clear outcome with next action
shui message "Firmware updated successfully!" --confirm "Reboot"
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
shui list --file settings.json --item-key settings \
    --title "Settings" \
    --confirm "Save" --cancel "Back" \
    --write-location /tmp/result --write-value state
```

### The Long Operation Pattern

For operations that take more than a few seconds:

```bash
# Use indeterminate progress when you don't know duration
shui progress "Scanning for networks..." --indeterminate
scan_networks

# Use determinate progress when you can track it
total=$(count_files)
current=0
for file in $files; do
    percent=$((current * 100 / total))
    shui progress "Copying files..." --value "$percent"
    copy_file "$file"
    current=$((current + 1))
done

# Always end at 100%
shui progress "Copying files..." --value 100
sleep 0.3
```

### The Error Pattern

When things go wrong:

```bash
if ! do_operation 2>/dev/null; then
    # Give user a clear error with acknowledgment
    shui message "Operation failed." \
        --subtext "Check the log file for details." \
        --confirm "Dismiss"
    exit 1
fi
```

For recoverable errors, offer options:

```bash
if ! connect_wifi; then
    if shui message "Connection failed." \
        --subtext "Would you like to try again?" \
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
shui message "Important information!" --timeout 2
```

Fix: Use `--confirm` buttons for anything the user needs to acknowledge.

### The Jargon Dump

```bash
# DON'T: Technical language
shui message "Process returned SIGTERM" --confirm "OK"
```

Fix: "Operation was cancelled" with confirm button "Dismiss"

### The Blank Pause

```bash
# DON'T: Long operations with no progress
do_long_thing
shui message "Done!"
```

Fix: Show progress throughout.

### The Stuck Progress Bar

```bash
# DON'T: Transition away while progress shows 87%
for i in $(seq 1 87); do
    shui progress "Working..." --value "$i"
done
shui message "Done!"
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
shui message "Delete the file?" --confirm "Yes" --cancel "No"
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
    shui message "No update file found." \
        --subtext "Place update.bin in the pak folder." \
        --confirm "Dismiss"
    exit 1
fi

# Describe what will happen and get confirmation
if ! shui message "A firmware update is available." \
    --subtext "This will update your device and reboot." \
    --confirm "Install" --cancel "Cancel"; then
    exit 0
fi

# Show progress during operation (subtext for warnings)
{
    shui progress "Preparing update..." --value 0 --subtext "Do not power off"
    prepare_update

    shui progress "Backing up current firmware..." --value 20 --subtext "Do not power off"
    backup_firmware

    shui progress "Writing new firmware..." --value 40 --subtext "Do not power off"
    for chunk in $(seq 1 10); do
        write_chunk "$chunk"
        progress=$((40 + chunk * 5))
        shui progress "Writing new firmware..." --value "$progress" --subtext "Do not power off"
    done

    shui progress "Verifying installation..." --value 95 --subtext "Do not power off"
    verify_update

    shui progress "Completing..." --value 100
    sleep 0.3

} 2>&1 | tee "$LOGS_PATH/update.txt"

# Check result and announce clearly
if verify_success; then
    shui message "Update installed successfully!" \
        --subtext "Your device will now reboot." \
        --confirm "Reboot"
    reboot
else
    shui message "Update failed." \
        --subtext "Your previous firmware has been restored. Check logs for details." \
        --confirm "Dismiss"
    exit 1
fi
```

---

## Quick Reference

| Situation                 | Pattern                                            |
| ------------------------- | -------------------------------------------------- |
| Before modifying anything | Confirm with action button                         |
| During long operations    | Show progress (indeterminate or percentage)        |
| After completing          | Announce result with action button                 |
| For toggles               | Use "On"/"Off", never "true"/"false"               |
| For confirmations         | Single-word verbs: "Delete", "Install", "Enable"   |
| For cancellations         | Single-word verbs: "Keep", "Back", "Cancel"        |
| For errors                | Clear message + "Dismiss" or recovery options      |
| Before reboot             | Let user trigger: "Reboot"                         |
| Progress bar              | Always hit 100% before transitioning               |
| Additional context        | Use `--subtext` for explanations, not `\n\n`       |
| Critical warnings         | Use `--subtext` on progress for "Do not power off" |

---

## Pak Audit Checklist

Before shipping a pak, verify:

- [ ] No action taken without user confirmation
- [ ] No blank screens during operations
- [ ] Progress shown for anything > 1 second
- [ ] Progress bar reaches 100% before transitioning
- [ ] Success/failure clearly announced
- [ ] All buttons use single-word action verbs
- [ ] All toggles use human-friendly options
- [ ] Error messages are helpful, not technical
- [ ] User controls all transitions (no auto-dismiss for important info)
- [ ] Reboots require explicit user trigger
- [ ] Secondary info uses `--subtext`, not `\n\n` in messages
- [ ] Critical operations show warnings via `--subtext` on progress
