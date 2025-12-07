# ADBD.pak

Enable ADB (Android Debug Bridge) over WiFi on Miyoo Mini Plus.

## Overview

This pak enables wireless ADB connections on the Miyoo Mini Plus, allowing you to:
- Access the device shell remotely
- Push/pull files over WiFi
- Debug applications
- Run adb commands without USB connection

## Platform Support

- Miyoo Mini Plus only (requires WiFi hardware)

## Implementation

The pak performs the following steps:

1. Loads the WiFi kernel module (8188fu.ko)
2. Enables WiFi hardware using axp_test
3. Brings up the wlan0 interface
4. Starts wpa_supplicant with existing WiFi configuration
5. Obtains IP address via DHCP
6. Bind-mounts a minimal profile to prevent system re-initialization
7. Launches adbd daemon

## Usage

1. Ensure WiFi is configured on your Miyoo Mini Plus (stock WiFi settings or Wifi.pak)
2. Launch ADBD.pak from the Tools menu
3. Note your device's IP address from WiFi settings
4. On your computer, run: `adb connect <device-ip>:5555`

## Files

- `launch.sh` - Main entry point
- `miyoomini/adbd` - ADB daemon binary
- `miyoomini/8188fu.ko` - WiFi kernel module (RTL8188FU driver)
- `miyoomini/libcrypto.so.1.1` - OpenSSL crypto library for adbd
- `miyoomini/profile` - Minimal /etc/profile replacement

## Technical Notes

The pak uses a bind mount to replace `/etc/profile` with a minimal stub. This prevents the stock system from re-initializing all services when adbd sources the profile, which would interfere with the minimal environment.

## Architecture

This pak follows the cross-platform pak architecture in `workspace/all/paks/`.
