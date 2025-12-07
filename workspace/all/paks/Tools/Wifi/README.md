# Wifi.pak

Manage WiFi settings on supported devices.

## Features

- Enable/disable WiFi
- Scan and connect to networks
- Save multiple network credentials
- Auto-start WiFi on boot
- Forget saved networks

## Supported Platforms

- **miyoomini** - Miyoo Mini Plus only (not regular Mini)
- **my282** - Miyoo A30
- **my355** - Miyoo Flip
- **rg35xxplus** - RG-35XX Plus, RG-34XX, RG-35XX H, RG-35XX SP (not RG28XX)
- **tg5040** - Trimui Brick, Trimui Smart Pro

## Usage

1. Launch Wifi.pak from Tools menu
2. Enable WiFi toggle
3. Select "Connect to network"
4. Choose a network and enter password
5. Optionally enable "Start on boot"

Credentials are stored in `wifi.txt` at the SD card root.

## Credential File Format

```
# Comments start with hash
MyNetwork:MyPassword
OpenNetwork:
AnotherNetwork:AnotherPassword
```

## Attribution

Based on [minui-wifi-pak](https://github.com/josegonzalez/minui-wifi-pak) by Jose Gonzalez.

Forked from commit: `ff5fae810d6a3a0f5c612ebcd78ac2a6c568d3d4`

## Platform-Specific Notes

### miyoomini
- Only works on Miyoo Mini **Plus** (has WiFi hardware)
- Bundles `iw` tool and required libraries (libnl, libssl)
- Loads 8188fu.ko WiFi kernel module

### rg35xxplus
- Uses systemd-networkd and netplan (Ubuntu-style networking)
- Does not support RG28XX variant

### my355
- Uses wpa_cli instead of iw for network scanning
