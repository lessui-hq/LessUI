# Shutdown Architecture

How device shutdown works in LessUI.

## Shutdown Triggers

### 1. keymon (MENU+POWER)

keymon daemon monitors for MENU+POWER combo and calls shutdown directly:

```c
// workspace/*/keymon/keymon.c
system("shutdown");
while (1) pause();
```

Works regardless of what app is running.

### 2. Power Button Hold (1 second)

Applications call `PWR_update()` each frame, which detects power held for 1 second:

```c
// workspace/all/common/api.c - PWR_update()
if (power_pressed_at && now - power_pressed_at >= 1000) {
    if (before_sleep) before_sleep();
    PWR_powerOff();
}
```

`PWR_powerOff()` shows "Powering off" message and calls `PLAT_powerOff()`:

```c
void PWR_powerOff(void) {
    if (pwr.can_poweroff) {
        // ... show message ...
        PLAT_powerOff();
    }
}
```

### 3. Auto-shutdown (after 2 min sleep)

If device is sleeping for 2 minutes and not charging:

```c
// workspace/all/common/api.c - PWR_waitForWake()
if (pwr.can_poweroff && SDL_GetTicks() - sleep_ticks >= 120000) {
    if (pwr.is_charging)
        sleep_ticks += 60000;  // check again in a minute
    else
        PWR_powerOff();
}
```

### 4. Launcher Script Fallback

MinUI.pak/launch.sh has shutdown at the end as a safety net:

```sh
while [ -f "$EXEC_PATH" ]; do
    # ... main loop ...
done

shutdown  # just in case
```

## PLAT_powerOff() Implementation

All platforms (except physical switch devices) call `system("shutdown")` directly:

```c
void PLAT_powerOff(void) {
    sleep(2);
    // ... cleanup (mute, backlight off, quit subsystems) ...
    system("shutdown");
    while (1) pause();
}
```

This ensures shutdown works consistently regardless of which process triggers it
(minui, minarch, shui daemon, or tool paks).

### Physical Power Switch Platforms

trimuismart and m17 have physical power switches and can't actually power off.
They enter a low-power mode instead:

```c
void PLAT_powerOff(void) {
    // ... cleanup ...
    touch("/tmp/poweroff");
    exit(0);
}
```

The launcher script checks for this marker and handles low-power mode:

```sh
if [ -f "/tmp/poweroff" ]; then
    rm -f "/tmp/poweroff"
    killall keymon.elf
    # ... enter low power mode ...
fi
```

## Shutdown Scripts

Each platform has a `shutdown` script in `skeleton/SYSTEM/<platform>/bin/`:

| Platform    | Method                                               |
| ----------- | ---------------------------------------------------- |
| miyoomini   | Plus: `sync && poweroff`; Non-plus: `sync && reboot` |
| tg5040      | AXP register writes for hardware power-off           |
| rg35xxplus  | `reboot -f`                                          |
| rg35xx      | `sync && poweroff`                                   |
| rgb30       | `poweroff`                                           |
| zero28      | Clear framebuffer, `poweroff`                        |
| my282       | `poweroff`                                           |
| my355       | `poweroff`                                           |
| magicmini   | `poweroff`                                           |
| trimuismart | Just saves datetime (physical switch)                |
| m17         | Just saves datetime (physical switch)                |

## Who Calls PWR_update()

| Context                | Calls PWR_update()?       |
| ---------------------- | ------------------------- |
| minui main loop        | Yes                       |
| minarch game loop      | Yes                       |
| minarch in-game menu   | Yes                       |
| shui ui_list           | Yes                       |
| shui ui_message        | Yes                       |
| shui ui_keyboard       | Yes                       |
| shui daemon idle loop  | No                        |
| Tool pak shell scripts | No (they're shell, not C) |

## Summary

| Trigger               | Path                             | Works everywhere? |
| --------------------- | -------------------------------- | ----------------- |
| MENU+POWER            | keymon → system("shutdown")      | Yes               |
| Power hold in minui   | PWR_powerOff() → PLAT_powerOff() | Yes               |
| Power hold in minarch | PWR_powerOff() → PLAT_powerOff() | Yes               |
| Power hold in shui    | PWR_powerOff() → PLAT_powerOff() | Yes               |
| 2 min sleep timeout   | PWR_powerOff() → PLAT_powerOff() | Yes               |
