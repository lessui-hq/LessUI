# LessUI

> **Built on the shoulders of giants:** LessUI is a fork of [MinUI](https://github.com/shauninman/MinUI), the brilliant work of [Shaun Inman](https://github.com/shauninman). Shaun's vision of a distraction-free, beautifully simple retro gaming experience is what makes this project possible. This fork exists to explore new directions while honoring the elegance and philosophy of the original.

LessUI is a focused, custom launcher and libretro frontend for [a variety of retro handhelds](#supported-devices).

<img src="github/minui-main.png" width=320 /> <img src="github/minui-menu-gbc.png" width=320 />

## Features

- Simple launcher, simple SD card
- No settings or configuration
- No boxart, themes, or distractions
- Automatically hides hidden files
  and extension and region/version
  cruft in display names
- Consistent in-emulator menu with
  quick access to save states, disc
  changing, and emulator options
- Automatically sleeps after 30 seconds
  or press POWER to sleep (and wake)
- Automatically powers off while asleep
  after two minutes or hold POWER for
  one second
- Automatically resumes right where
  you left off if powered off while
  in-game, manually or while asleep
- Resume from manually created, last
  used save state by pressing X in
  the launcher instead of A
- Streamlined emulator frontend
  (minarch + libretro cores)
- Single SD card compatible with
  multiple devices from different
  manufacturers

**[Download the latest release](https://github.com/nchapman/LessUI/releases)**

## Installation

1. Download the latest release (base ZIP for your device)
2. Extract to a freshly formatted FAT32 SD card
3. Follow device-specific instructions in the included `README.txt`
4. Insert SD card and boot your device

For detailed installation steps and device-specific setup, see the `README.txt` included in the base download.

> [!TIP]
> Devices with a physical power switch use MENU to sleep and wake instead of POWER. Once asleep, the device can be safely powered off with the switch.

## Supported Consoles

**Base consoles** (included with every release):
- Game Boy
- Game Boy Color
- Game Boy Advance
- Nintendo Entertainment System
- Super Nintendo Entertainment System
- Sega Genesis
- PlayStation

**Extra consoles** (optional download):
- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear
- Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy

## Supported Devices

| Manufacturer | Device | Released | Status |
|--------------|--------|----------|--------|
| **Anbernic** | RG35XX | December 2022 | ✓ |
| **Anbernic** | RG35XX Plus | November 2023 | ✓ |
| **Anbernic** | RG35XXH | January 2024 | ✓ |
| **Anbernic** | RG28XX | April 2024 | ✓ |
| **Anbernic** | RG35XXSP | May 2024 | ✓ |
| **Anbernic** | RG40XXH | July 2024 | ✓ |
| **Anbernic** | RG40XXV | August 2024 | ✓ |
| **Anbernic** | RG CubeXX | October 2024 | ✓ |
| **Anbernic** | RG34XX | December 2024 | ✓ |
| **Anbernic** | RG34XXSP | May 2025 | ✓ |
| **Miyoo** | Mini | December 2021 | ✓ |
| **Miyoo** | Mini Plus | February 2023 | ✓ |
| **Miyoo** | A30 | May 2024 | ✓ |
| **Miyoo** | Mini Flip | October 2024 | ✓ |
| **Miyoo** | Flip | January 2025 | ✓ |
| **Trimui** | Smart | October 2022 | ✓ |
| **Trimui** | Smart Pro | November 2023 | ✓ |
| **Trimui** | Brick | October 2024 | ✓ |
| **Powkiddy** | RGB30 | August 2023 | ✓ |
| **MagicX** | Mini Zero 28 | January 2025 | ✓ |
| **GKD** | Pixel | January 2024 | Deprecated |
| **Generic** | M17 | October 2023 | Deprecated |
| **MagicX** | XU Mini M | May 2024 | Deprecated |

> [!NOTE]
> Deprecated devices will continue to work with current LessUI releases but will not receive new features or platform-specific fixes.

---

## For Developers

Want to build LessUI, create custom paks, or understand how it works?

- **[Development Guide](docs/DEVELOPMENT.md)** - Building, testing, and contributing
- **[Architecture Guide](docs/ARCHITECTURE.md)** - How LessUI works internally
- **[Cores Guide](docs/CORES.md)** - How libretro cores are built and loaded
- **[Pak Development Guide](docs/PAKS.md)** - Creating custom emulator and tool paks
- **[Platform Documentation](workspace/)** - Technical hardware details for each device
