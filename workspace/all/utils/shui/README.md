# shui

A persistent UI daemon for shell scripts. Consolidates message dialogs, list selectors, and keyboard input into a single tool that keeps SDL initialized between calls, eliminating blank screens.

## Why shui?

Previously, shell scripts used separate utilities (`minui-presenter`, `minui-list`, `minui-keyboard`) for UI. Each utility initialized and cleaned up SDL graphics independently, causing noticeable blank screens between calls.

shui solves this by running as a persistent daemon that maintains the graphics context. The first command auto-starts the daemon, and subsequent commands communicate with it via IPC. The launcher calls `shui shutdown` after each pak completes.

## Key Concept: Push-Based Updates

Unlike the old utilities which required backgrounding (`&`) and manual cleanup, shui uses a **push-based model**:

- **Status messages** return immediately (fire-and-forget)
- **Interactive dialogs** wait for user input
- No backgrounding needed - the daemon handles screen updates
- Each new command replaces the current screen

```bash
# Fire-and-forget: returns immediately, script continues
shui message "Loading..."

# Interactive: waits for user response
shui message "Delete file?" --confirm "Delete" --cancel "Keep"
```

## Commands

### message

Display a message dialog. Returns immediately unless buttons are specified.

```bash
# Status message (fire-and-forget)
shui message "Connecting..."
do_connection
shui message "Connected!"

# Confirmation dialog (waits for response)
if shui message "Are you sure?" --confirm "Confirm" --cancel "Cancel"; then
    do_action
fi

# Error with acknowledgment
shui message "Operation failed" --confirm "OK"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--timeout N` | Auto-dismiss after N seconds |
| `--confirm TEXT` | Label for confirm button (A) - makes command wait. Use a single propercase verb (e.g., "Done", "Generate"). Text is automatically uppercased for display. |
| `--cancel TEXT` | Label for cancel button (B) - makes command wait. Use a single propercase verb (e.g., "Cancel", "Keep"). Text is automatically uppercased for display. |
| `--background-color #RRGGBB` | Hex background color |
| `--background-image PATH` | Background image file |
| `--show-pill` | Draw pill background around text |

### list

Display a scrollable list selector with optional toggle support.

```bash
# Simple list
choice=$(shui list --file menu.json --title "Main Menu")

# With toggle items (use left/right to change values)
shui list --file settings.json --item-key settings --write-location /tmp/out --write-value state
```

**Options:**
| Option | Description |
|--------|-------------|
| `--file PATH` | JSON or text file with items |
| `--format FORMAT` | Input format: `json` (default) or `text` |
| `--title TEXT` | Dialog title |
| `--title-alignment ALIGN` | Title alignment: `left`, `center`, `right` |
| `--item-key KEY` | JSON array key (default: `items`) |
| `--confirm TEXT` | Confirm button label |
| `--cancel TEXT` | Cancel button label |
| `--write-location PATH` | Write output to file instead of stdout |
| `--write-value TYPE` | Output type: `selected`, `state`, `name`, `value` |

**JSON format:**
```json
{
  "items": [
    "Simple string item",
    {"name": "Item with value", "value": "returned_value"},
    {"name": "Header", "is_header": true},
    {"name": "Disabled", "disabled": true},
    {
      "name": "Toggle Option",
      "options": ["Off", "On"],
      "selected": 0
    }
  ]
}
```

**Toggle items:** Items with an `options` array become toggles. Use left/right to cycle through options. The `selected` field sets the initial selection.

**Per-item features:**
```json
{
  "name": "Advanced Item",
  "features": {
    "confirm_text": "Save",
    "disabled": false,
    "is_header": false,
    "unselectable": false,
    "hide_confirm": false,
    "hide_cancel": false
  }
}
```

**Output:** Selected item value or state JSON written to stdout (or `--write-location`).

### keyboard

Display an on-screen keyboard for text input.

```bash
shui keyboard --title "Enter Password"
shui keyboard --title "WiFi SSID" --initial "MyNetwork"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--title TEXT` | Prompt displayed above input |
| `--initial TEXT` | Pre-fill input with this value |
| `--write-location PATH` | Write output to file instead of stdout |

**Controls:**
- D-pad: Navigate keyboard
- A: Select key
- B: Backspace
- Y: Cancel (returns initial value)
- Select: Cycle through all layouts

**Layouts (iPhone-style):**
- **Lowercase**: `a-z` with `aA` key to toggle case
- **Uppercase**: `A-Z` with `Aa` key to toggle case
- **Numbers/Symbols**: `0-9`, `-/:;()$&@"` and common punctuation (access via `123` key)
- **Additional Symbols**: `[]{}#%^*+=_\|~<>` (access via `#+=` key)

**On-screen keys:**
- `aA`/`Aa`: Toggle between lowercase and uppercase
- `DEL`: Backspace (same as B button)
- `123`: Switch to numbers/symbols layout
- `ABC`: Switch back to letters
- `#+=`: Switch to additional symbols
- `SPACE`: Insert space
- `OK`: Confirm and return text

**Output:** Entered text written to stdout.

### progress

Display a progress bar. Returns immediately (fire-and-forget).

```bash
# Determinate progress bar
shui progress "Installing..." --value 45

# Indeterminate (animated bouncing bar)
shui progress "Scanning..." --indeterminate

# With title
shui progress "Downloading..." --value 75 --title "RetroArch"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--value N` | Progress percentage (0-100) |
| `--indeterminate` | Show animated bar instead of fixed progress |
| `--title TEXT` | Title displayed above the progress bar |

**Behavior:** Fire-and-forget. The command returns immediately after updating the display. Call repeatedly to update progress.

### auto-sleep

Control device auto-sleep behavior for the current session. Use this when running long operations (like file servers) where the device should stay awake.

```bash
# Disable auto-sleep (keep device awake)
shui auto-sleep off

# Re-enable auto-sleep
shui auto-sleep on
```

**Note:** Auto-sleep is automatically re-enabled when the daemon shuts down or receives a `restart` command.

### restart

Start the daemon if not running, or reset session state if already running. This is the recommended way to initialize shui at the start of each launcher loop iteration.

```bash
shui restart
```

Re-enables auto-sleep and clears any cached state, ensuring a clean slate for the next pak.

### shutdown

Stop the daemon process. Called automatically by the launcher after pak execution. Also re-enables auto-sleep.

```bash
shui shutdown
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success (confirm/select) |
| 2 | Cancel button pressed |
| 3 | Menu button pressed |
| 124 | Timeout expired |

## Examples

### Status updates during operation

```bash
shui message "Enabling WiFi..."
enable_wifi

shui message "Scanning for networks..."
scan_networks

shui message "Connecting..."
connect_to_network

shui message "Connected!"
sleep 1
```

### Confirmation dialog

```bash
if shui message "Delete save file?" --confirm "Delete" --cancel "Keep"; then
    rm "$SAVE_FILE"
fi
```

### Error with acknowledgment

```bash
if ! do_operation; then
    shui message "Operation failed" --confirm "OK"
    exit 1
fi
```

### Menu selection

```bash
cat > /tmp/menu.json << 'EOF'
{"items": ["Continue", "New Game", "Options", "Quit"]}
EOF

choice=$(shui list --file /tmp/menu.json --title "Main Menu")
case "$choice" in
    "Continue") load_game ;;
    "New Game") new_game ;;
    "Options")  show_options ;;
    "Quit")     exit 0 ;;
esac
```

### Settings with toggles

```bash
cat > /tmp/settings.json << 'EOF'
{
  "settings": [
    {"name": "Sound", "options": ["Off", "On"], "selected": 1},
    {"name": "Vibration", "options": ["Off", "On"], "selected": 0},
    {"name": "Brightness", "options": ["Low", "Medium", "High"], "selected": 1}
  ]
}
EOF

shui list --file /tmp/settings.json --item-key settings \
    --write-location /tmp/result.json --write-value state

# Parse result with jq
sound=$(jq -r '.settings[0].selected' /tmp/result.json)
```

### Text input

```bash
name=$(shui keyboard --title "Enter your name")
echo "Hello, $name!"
```

### WiFi password entry

```bash
password=$(shui keyboard --title "WiFi Password")
if [ $? -eq 0 ] && [ -n "$password" ]; then
    connect_wifi "$SSID" "$password"
fi
```

### Progress during file operations

```bash
# Copy files with progress
total=$(ls -1 *.rom | wc -l)
count=0
for file in *.rom; do
    percent=$((count * 100 / total))
    shui progress "Copying ROMs..." --value $percent --title "Backup"
    cp "$file" /backup/
    count=$((count + 1))
done
shui message "Backup complete!" --timeout 2
```

### Indeterminate progress for unknown duration

```bash
shui progress "Scanning for networks..." --indeterminate
scan_wifi_networks
shui message "Found $(wc -l < /tmp/networks.txt) networks"
```

## Architecture

```
┌──────────────────┐         /tmp/shui/         ┌──────────────────┐
│                  │                               │                  │
│  shui (CLI)   │  ────▶  request (JSON)        │  shui daemon  │
│                  │                               │                  │
│  Sends command   │  ◀────  response (JSON)       │  SDL initialized │
│  Fire-and-forget │         (if needed)           │  Processes UI    │
│  or waits        │         pid, ready            │                  │
└──────────────────┘                               └──────────────────┘
```

**IPC Files:**
- `/tmp/shui/pid` - Daemon process ID
- `/tmp/shui/ready` - Signals daemon is initialized
- `/tmp/shui/request` - Command from CLI to daemon
- `/tmp/shui/response` - Result from daemon to CLI

**Lifecycle:**
1. First `shui` command checks for running daemon
2. If not running, forks daemon process
3. Daemon initializes SDL once, writes `ready` file
4. CLI writes request
5. For interactive commands: CLI waits for response
6. For status messages: CLI returns immediately
7. `shui shutdown` (from launcher) stops daemon

## Building

shui is built as part of the standard LessUI build:

```bash
make PLATFORM=miyoomini build
make PLATFORM=miyoomini system
```

The binary is installed to `$SYSTEM_PATH/bin/shui`.

## Comparison with Previous Utilities

| Feature | minui-presenter/list/keyboard | shui |
|---------|------------------------------|---------|
| SDL init | Every call | Once per session |
| Blank screens | Yes, between calls | No |
| Binary count | 3 separate utilities | 1 unified tool |
| Status messages | Requires `&` and `killall` | Fire-and-forget |
| Toggle items | Supported | Supported |
| Cleanup | Each pak must handle | Launcher handles |
| Output | File or stdout | stdout (or file) |
