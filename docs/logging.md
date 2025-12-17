# LessUI Logging System

Comprehensive guide to the LessUI logging infrastructure, covering both C code and shell scripts.

## Overview

LessUI uses a **dual logging system**:

1. **C Logging** (`log.h` / `log.c`) - For compiled binaries (launcher, player, keymon)
2. **Shell Logging** (`log.sh`) - For boot/install scripts

Both systems provide:

- Automatic timestamps (HH:MM:SS format)
- Log levels (ERROR, WARN, INFO, DEBUG)
- Consistent formatting across all platforms
- Automatic newlines (never add `\n` to messages)

---

## C Logging API

### Quick Start

```c
#include "log.h"

// Basic logging (auto-adds newline)
LOG_error("Failed to allocate memory");
LOG_warn("Battery low: %d%%", battery_level);
LOG_info("Loading ROM: %s", rom_path);
LOG_debug("Processing pixel at %d,%d", x, y);

// Automatic errno translation
FILE* fp = fopen(path, "r");
if (!fp) {
    LOG_errno("Failed to open %s", path);
    return -1;
}

// Output: [ERROR] utils.c:123 Failed to open /path/to/file: No such file or directory
```

### Log Levels

| Level     | When to Use                              | Compiled By Default           | Output |
| --------- | ---------------------------------------- | ----------------------------- | ------ |
| **ERROR** | Critical failures that prevent operation | ✅ Always                     | stderr |
| **WARN**  | Non-critical issues, recoverable errors  | ✅ Always                     | stderr |
| **INFO**  | Key events, milestones                   | ⚙️ With `-DENABLE_INFO_LOGS`  | stdout |
| **DEBUG** | Detailed tracing, variable dumps         | ⚙️ With `-DENABLE_DEBUG_LOGS` | stdout |

### Macros

#### Standard Logging

```c
LOG_error(fmt, ...)   // Always compiled, includes file:line
LOG_warn(fmt, ...)    // Always compiled, includes file:line
LOG_info(fmt, ...)    // Compile-time controlled, no file:line
LOG_debug(fmt, ...)   // Compile-time controlled, no file:line
```

#### Errno Helpers

```c
LOG_errno(fmt, ...)       // ERROR level with automatic strerror(errno)
LOG_errno_warn(fmt, ...)  // WARN level with automatic strerror(errno)
```

**Example:**

```c
int fd = open(path, O_RDONLY);
if (fd < 0) {
    LOG_errno("Failed to open %s", path);
    // Output: [ERROR] file.c:42 Failed to open /path/to/file: Permission denied
}
```

### File Logging API (Optional)

For daemons or apps needing direct file logging with rotation:

```c
// Open log file (1MB max, keep 3 backups)
LogFile* log = log_file_open("/tmp/keymon.log", 1024*1024, 3);
if (!log) {
    return -1;
}

// Write to file (thread-safe)
log_file_write(log, LOG_LEVEL_WARN, "Battery low: %d%%", level);

// Cleanup
log_file_close(log);
```

**Rotation behavior:**

- When file exceeds `max_size`, it's rotated
- `file.log` → `file.log.1`
- `file.log.1` → `file.log.2`
- `file.log.2` → `file.log.3`
- `file.log.3` → deleted (if max_backups=3)

---

## Shell Logging API

### Quick Start

```bash
#!/bin/sh

# Source the logging library
source "$SYSTEM_PATH/common/log.sh"

# Initialize logging
log_init "$SDCARD_PATH/boot.log"

# Log messages
log_info "System starting..."
log_warn "Battery low"
log_error "Failed to mount SD card"

# Optional cleanup
log_close
```

### Functions

```bash
log_init "/path/to/file.log"  # Initialize with rotation support
log_info "message"             # Info level (stdout + file)
log_warn "message"             # Warning level (stdout + file)
log_error "message"            # Error level (stderr + file)
log_disable                    # Temporarily disable logging
log_enable                     # Re-enable logging
log_close                      # Write footer and close (optional)
```

### Configuration

```bash
LOG_MAX_SIZE=1048576    # 1MB rotation limit (default)
LOG_MAX_BACKUPS=3       # Keep 3 old logs (default)
```

---

## Log File Locations

### Application Logs

**Location:** `$SDCARD_PATH/.userdata/$PLATFORM/logs/`

Example for Miyoo Mini: `/mnt/SDCARD/.userdata/miyoomini/logs/`

**Files:**

- `launcher.log` - Main launcher (rotated)
- `keymon.log` - Hardware button/battery daemon (rotated)
- `batmon.log` - Battery charging monitor (optional)
- `GB.log`, `GBA.log`, `GBC.log`, etc. - Per-emulator logs (19 cores)

### Boot/Install Logs

**Location:** `$SDCARD_PATH/` (SD card root)

**Files:**

- `boot.log` - Boot sequence, hardware init (rotated)
- `install.log` - Installation/update operations (rotated)

### Log Redirection

Application logs are created via **shell redirection** in launch scripts:

```bash
# skeleton/SYSTEM/miyoomini/paks/LessUI.pak/launch.sh
launcher.elf &> $LOGS_PATH/launcher.log

# workspace/all/paks/Emus/launch.sh.template
player.elf "$CORE" "$ROM" &> "$LOGS_PATH/$EMU_TAG.log"
```

---

## When to Log

### ERROR Level

Use for **critical failures** that prevent normal operation:

```c
✅ File open failures (fopen, open)
✅ Memory allocation failures (malloc, calloc)
✅ Invalid state/corruption
✅ System call failures (ioctl, I2C communication)
✅ Save state failures

❌ Missing optional files (use WARN or DEBUG)
❌ Per-frame events (too noisy)
```

**Examples:**

```c
FILE* fp = fopen(path, "r");
if (!fp) {
    LOG_errno("Failed to open config file %s", path);
    return -1;
}

void* buf = malloc(size);
if (!buf) {
    LOG_error("Failed to allocate %zu bytes", size);
    return NULL;
}
```

### WARN Level

Use for **non-critical issues** that don't prevent operation:

```c
✅ Battery low warnings
✅ Missing optional files (bootlogo, themes)
✅ Deprecated config options
✅ Recoverable errors
✅ Performance issues

❌ Expected conditions (file doesn't exist when optional)
❌ Normal user actions
```

**Examples:**

```c
if (battery < 10 && !charging) {
    LOG_warn("Battery critically low: %d%%", battery);
}

if (!exists(bootlogo_path)) {
    LOG_warn("Custom bootlogo not found, using default");
}
```

### INFO Level

Use for **key events and milestones**:

```c
✅ Application startup (with version/platform)
✅ ROM loading start/complete
✅ Save state operations
✅ Config changes
✅ Major state transitions

❌ Per-frame events
❌ Individual button presses
❌ Fine-grained debugging
```

**Examples:**

```c
LOG_info("Starting Launcher on %s", PLATFORM);
LOG_info("Loading ROM: %s", rom_path);
LOG_info("Saved %d recent games", count);
LOG_info("Config loaded from %s", config_path);
```

### DEBUG Level

Use for **detailed tracing** (development/testing only):

```c
✅ Loop iterations
✅ Variable values
✅ Function entry/exit
✅ Detailed state dumps

❌ Don't use in production (compiled out)
```

**Examples:**

```c
LOG_debug("Processing frame %d", frame_num);
LOG_debug("Analog stick: x=%d y=%d", x, y);
LOG_debug("Config option %s = %s", key, value);
```

---

## Build Configuration

### Enable/Disable Log Levels

**Makefiles control which levels are compiled in:**

```makefile
# launcher/makefile - Main launcher (INFO + ERROR/WARN)
CFLAGS += -DENABLE_INFO_LOGS

# player/makefile - Libretro frontend (INFO + ERROR/WARN)
CFLAGS += -DENABLE_INFO_LOGS

# keymon/makefile - Hardware daemon (ERROR/WARN only)
# No flags = minimal logging for performance

# Makefile.dev - Development builds (ALL levels)
CFLAGS += -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS

# Makefile.qa - Test builds (ALL levels)
CFLAGS += -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS
```

### Linking

**All binaries must link `log.c`:**

```makefile
SOURCE = main.c ../../all/common/log.c ...
```

---

## Log Format

### Console Output

```
[HH:MM:SS] [LEVEL] [context] message
```

**Examples:**

```
[14:32:15] [ERROR] utils.c:298 Failed to open file /Roms/GB/game.gb: No such file or directory
[14:32:16] [WARN] keymon.c:298 Battery critically low: 5%
[14:32:17] [INFO] Loading ROM: Pokemon Red.gb
[14:32:18] [DEBUG] Analog stick: x=128 y=64
```

**Context rules:**

- ERROR/WARN: Includes `file.c:line`
- INFO/DEBUG: No context (cleaner output)

### File Output (log_file_write)

Same format as console, but written to files with automatic rotation.

---

## Log Rotation

### Automatic Rotation (File API)

```c
LogFile* lf = log_file_open("/tmp/daemon.log", 1024*1024, 3);
//                                              ^^^^^^^^  ^^
//                                              1MB limit  3 backups
```

**When rotation occurs:**

1. `daemon.log` reaches 1MB
2. System rotates:
   - Delete `daemon.log.3`
   - Rename `daemon.log.2` → `daemon.log.3`
   - Rename `daemon.log.1` → `daemon.log.2`
   - Rename `daemon.log` → `daemon.log.1`
   - Create new empty `daemon.log`

### Shell Script Rotation

The `log.sh` library automatically rotates before writing:

```bash
log_init "$SDCARD_PATH/boot.log"  # Checks size, rotates if > 1MB
```

---

## Thread Safety

### File API (Thread-Safe)

```c
LogFile* log = log_file_open("/tmp/keymon.log", 0, 0);

// Safe to call from multiple threads
pthread_create(&thread1, NULL, worker1, log);
pthread_create(&thread2, NULL, worker2, log);

void* worker1(void* arg) {
    LogFile* log = (LogFile*)arg;
    log_file_write(log, LOG_LEVEL_INFO, "Thread 1 running");
}
```

**Guarantees:**

- Messages from different threads won't interleave
- Rotation is atomic (one thread rotates, others wait)
- No corrupted lines

### Console API (Not Thread-Safe)

```c
// From multiple threads - OUTPUT MAY INTERLEAVE
LOG_info("Thread A");
LOG_info("Thread B");
```

**Recommendation:** Use file API for multi-threaded daemons.

---

## Platform-Specific Considerations

### Boot Scripts

Boot scripts run **before SD card is fully mounted**, so they:

- Use simple `echo >> file.log` instead of `log.sh`
- Write to boot partition: `/mnt/mmc/boot.log` (RG35XX Plus)
- Or TF1 partition before TF2 is mounted

### Install Scripts

Install scripts run **after SD card is mounted**, so they can use `log.sh`:

```bash
source "$SYSTEM_PATH/common/log.sh"
log_init "$SDCARD_PATH/install.log"
log_info "Installing LessUI..."
```

### Daemons (keymon, batmon, lumon)

Daemons run continuously in background:

**Option 1: Shell redirection (current)**

```bash
keymon.elf &> $LOGS_PATH/keymon.log &
```

**Option 2: Direct file logging**

```c
int main() {
    LogFile* log = log_file_open("/tmp/keymon.log", 1024*1024, 3);

    while(1) {
        int battery = getBattery();
        if (battery < 10) {
            log_file_write(log, LOG_LEVEL_WARN, "Battery low: %d%%", battery);
        }
    }

    log_file_close(log);
}
```

---

## Common Patterns

### Pattern 1: File Operations

```c
FILE* fp = fopen(path, "r");
if (!fp) {
    LOG_errno("Failed to open %s", path);
    return -1;
}

// Use file...

fclose(fp);
```

### Pattern 2: Memory Allocation

```c
void* buf = malloc(size);
if (!buf) {
    LOG_error("Failed to allocate %zu bytes for %s", size, name);
    return NULL;
}
```

### Pattern 3: System Calls

```c
int ret = ioctl(fd, IOCTL_SAR_INIT, NULL);
if (ret < 0) {
    LOG_errno("SAR ADC initialization failed");
    return -1;
}
```

### Pattern 4: Conditional Warnings

```c
if (!is_charging && battery < 10) {
    static int last_warn = 100;
    if (battery < last_warn) {  // Only warn when dropping
        LOG_warn("Battery low: %d%%", battery);
        last_warn = battery;
    }
}
```

### Pattern 5: Config Loading

```c
LOG_info("Loading configuration for device: %s", device_tag);

FILE* fp = fopen(config_path, "r");
if (!fp) {
    LOG_warn("Config file not found, using defaults: %s", config_path);
    return load_defaults();
}

LOG_info("Loaded user config: %s", config_path);
```

---

## Troubleshooting Guide (For Users)

### Where Are My Logs?

**Main launcher log:**

```
/mnt/SDCARD/.userdata/<platform>/logs/launcher.log
```

**Emulator logs:**

```
/mnt/SDCARD/.userdata/<platform>/logs/GB.log
/mnt/SDCARD/.userdata/<platform>/logs/GBA.log
... (one per system)
```

**System logs:**

```
/mnt/SDCARD/boot.log      - Boot sequence
/mnt/SDCARD/install.log   - Installation/updates
```

**Daemon logs:**

```
/mnt/SDCARD/.userdata/<platform>/logs/keymon.log  - Button/battery monitoring
```

### Common Issues

**Problem: Emulator crashes immediately**

```
Check: /mnt/SDCARD/.userdata/miyoomini/logs/GB.log
Look for: [ERROR] lines showing core loading failures
```

**Problem: ROM doesn't appear in list**

```
Check: /mnt/SDCARD/.userdata/miyoomini/logs/launcher.log
Look for: File system errors, permission issues
```

**Problem: Save states not working**

```
Check: /mnt/SDCARD/.userdata/miyoomini/logs/PS.log (or appropriate core)
Look for: [ERROR] lines in save state operations
```

**Problem: Installation failed**

```
Check: /mnt/SDCARD/install.log
Look for: Extract errors, permission issues, disk space
```

**Problem: Won't boot after update**

```
Check: /mnt/mmc/boot.log (on RG35XX Plus)
       /mnt/SDCARD/boot.log (on other platforms)
Look for: Mount failures, missing files
```

### Clearing Logs

To free up SD card space:

```bash
# Remove all logs
rm -rf /mnt/SDCARD/.userdata/*/logs/*.log*
rm -f /mnt/SDCARD/boot.log* /mnt/SDCARD/install.log*

# Remove just old backups
rm -f /mnt/SDCARD/.userdata/*/logs/*.log.[0-9]
```

---

## Developer Guidelines

### Adding Logging to New Code

**Rule 1: Every file operation must have error logging**

```c
// ❌ BAD - Silent failure
FILE* fp = fopen(path, "r");
if (!fp) return NULL;

// ✅ GOOD - Logged error
FILE* fp = fopen(path, "r");
if (!fp) {
    LOG_errno("Failed to open %s", path);
    return NULL;
}
```

**Rule 2: Every malloc must have error logging**

```c
// ❌ BAD
void* buf = malloc(size);
if (!buf) return NULL;

// ✅ GOOD
void* buf = malloc(size);
if (!buf) {
    LOG_error("Failed to allocate %zu bytes", size);
    return NULL;
}
```

**Rule 3: Log key events at INFO level**

```c
// Application startup
LOG_info("Starting Launcher on %s", PLATFORM);

// Major operations
LOG_info("Loading ROM: %s", rom_name);
LOG_info("Saved state to slot %d", slot);
```

**Rule 4: Never add \n to log messages**

```c
// ❌ BAD - Double newline
LOG_error("Failed to open file\n");

// ✅ GOOD - Newline added automatically
LOG_error("Failed to open file");
```

**Rule 5: Use LOG_errno for system call failures**

```c
// ❌ VERBOSE
if (!fp) {
    LOG_error("Failed to open %s: %s", path, strerror(errno));
}

// ✅ CONCISE
if (!fp) {
    LOG_errno("Failed to open %s", path);
}
```

### Testing Logging

**Write tests that verify error handling:**

```c
void test_function_logs_error_on_failure(void) {
    // Capture stderr
    // Call function with invalid input
    // Verify LOG_error was called
    // Verify error message contains useful context
}
```

**See:** `tests/unit/all/common/test_log.c` for comprehensive examples

---

## Performance Considerations

### Compile-Time Filtering

Disabled log levels have **zero runtime overhead**:

```c
// Without -DENABLE_DEBUG_LOGS:
LOG_debug("Value: %d", x);
// Compiles to: ((void)0)  // No code generated!
```

### Conditional Verbose Logging

For expensive operations, guard with `#ifdef`:

```c
#ifdef ENABLE_DEBUG_LOGS
LOG_debug("Dumping buffer (%zu bytes):", size);
for (int i = 0; i < size; i++) {
    LOG_debug("  [%d] = 0x%02x", i, buf[i]);
}
#endif
```

### Recommended Configuration

**Production builds (release ZIPs):**

```makefile
# launcher/player - ERROR + WARN + INFO
CFLAGS += -DENABLE_INFO_LOGS

# keymon/daemons - ERROR + WARN only
# (no flags)
```

**Development builds:**

```makefile
# All levels for debugging
CFLAGS += -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS
```

---

## Examples by Component

### launcher (Launcher)

```c
// Startup
LOG_info("Starting Launcher on %s", PLATFORM);

// ROM selection
LOG_info("Opening ROM: %s", rom_path);

// Errors
if (!dir) {
    LOG_errno("Failed to open directory %s", dir_path);
}

// Warnings
if (recent_count == 0) {
    LOG_info("No recent games found");
}
```

### player (Libretro Frontend)

```c
// Core loading
LOG_info("Loading configuration for device: %s", device_tag);
LOG_info("Loaded user config: %s", config_path);

// Save operations
if (!save_state_success) {
    LOG_error("Failed to create save state: %s", strerror(errno));
}

// SRAM writes
if (sram_write_failed) {
    LOG_error("Error writing SRAM data to file");
}
```

### keymon (Hardware Daemon)

```c
// Battery warnings
if (!charging && battery <= 5) {
    LOG_warn("Battery critically low: %d%%", battery);
}

// I2C errors
int val = axp_read(0x00);
if (val < 0) {
    LOG_error("Failed to read AXP223 register 0x00");
}

// Hardware errors
if (input_fd < 0) {
    LOG_error("Failed to open input device: /dev/input/event0");
    quit(EXIT_FAILURE);
}
```

### Boot Scripts

```bash
#!/bin/sh
source "$SYSTEM_PATH/common/log.sh"
log_init "$SDCARD_PATH/boot.log"

log_info "Initializing hardware..."

if ! mount /dev/mmcblk0p1 /mnt/sdcard; then
    log_error "Failed to mount SD card"
    poweroff
fi

log_info "SD card mounted successfully"
```

---

## Migration Checklist

When adding logging to existing code:

- [ ] Add `#include "log.h"` to C files
- [ ] Add `#include <errno.h>` and `#include <string.h>` for LOG_errno
- [ ] Add `../common/log.c` to SOURCE in makefile
- [ ] Replace silent failures with LOG_error/LOG_errno
- [ ] Add LOG_warn for edge cases
- [ ] Add LOG_info for key events
- [ ] Remove all `\n` from log messages
- [ ] Test that logs appear in expected files
- [ ] Verify rotation works (for file API)

---

## FAQ

**Q: Why are my DEBUG logs not appearing?**

A: DEBUG logs require `-DENABLE_DEBUG_LOGS` in makefile CFLAGS. By default, only ERROR and WARN are compiled in production.

**Q: Can I log from interrupt handlers?**

A: No. The logging system uses mutexes and file I/O, which aren't safe in interrupt context. Use atomic flags instead.

**Q: How do I view logs on the device?**

A: Connect via SSH (if enabled) or remove SD card and view on a computer. Logs are plain text files.

**Q: Will logging slow down my emulator?**

A: INFO/DEBUG logs in tight loops can impact performance. Use them only for initialization and major events. ERROR/WARN have minimal overhead since they're rare.

**Q: Can I disable logging entirely?**

A: ERROR and WARN are always compiled (minimal overhead). INFO and DEBUG can be disabled by omitting `-DENABLE_*_LOGS` flags.

**Q: What happens if the log file can't be created?**

A: `log_file_open()` returns NULL. Check for NULL and fall back to console logging or continue without logging.

**Q: Do logs survive power loss?**

A: Yes, if using file API or shell redirection. Logs are flushed immediately after each message. However, logs in memory-only locations (`/tmp/`) are lost.

---

## Best Practices Summary

✅ **DO:**

- Log all errors with context (file:line, errno)
- Log warnings for edge cases
- Log key events at INFO level
- Use LOG_errno for system call failures
- Let the library add newlines (no `\n`)
- Flush immediately (library does this)
- Use file API for daemons needing rotation

❌ **DON'T:**

- Add `\n` to log messages (auto-added)
- Log per-frame events at INFO level
- Log normal user actions
- Forget to link log.c in makefile
- Use printf/fprintf for logging (use LOG\_\* macros)
- Log from interrupt handlers
- Forget to check log_file_open() return value

---

## Related Documentation

- `workspace/all/common/log.h` - Complete API reference
- `tests/unit/all/common/test_log.c` - Example usage and tests
- `skeleton/SYSTEM/common/log.sh` - Shell logging library
- `tests/README.md` - Testing guide
