# shellui

A persistent UI daemon for shell scripts. Consolidates message dialogs, list selectors, and keyboard input into a single tool that keeps SDL initialized between calls, eliminating blank screens.

## Why shellui?

Previously, shell scripts used separate utilities (`minui-presenter`, `minui-list`, `minui-keyboard`) for UI. Each utility initialized and cleaned up SDL graphics independently, causing noticeable blank screens between calls.

shellui solves this by running as a persistent daemon that maintains the graphics context. The first command auto-starts the daemon, and subsequent commands communicate with it via IPC. The launcher calls `shellui shutdown` after each pak completes.

## Commands

### message

Display a message dialog.

```bash
shellui message "Hello World"
shellui message "Operation complete" --timeout 3
shellui message "Are you sure?" --confirm "YES" --cancel "NO"
shellui message "Loading..." --background-color "#1a1a2e"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--timeout N` | Auto-dismiss after N seconds (-1 = wait forever) |
| `--confirm TEXT` | Label for confirm button (A) |
| `--cancel TEXT` | Label for cancel button (B) |
| `--background-color #RRGGBB` | Hex background color |
| `--background-image PATH` | Background image file |
| `--show-pill` | Draw pill background around text |

### list

Display a scrollable list selector.

```bash
# From JSON file
shellui list --file /tmp/options.json --title "Select Option"

# From piped text (one item per line)
echo -e "Option 1\nOption 2\nOption 3" | shellui list --format text

# With custom buttons
shellui list --file menu.json --confirm "OK" --cancel "EXIT"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--file PATH` | JSON or text file with items |
| `--format FORMAT` | Input format: `json` (default) or `text` |
| `--title TEXT` | Dialog title |
| `--item-key KEY` | JSON array key (default: `items`) |
| `--confirm TEXT` | Confirm button label |
| `--cancel TEXT` | Cancel button label |

**JSON format:**
```json
{
  "items": [
    "Simple string item",
    {"name": "Item with value", "value": "returned_value"},
    {"name": "Header", "is_header": true},
    {"name": "Disabled", "disabled": true}
  ]
}
```

**Output:** Selected item value written to stdout.

### keyboard

Display an on-screen keyboard for text input.

```bash
shellui keyboard --title "Enter Password"
shellui keyboard --title "WiFi SSID" --initial "MyNetwork"
```

**Options:**
| Option | Description |
|--------|-------------|
| `--title TEXT` | Prompt displayed above input |
| `--initial TEXT` | Pre-fill input with this value |

**Controls:**
- D-pad: Navigate keyboard
- A: Select key
- B: Backspace
- Y: Cancel (returns initial value)
- Select: Toggle uppercase/lowercase

**Output:** Entered text written to stdout.

### shutdown

Stop the daemon process. Called automatically by the launcher after pak execution.

```bash
shellui shutdown
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success (confirm/select) |
| 2 | Cancel button pressed |
| 3 | Menu button pressed |
| 124 | Timeout expired |

## Examples

### Simple confirmation

```bash
if shellui message "Delete save file?" --confirm "DELETE" --cancel "KEEP"; then
    rm "$SAVE_FILE"
fi
```

### Menu selection

```bash
cat > /tmp/menu.json << 'EOF'
{"items": ["Continue", "New Game", "Options", "Quit"]}
EOF

choice=$(shellui list --file /tmp/menu.json --title "Main Menu")
case "$choice" in
    "Continue") load_game ;;
    "New Game") new_game ;;
    "Options")  show_options ;;
    "Quit")     exit 0 ;;
esac
```

### Text input

```bash
name=$(shellui keyboard --title "Enter your name")
echo "Hello, $name!"
```

### WiFi password entry

```bash
password=$(shellui keyboard --title "WiFi Password" --initial "")
if [ $? -eq 0 ] && [ -n "$password" ]; then
    connect_wifi "$SSID" "$password"
fi
```

## Architecture

```
┌──────────────────┐         /tmp/shellui/         ┌──────────────────┐
│                  │                               │                  │
│  shellui (CLI)   │  ────▶  request (JSON)        │  shellui daemon  │
│                  │                               │                  │
│  Sends command   │  ◀────  response (JSON)       │  SDL initialized │
│  Waits for reply │                               │  Processes UI    │
│                  │         pid, ready            │                  │
└──────────────────┘                               └──────────────────┘
```

**IPC Files:**
- `/tmp/shellui/pid` - Daemon process ID
- `/tmp/shellui/ready` - Signals daemon is initialized
- `/tmp/shellui/request` - Command from CLI to daemon
- `/tmp/shellui/response` - Result from daemon to CLI

**Lifecycle:**
1. First `shellui` command checks for running daemon
2. If not running, forks daemon process
3. Daemon initializes SDL once, writes `ready` file
4. CLI writes request, waits for response
5. Daemon processes UI, writes response
6. `shellui shutdown` (from launcher) stops daemon

## Building

shellui is built as part of the standard LessUI build:

```bash
make PLATFORM=miyoomini build
make PLATFORM=miyoomini system
```

The binary is installed to `$SYSTEM_PATH/bin/shellui`.

## Comparison with Previous Utilities

| Feature | minui-presenter/list/keyboard | shellui |
|---------|------------------------------|---------|
| SDL init | Every call | Once per session |
| Blank screens | Yes, between calls | No |
| Binary count | 3 separate utilities | 1 unified tool |
| Cleanup | Each pak must handle | Launcher handles |
| Output | File or stdout | stdout only |
