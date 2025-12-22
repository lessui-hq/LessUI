#!/usr/bin/env python3
"""
Calculate BUFFER_SCALE_FACTOR for platforms using CPU/hardware integer scaling.

The buffer must hold the largest integer-scaled core output. This script
calculates the worst case for each platform based on all supported cores.

Usage:
    python3 scripts/calculate-buffer-scale.py
"""

import math

# All supported cores with their base video dimensions
# Sources: libretro core info files, system specs
CORES = {
    # Nintendo
    "FC/FDS (fceumm)": (256, 224),
    "GB/GBC/SGB (gambatte)": (160, 144),
    "GBA (gpsp/mgba)": (240, 160),
    "SFC (snes9x2005)": (256, 224),
    "SFC hi-res": (512, 448),
    "SUPA (supafaust)": (256, 224),
    "VB (mednafen_vb)": (384, 224),
    "N64 (mupen64plus)": (320, 240),
    "NDS (melonds)": (256, 384),
    # Sega
    "MD/32X/SEGACD (picodrive)": (320, 224),
    "SMS/SG1000 (picodrive)": (256, 192),
    "GG (picodrive)": (160, 144),
    "DC (flycast)": (640, 480),
    # Sony
    "PS (pcsx_rearmed)": (320, 240),
    "PS hi-res": (640, 480),
    "PSP (ppsspp)": (480, 272),
    # NEC
    "PCE/PCECD (mednafen_pce)": (256, 224),
    "SGX (supergrafx)": (256, 224),
    # SNK
    "NGP/NGPC (race)": (160, 152),
    # Atari
    "A2600 (stella)": (160, 192),
    "A5200 (a5200)": (336, 240),
    "A7800 (prosystem)": (320, 225),
    "LYNX (mednafen_lynx)": (160, 102),
    # Arcade
    "FBN/CPS/MAME": (384, 224),
    "CPS3": (384, 224),
    # Other
    "PKM (pokemini)": (96, 64),
    "P8 (fake08)": (128, 128),
    "VEC (vecx)": (330, 410),
    "MSX/COLECO (bluemsx)": (256, 192),
    "C64/C128/VIC/PLUS4/PET (vice)": (320, 200),
    "ZXS (fuse)": (256, 192),
    "SCUMM (scummvm)": (320, 200),
}

# Platforms that need CPU/hardware integer scaling (not SDL2 GPU)
PLATFORMS = {
    "trimuismart": (320, 240),
    "miyoomini": (640, 480),
    "rg35xx": (640, 480),
}


def calculate_buffer_needs(screen_w, screen_h, cores):
    """Calculate buffer requirements for all cores on a given screen."""
    results = []
    for core, (src_w, src_h) in cores.items():
        # Integer scale needed to cover screen in both dimensions
        scale_x = math.ceil(screen_w / src_w)
        scale_y = math.ceil(screen_h / src_h)
        scale = max(scale_x, scale_y)

        # Resulting buffer size
        buf_w = src_w * scale
        buf_h = src_h * scale

        # Factor needed: buffer must fit in both dimensions
        # VIDEO_BUFFER_WIDTH = FIXED_WIDTH * BUFFER_SCALE_FACTOR
        # VIDEO_BUFFER_HEIGHT = FIXED_HEIGHT * BUFFER_SCALE_FACTOR
        # So factor = max(buf_w/screen_w, buf_h/screen_h)
        factor = max(buf_w / screen_w, buf_h / screen_h)

        results.append({
            "core": core,
            "src": (src_w, src_h),
            "scale": scale,
            "buffer": (buf_w, buf_h),
            "factor": factor,
        })

    # Sort by factor needed (largest first)
    results.sort(key=lambda x: x["factor"], reverse=True)
    return results


def main():
    print(f"BUFFER_SCALE_FACTOR Calculator")
    print(f"Testing {len(CORES)} core configurations")
    print("=" * 70)

    for platform, (screen_w, screen_h) in PLATFORMS.items():
        print(f"\n{platform} (screen {screen_w}x{screen_h}):")
        print("-" * 60)

        results = calculate_buffer_needs(screen_w, screen_h, CORES)

        print("  Top 10 worst cases (sorted by factor needed):")
        for i, r in enumerate(results[:10]):
            src_w, src_h = r["src"]
            buf_w, buf_h = r["buffer"]
            print(f"  {i+1:2}. {r['core']:35} {src_w:3}x{src_h:3} "
                  f"@ {r['scale']}x = {buf_w:4}x{buf_h:4} (factor {r['factor']:.2f})")

        worst = results[0]
        buf_w, buf_h = worst["buffer"]

        print()
        print(f"  Worst case: {worst['core']}")
        print(f"  Buffer needed: {buf_w}x{buf_h}")
        print(f"  Factor needed: {worst['factor']:.2f}")
        print(f"  Recommended BUFFER_SCALE_FACTOR: {math.ceil(worst['factor'] * 10) / 10}")

    print()
    print("=" * 70)
    print("SDL2 GPU platforms use BUFFER_SCALE_FACTOR 1.0 (GPU handles scaling)")


if __name__ == "__main__":
    main()
