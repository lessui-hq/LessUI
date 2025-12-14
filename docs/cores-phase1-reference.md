# LessUI Phase 1 Cores Reference

This document contains technical specifications for all Phase 1 libretro cores supported by LessUI.

---

## Table of Contents

1. [Game Boy / Game Boy Color (gambatte)](#game-boy--game-boy-color-gambatte)
2. [Game Boy Advance (gpsp)](#game-boy-advance-gpsp)
3. [Game Boy Advance / Super Game Boy (mgba)](#game-boy-advance--super-game-boy-mgba)
4. [Nintendo Entertainment System (fceumm)](#nintendo-entertainment-system-fceumm)
5. [Super Nintendo (snes9x2005_plus)](#super-nintendo-snes9x2005_plus)
6. [Super Nintendo Alt (mednafen_supafaust)](#super-nintendo-alt-mednafen_supafaust)
7. [Sega Genesis / Game Gear / Master System (picodrive)](#sega-genesis--game-gear--master-system-picodrive)
8. [Sony PlayStation (pcsx_rearmed)](#sony-playstation-pcsx_rearmed)
9. [TurboGrafx-16 / PC Engine CD (mednafen_pce_fast)](#turbografx-16--pc-engine-cd-mednafen_pce_fast)
10. [Nintendo Virtual Boy (mednafen_vb)](#nintendo-virtual-boy-mednafen_vb)
11. [Neo Geo Pocket / Color (race)](#neo-geo-pocket--color-race)
12. [Pokemon mini (pokemini)](#pokemon-mini-pokemini)
13. [Pico-8 (fake08)](#pico-8-fake08)
14. [TIC-80 (tic80)](#tic-80-tic80)
15. [MAME Arcade (mame2003_plus)](#mame-arcade-mame2003_plus)
16. [FBNeo Arcade / Neo Geo / CPS (fbneo)](#fbneo-arcade--neo-geo--cps-fbneo)

---

## Game Boy / Game Boy Color (gambatte)

**Core:** `gambatte_libretro.so`
**Systems:** GB, GBC

### Extensions

| Extension | Description         |
| --------- | ------------------- |
| `.gb`     | Game Boy ROMs       |
| `.gbc`    | Game Boy Color ROMs |
| `.dmg`    | Game Boy ROMs (alt) |

### BIOS Files

| Filename       | Description               | MD5                                | Required |
| -------------- | ------------------------- | ---------------------------------- | -------- |
| `gb_bios.bin`  | Game Boy bootloader       | `32fbbd84168d3482956eb3c5051637f5` | Optional |
| `gbc_bios.bin` | Game Boy Color bootloader | `dbfce9db9deaa2567f6a84fde55f9680` | Optional |

### Base Resolution

**160×144** (10:9 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| Start       | Start                 |
| Select      | Select                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Game Boy Advance (gpsp)

**Core:** `gpsp_libretro.so`
**Systems:** GBA

### Extensions

| Extension | Description           |
| --------- | --------------------- |
| `.gba`    | Game Boy Advance ROMs |
| `.bin`    | Binary ROMs           |

### BIOS Files

| Filename       | Description | MD5                                | Required    |
| -------------- | ----------- | ---------------------------------- | ----------- |
| `gba_bios.bin` | GBA BIOS    | `a860e8c0b6d573d191e4ec7db1b1e4f6` | Recommended |

### Base Resolution

**240×160** (3:2 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| L           | Left Shoulder         |
| R           | Right Shoulder        |
| Start       | Start                 |
| Select      | Select                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind L = L1
bind R = R1
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Game Boy Advance / Super Game Boy (mgba)

**Core:** `mgba_libretro.so`
**Systems:** MGBA (GBA alt), SGB

### Extensions

| Extension | Description           |
| --------- | --------------------- |
| `.gba`    | Game Boy Advance ROMs |
| `.gb`     | Game Boy ROMs         |
| `.gbc`    | Game Boy Color ROMs   |

### BIOS Files

| Filename       | Description         | MD5                                | Required           |
| -------------- | ------------------- | ---------------------------------- | ------------------ |
| `gba_bios.bin` | GBA BIOS            | `a860e8c0b6d573d191e4ec7db1b1e4f6` | Optional           |
| `gb_bios.bin`  | Game Boy BIOS       | `32fbbd84168d3482956eb3c5051637f5` | Optional           |
| `gbc_bios.bin` | Game Boy Color BIOS | `dbfce9db9deaa2567f6a84fde55f9680` | Optional           |
| `sgb_bios.bin` | Super Game Boy BIOS | `d574d4f9c12f305074798f54c091a8b4` | Optional (for SGB) |

### Base Resolution

- **GBA:** 240×160 (3:2)
- **GB/GBC:** 160×144 (10:9)
- **SGB:** 256×224 with borders

### Controller Mapping

| Core Button | Description               |
| ----------- | ------------------------- |
| A           | A Button                  |
| B           | B Button                  |
| L           | Left Shoulder (GBA only)  |
| R           | Right Shoulder (GBA only) |
| Start       | Start                     |
| Select      | Select                    |
| D-Pad       | Up, Down, Left, Right     |

**Suggested LessUI Config (GBA/MGBA):**

```
bind A = A
bind B = B
bind L = L1
bind R = R1
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

**Suggested LessUI Config (SGB):**

```
bind A = A
bind B = B
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Nintendo Entertainment System (fceumm)

**Core:** `fceumm_libretro.so`
**Systems:** FC (NES/Famicom)

### Extensions

| Extension | Description                      |
| --------- | -------------------------------- |
| `.nes`    | Standard NES ROMs                |
| `.fds`    | Famicom Disk System              |
| `.unif`   | Universal NES Image Format       |
| `.unf`    | Universal NES Image Format (alt) |

### BIOS Files

| Filename        | Description | MD5                                | Required             |
| --------------- | ----------- | ---------------------------------- | -------------------- |
| `disksys.rom`   | FDS BIOS    | `ca30b50f880eb660a320674ed365ef7a` | **Required** for FDS |
| `gamegenie.nes` | Game Genie  | `7f98d77d7a094ad7d069b74bd553ec98` | Optional             |

### Base Resolution

**256×240** (8:7 PAR, 4:3 DAR)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| Start       | Start                 |
| Select      | Select                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Super Nintendo (snes9x2005_plus)

**Core:** `snes9x2005_plus_libretro.so`
**Systems:** SFC (SNES/Super Famicom)

### Extensions

| Extension | Description              |
| --------- | ------------------------ |
| `.smc`    | Super Magicom            |
| `.sfc`    | Super Famicom (standard) |
| `.fig`    | Pro Fighter              |
| `.swc`    | Super Wild Card          |
| `.gd3`    | Game Doctor SF3          |
| `.gd7`    | Game Doctor SF7          |
| `.dx2`    | Game Doctor SF6/SF7      |
| `.bsx`    | BS-X Satellaview         |

### BIOS Files

None required.

### Base Resolution

**256×224** (8:7 PAR, 4:3 DAR)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| X           | X Button              |
| Y           | Y Button              |
| L           | Left Shoulder         |
| R           | Right Shoulder        |
| Start       | Start                 |
| Select      | Select                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind X = X
bind Y = Y
bind L = L1
bind R = R1
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Super Nintendo Alt (mednafen_supafaust)

**Core:** `mednafen_supafaust_libretro.so`
**Systems:** SUPA (SNES alt, higher accuracy)

### Extensions

| Extension | Description              |
| --------- | ------------------------ |
| `.smc`    | Super Magicom            |
| `.sfc`    | Super Famicom (standard) |
| `.swc`    | Super Wild Card          |
| `.fig`    | Pro Fighter              |

### BIOS Files

None required.

### Base Resolution

**256×224** (standard), up to 512×478 (hi-res modes)

### Controller Mapping

Same as snes9x2005_plus (see above).

**Note:** Supports DSP-1/2, CX4, SuperFX, SA-1, S-DD1, MSU1 special chips.

---

## Sega Genesis / Game Gear / Master System (picodrive)

**Core:** `picodrive_libretro.so`
**Systems:** MD, GG, SMS

### Extensions

**Genesis/Mega Drive (MD):**
| Extension | Description |
|-----------|-------------|
| `.bin` | Binary ROM |
| `.gen` | Genesis ROM |
| `.smd` | Super Magic Drive |
| `.md` | Mega Drive ROM |
| `.68k` | 68000 ROM |

**Sega CD:**
| Extension | Description |
|-----------|-------------|
| `.cue` | Cue sheet |
| `.iso` | ISO image |
| `.chd` | Compressed disc |

**Game Gear (GG) / Master System (SMS):**
| Extension | Description |
|-----------|-------------|
| `.sms` | Master System ROM |
| `.gg` | Game Gear ROM |

### BIOS Files

| Filename        | Description     | MD5                                | Required    |
| --------------- | --------------- | ---------------------------------- | ----------- |
| `bios_CD_U.bin` | Sega CD US BIOS | `2efd74e3232ff260e371b99f84024f7f` | For Sega CD |
| `bios_CD_E.bin` | Mega CD EU BIOS | `e66fa1dc5820d254611fdcdba0662372` | For Mega CD |
| `bios_CD_J.bin` | Mega CD JP BIOS | `278a9397d192149e84e820ac621a8edd` | For Mega CD |

**Note:** No BIOS required for Genesis, Game Gear, or Master System cartridge games.

### Base Resolution

**320×224** (all systems)

### Controller Mapping

**Genesis/Mega Drive 3-Button:**
| Core Button | Description |
|-------------|-------------|
| A | A Button |
| B | B Button |
| C | C Button |
| Start | Start |
| D-Pad | Up, Down, Left, Right |

**Genesis/Mega Drive 6-Button:**
| Core Button | Description |
|-------------|-------------|
| A | A Button |
| B | B Button |
| C | C Button |
| X | X Button |
| Y | Y Button |
| Z | Z Button |
| Start | Start |
| Mode | Mode |
| D-Pad | Up, Down, Left, Right |

**Suggested LessUI Config (MD 6-button):**

```
bind A = Y
bind B = B
bind C = A
bind X = X
bind Y = L1
bind Z = R1
bind Start = START
bind Mode = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

**Master System / Game Gear:**
| Core Button | Description |
|-------------|-------------|
| Button 1 | Button 1 / Start |
| Button 2 | Button 2 |
| Pause | Pause (SMS) / Start (GG) |
| D-Pad | Up, Down, Left, Right |

**Suggested LessUI Config (SMS/GG):**

```
bind Button 1 = B
bind Button 2 = A
bind Pause = START
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Sony PlayStation (pcsx_rearmed)

**Core:** `pcsx_rearmed_libretro.so`
**Systems:** PS (PSX/PS1)

### Extensions

| Extension | Description         |
| --------- | ------------------- |
| `.bin`    | Raw CD image        |
| `.cue`    | Cue sheet           |
| `.img`    | Disk image          |
| `.iso`    | ISO image           |
| `.pbp`    | PSP EBOOT           |
| `.chd`    | Compressed disc     |
| `.m3u`    | Multi-disc playlist |
| `.mdf`    | Media Descriptor    |
| `.ccd`    | CloneCD             |
| `.exe`    | PS executable       |

### BIOS Files

| Filename       | Description        | MD5                                | Required     |
| -------------- | ------------------ | ---------------------------------- | ------------ |
| `scph5501.bin` | PS1 BIOS v3.0 (US) | `490f666e1afb15b7362b406ed1cea246` | **Required** |
| `scph5500.bin` | PS1 BIOS v3.0 (JP) | -                                  | Alternative  |
| `scph5502.bin` | PS1 BIOS v3.0 (EU) | -                                  | Alternative  |
| `scph1001.bin` | PS1 BIOS v2.0 (US) | `924e392ed05558ffdb115408c263dccf` | Alternative  |
| `scph7001.bin` | PS1 BIOS v4.1 (US) | `1e68c231d0896b7eadcad1d7d8e76129` | Alternative  |
| `scph101.bin`  | PS1 BIOS v4.4 (US) | `6E3735FF4C7DC899EE98981385F6F3D0` | Alternative  |

### Base Resolution

**320×240** (4:3 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| Cross       | Cross (×)             |
| Circle      | Circle (○)            |
| Square      | Square (□)            |
| Triangle    | Triangle (△)          |
| L1          | Left Shoulder         |
| L2          | Left Trigger          |
| R1          | Right Shoulder        |
| R2          | Right Trigger         |
| L3          | Left Stick Click      |
| R3          | Right Stick Click     |
| Start       | Start                 |
| Select      | Select                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind Cross = A
bind Circle = B
bind Square = Y
bind Triangle = X
bind L1 = L1
bind L2 = L2
bind R1 = R1
bind R2 = R2
bind Start = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## TurboGrafx-16 / PC Engine CD (mednafen_pce_fast)

**Core:** `mednafen_pce_fast_libretro.so`
**Systems:** PCE, PCECD

### Extensions

**HuCard (PCE):**
| Extension | Description |
|-----------|-------------|
| `.pce` | PC Engine ROM |

**CD-ROM (PCECD):**
| Extension | Description |
|-----------|-------------|
| `.cue` | Cue sheet |
| `.ccd` | CloneCD |
| `.iso` | ISO image |
| `.img` | Raw image |
| `.bin` | Binary image |
| `.chd` | Compressed disc |

### BIOS Files

| Filename       | Description                | MD5                                | Required            |
| -------------- | -------------------------- | ---------------------------------- | ------------------- |
| `syscard3.pce` | Super CD-ROM2 System V3.xx | `38179df8f4ac870017db21ebcbf53114` | **Required** for CD |

**Note:** HuCard games do not require BIOS.

### Base Resolution

**512×243** (6:5 aspect ratio)

### Controller Mapping

**2-Button Pad:**
| Core Button | Description |
|-------------|-------------|
| I | Button I |
| II | Button II |
| Run | Run |
| Select | Select |
| D-Pad | Up, Down, Left, Right |

**6-Button Pad:**
| Core Button | Description |
|-------------|-------------|
| I | Button I |
| II | Button II |
| III | Button III |
| IV | Button IV |
| V | Button V |
| VI | Button VI |
| Run | Run |
| Select | Select |
| D-Pad | Up, Down, Left, Right |

**Suggested LessUI Config (2-button):**

```
bind I = A
bind II = B
bind Run = START
bind Select = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Nintendo Virtual Boy (mednafen_vb)

**Core:** `mednafen_vb_libretro.so`
**Systems:** VB

### Extensions

| Extension | Description     |
| --------- | --------------- |
| `.vb`     | Virtual Boy ROM |
| `.vboy`   | Virtual Boy ROM |
| `.bin`    | Binary ROM      |

### BIOS Files

None required.

### Base Resolution

**384×224** (12:7 aspect ratio)

### Controller Mapping

| Core Button | Description                          |
| ----------- | ------------------------------------ |
| A           | A Button                             |
| B           | B Button                             |
| L           | Left Trigger                         |
| R           | Right Trigger                        |
| Start       | Start                                |
| Select      | Select                               |
| Left D-Pad  | Left D-Pad (Up/Down/Left/Right)      |
| Right D-Pad | Right D-Pad (mapped to right analog) |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind L = L1
bind R = R1
bind Start = START
bind Select = SELECT
bind L. Up = UP
bind L. Down = DOWN
bind L. Left = LEFT
bind L. Right = RIGHT
```

**Note:** Right D-Pad requires "Right Analog to Digital" core option enabled.

---

## Neo Geo Pocket / Color (race)

**Core:** `race_libretro.so`
**Systems:** NGP, NGPC

### Extensions

| Extension | Description              |
| --------- | ------------------------ |
| `.ngp`    | Neo Geo Pocket ROM       |
| `.ngc`    | Neo Geo Pocket Color ROM |

### BIOS Files

None required.

### Base Resolution

**160×152** (20:19 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| Option      | Option                |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = B
bind B = A
bind Option = START
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Pokemon mini (pokemini)

**Core:** `pokemini_libretro.so`
**Systems:** PKM

### Extensions

| Extension | Description      |
| --------- | ---------------- |
| `.min`    | Pokemon mini ROM |

### BIOS Files

| Filename   | Description       | MD5                                | Required |
| ---------- | ----------------- | ---------------------------------- | -------- |
| `bios.min` | Pokemon mini BIOS | `1e4fb124a3a886865acb574f388c803d` | Optional |

### Base Resolution

**96×64** (3:2 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| C           | C Button              |
| Power       | Power                 |
| Shock       | Shake/Shock sensor    |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = A
bind B = B
bind C = R1
bind Power = SELECT
bind Shock = L1
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Pico-8 (fake08)

**Core:** `fake08_libretro.so`
**Systems:** P8

### Extensions

| Extension | Description       |
| --------- | ----------------- |
| `.p8`     | Pico-8 text cart  |
| `.p8.png` | Pico-8 image cart |
| `.png`    | Pico-8 image cart |

### BIOS Files

None required.

### Base Resolution

**128×128** (1:1 aspect ratio)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| O           | O Button (circle)     |
| X           | X Button (cross)      |
| Pause       | Pause/Menu            |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind O = A
bind X = B
bind Pause = START
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## TIC-80 (tic80)

**Core:** `tic80_libretro.so`
**Systems:** TIC

### Extensions

| Extension | Description |
| --------- | ----------- |
| `.tic`    | TIC-80 cart |

### BIOS Files

None required.

### Base Resolution

**240×136** (fixed)

### Controller Mapping

| Core Button | Description           |
| ----------- | --------------------- |
| A           | A Button              |
| B           | B Button              |
| X           | X Button              |
| Y           | Y Button              |
| D-Pad       | Up, Down, Left, Right |

**Suggested LessUI Config:**

```
bind A = B
bind B = A
bind X = Y
bind Y = X
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

**Note:** Button mappings appear reversed in the core (A→B, B→A).

---

## MAME Arcade (mame2003_plus)

**Core:** `mame2003_plus_libretro.so`
**Systems:** MAME

### Extensions

| Extension | Description           |
| --------- | --------------------- |
| `.zip`    | ROM archive (primary) |
| `.chd`    | Compressed disc/HDD   |

### BIOS Files

- **ROM Set Version:** MAME 0.78 (with enhancements)
- **Recommended Format:** Full Non-Merged romsets (no separate BIOS needed)
- For Neo Geo games: `neogeo.zip` required
- **Uni-BIOS 3.3** recommended for Neo Geo (http://unibios.free.fr/)

### Base Resolution

Varies by game (typical: 320×240, 224×288 for vertical games)

### Controller Mapping

| Core Button | Arcade Function |
| ----------- | --------------- |
| B           | Button 1        |
| A           | Button 2        |
| Y           | Button 3        |
| X           | Button 4        |
| L1          | Button 5        |
| R1          | Button 6        |
| R2          | Insert Coin     |
| Start       | Start           |
| Select      | Select/Service  |
| D-Pad       | Joystick        |

**Suggested LessUI Config:**

```
bind Button 1 = B
bind Button 2 = A
bind Button 3 = Y
bind Button 4 = X
bind Button 5 = L1
bind Button 6 = R1
bind Coin = R2
bind Start = START
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## FBNeo Arcade / Neo Geo / CPS (fbneo)

**Core:** `fbneo_libretro.so`
**Systems:** FBN, CPS1, CPS2, CPS3, NEOGEO

### Extensions

| Extension | Description           |
| --------- | --------------------- |
| `.zip`    | ROM archive (primary) |
| `.7z`     | 7-Zip archive         |

### BIOS Files

**Neo Geo (NEOGEO):**
| Filename | Description | Required |
|----------|-------------|----------|
| `neogeo.zip` | Neo Geo BIOS collection | **Required** |

Contents of neogeo.zip should include MVS BIOS (Asia/Europe ver. 6 minimum).
Uni-BIOS supported via `fbneo/patched/` directory.

**Neo Geo CD:**
| Filename | Description | Required |
|----------|-------------|----------|
| `neocdz.zip` | Neo Geo CD BIOS | For CD games |

**CPS1/CPS2/CPS3:** No separate BIOS required (ROM-based).

### Base Resolution

- **Neo Geo:** 320×224
- **CPS1:** 384×224 (typical)
- **CPS2:** 384×224 (typical)
- **CPS3:** 384×224 (typical)
- Varies by individual game

### Controller Mapping

**Neo Geo (4-button):**
| Core Button | Description |
|-------------|-------------|
| A | A Button |
| B | B Button |
| C | C Button |
| D | D Button |
| Start | Start |
| Select | Select/Coin |
| D-Pad | Joystick |

**Suggested LessUI Config (NEOGEO):**

```
bind A = B
bind B = A
bind C = Y
bind D = X
bind Start = START
bind Coin = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

**CPS Fighting Games (6-button):**
| Core Button | Description |
|-------------|-------------|
| Light Punch | LP |
| Medium Punch | MP |
| Heavy Punch | HP |
| Light Kick | LK |
| Medium Kick | MK |
| Heavy Kick | HK |
| Start | Start |
| Coin | Coin |
| D-Pad | Joystick |

**Suggested LessUI Config (CPS1/CPS2/CPS3):**

```
bind Light Punch = Y
bind Medium Punch = X
bind Heavy Punch = R1
bind Light Kick = B
bind Medium Kick = A
bind Heavy Kick = R2
bind Start = START
bind Coin = SELECT
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

**Suggested LessUI Config (FBN general):**

```
bind Button 1 = B
bind Button 2 = A
bind Button 3 = Y
bind Button 4 = X
bind Button 5 = L1
bind Button 6 = R1
bind Coin = R2
bind Start = START
bind Up = UP
bind Down = DOWN
bind Left = LEFT
bind Right = RIGHT
```

---

## Quick Reference Summary

| System | Core               | Resolution | BIOS Required |
| ------ | ------------------ | ---------- | ------------- |
| GB     | gambatte           | 160×144    | No            |
| GBC    | gambatte           | 160×144    | No            |
| GBA    | gpsp               | 240×160    | Recommended   |
| MGBA   | mgba               | 240×160    | No            |
| SGB    | mgba               | 256×224    | Optional      |
| FC     | fceumm             | 256×240    | FDS only      |
| SFC    | snes9x2005_plus    | 256×224    | No            |
| SUPA   | mednafen_supafaust | 256×224    | No            |
| MD     | picodrive          | 320×224    | Sega CD only  |
| GG     | picodrive          | 320×224    | No            |
| SMS    | picodrive          | 320×224    | No            |
| PS     | pcsx_rearmed       | 320×240    | **Yes**       |
| PCE    | mednafen_pce_fast  | 512×243    | No            |
| PCECD  | mednafen_pce_fast  | 512×243    | **Yes**       |
| VB     | mednafen_vb        | 384×224    | No            |
| NGP    | race               | 160×152    | No            |
| NGPC   | race               | 160×152    | No            |
| PKM    | pokemini           | 96×64      | No            |
| P8     | fake08             | 128×128    | No            |
| TIC    | tic80              | 240×136    | No            |
| MAME   | mame2003_plus      | Varies     | Neo Geo only  |
| FBN    | fbneo              | Varies     | No            |
| CPS1   | fbneo              | ~384×224   | No            |
| CPS2   | fbneo              | ~384×224   | No            |
| CPS3   | fbneo              | ~384×224   | No            |
| NEOGEO | fbneo              | 320×224    | **Yes**       |

---

## Sources

- [Libretro Documentation](https://docs.libretro.com/)
- Individual core GitHub repositories
- LessUI codebase analysis
