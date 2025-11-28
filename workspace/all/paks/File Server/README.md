# File Server

Web-based file server for LessUI. Allows uploading and downloading files via a web browser.

## LessUI Integration

This pak has been integrated into LessUI from Jose Gonzalez's minui-dufs-server-pak.

**Original source:** https://github.com/josegonzalez/minui-dufs-server-pak

**Forked from commit:** `2171b4240133904721ffe25f8f450e0e7ab26142`

**Changes made for LessUI:**
- Renamed from "HTTP Server" to "File Server" for clarity
- Adapted pak.json to match LessUI schema (removed author, repo_url, release_filename fields)
- Updated version to 1.0.0
- Follows LessUI pak directory structure in `workspace/all/paks/`

## Overview

File Server runs a web server ([dufs](https://github.com/sigoden/dufs/)) that lets you:
- Browse device files from your web browser
- Upload files to your device
- Download files from your device
- Toggle server on/off
- Auto-start on boot

## Requirements

**Platforms:** miyoomini, my282, my355, rg35xxplus, tg5040

**WiFi:** Device must be connected to WiFi (same network as your computer)

## Usage

1. Connect device to WiFi using the Wifi pak
2. Launch `Tools > File Server`
3. Toggle "Enabled" to start the server
4. Note the IP address shown in settings
5. Open web browser on your computer
6. Navigate to `http://<device-ip>` (e.g., `http://192.168.1.100`)
7. Browse, upload, and download files

### Features

- **Enable/Disable Server** - Toggle the web server on/off
- **Start on Boot** - Automatically start server when device boots
- **Port Configuration** - Change server port (default: 80)
- **Full SD Card Access** - Browse entire SD card from browser

### Configuration

**Custom Port:** Create `$SDCARD_PATH/.userdata/$PLATFORM/File Server/port` with desired port number

**Logs:** Debug logs written to `$LOGS_PATH/File Server.txt`

## Technical Details

- Powered by [dufs](https://github.com/sigoden/dufs/) - A lightweight HTTP file server
- Runs as background service
- Auto-start via MinUI's auto.sh mechanism
- Supports ARM32 and ARM64 architectures

## Credits

- **Original pak:** [Jose Gonzalez](https://github.com/josegonzalez) - https://github.com/josegonzalez/minui-dufs-server-pak
- **dufs server:** [sigoden](https://github.com/sigoden) - https://github.com/sigoden/dufs
- **LessUI integration:** Adapted for LessUI pak architecture
