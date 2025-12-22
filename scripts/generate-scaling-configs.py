#!/usr/bin/env python3
"""
Generate platform-specific scaling configs based on integer scaling analysis.

This script is the SOURCE OF TRUTH for platform scaling overrides.
Change the rules here, regenerate, done.

Usage:
  ./scripts/generate-scaling-configs.py          # Analyze only (dry run)
  ./scripts/generate-scaling-configs.py --apply  # Generate configs
  ./scripts/generate-scaling-configs.py --clean  # Remove all platform configs
"""

import os
import sys
import shutil
from pathlib import Path

# =============================================================================
# CONFIGURATION - Edit these to change the rules
# =============================================================================

# Minimum fill percentage to recommend Native scaling
FILL_THRESHOLD_LARGE_SCREEN = 94  # Screens >= 3"
FILL_THRESHOLD_SMALL_SCREEN = 100  # Screens < 3" (must be perfect fit)

# Screen size threshold (inches)
MIN_SCREEN_INCHES = 3.0

# =============================================================================
# DEVICE DATA
# =============================================================================

# Format: (width, height, diagonal_inches)
PLATFORMS = {
    # miyoomini excluded: has 480p/560p and 2.8"/3.5" variants we can't detect yet
    "rg35xxplus":   (640, 480, 3.5),
    "rgb30":        (720, 720, 4.0),    # Square
    "m17":          (480, 272, 4.3),    # Miyoo A30 / similar
    "trimuismart":  (640, 480, 4.95),
    "tg5040":       (1280, 720, 4.96),  # Trimui Smart Pro
}

# Device variants (platform -> variant -> screen info)
VARIANTS = {
    "rg35xxplus": {
        "cube": (720, 720, 3.95),   # Square
        "wide": (720, 480, 3.4),
    },
    "tg5040": {
        "brick": (1024, 768, 3.2),
    },
}

# Square screens get Sharp sharpness, others get Sharp too when Native
SQUARE_SCREENS = {"cube", "rgb30"}

# =============================================================================
# CORE DATA
# =============================================================================

# From workspace/all/paks/Emus/cores.json
# Skip MAME/FBN/GW which have "varies" resolution
# Skip PS - always uses aspect scaling (many games use non-native resolutions)
CORES = {
    # Nintendo
    "GB":    (160, 144),
    "GBC":   (160, 144),
    "GBA":   (240, 160),
    "MGBA":  (240, 160),
    "SGB":   (256, 224),
    "FC":    (256, 240),
    "SFC":   (256, 224),
    "SUPA":  (256, 224),
    "VB":    (384, 224),
    # Sega
    "MD":    (320, 224),
    "GG":    (160, 144),
    "SMS":   (256, 192),
    # NEC
    "PCE":   (512, 243),
    "PCECD": (512, 243),
    # SNK
    "NGP":   (160, 152),
    "NGPC":  (160, 152),
    "NG":    (320, 224),
    "NEOCD": (320, 224),
    "CPS1":  (384, 224),
    "CPS2":  (384, 224),
    "CPS3":  (384, 224),
    # Atari
    "A2600": (160, 192),
    "A5200": (336, 240),
    "A7800": (320, 240),
    "LYNX":  (160, 102),
    # Bandai
    "WS":    (224, 144),
    "WSC":   (224, 144),
    # Other
    "PKM":   (96, 64),
    "PICO":  (128, 128),
}

# =============================================================================
# SPECIAL OVERRIDES - configs that need extra settings beyond scaling
# =============================================================================

# Extra lines to add to specific platform/core configs
EXTRA_CONFIG_LINES = {
    ("m17", "PS", None): ["bind L2 Button = NONE:L2", "bind R2 Button = NONE:R2"],
    ("trimuismart", "PS", None): ["bind L2 Button = NONE:L2", "bind R2 Button = NONE:R2"],
}

# Scaling overrides (platform, core, variant) -> scaling value
# Use this for cases where math says one thing but experience says another
SCALING_OVERRIDES = {
    # Square screens: FC/SFC only get 71% fill with Native, use Cropped instead
    ("rg35xxplus", "FC", "cube"): "Cropped",
    ("rg35xxplus", "SFC", "cube"): "Cropped",
    ("rgb30", "FC", None): "Cropped",
    ("rgb30", "SFC", None): "Cropped",
}

# =============================================================================
# CALCULATION LOGIC
# =============================================================================

def calc_fill(screen_w, screen_h, core_w, core_h):
    """Calculate integer scaling fill percentage in limiting dimension."""
    scale_w = screen_w // core_w
    scale_h = screen_h // core_h
    scale = min(scale_w, scale_h)

    if scale == 0:
        return 0, 0

    if scale_w <= scale_h:
        fill = (core_w * scale) / screen_w * 100
    else:
        fill = (core_h * scale) / screen_h * 100

    return fill, scale


def should_use_native(fill, screen_inches):
    """Determine if Native scaling should be used based on fill % and screen size."""
    if screen_inches < MIN_SCREEN_INCHES:
        return fill >= FILL_THRESHOLD_SMALL_SCREEN
    return fill >= FILL_THRESHOLD_LARGE_SCREEN


def is_square_screen(platform, variant):
    """Check if this is a square screen."""
    if variant in SQUARE_SCREENS:
        return True
    if platform in SQUARE_SCREENS:
        return True
    return False


# =============================================================================
# CONFIG GENERATION
# =============================================================================

def generate_config_content(platform, core, variant, scaling, fill):
    """Generate the content for a config file."""
    if scaling == "Cropped":
        lines = ["# Auto-generated: Cropped fills screen by scaling up and trimming edges"]
    else:
        lines = [f"# Auto-generated: {fill:.0f}% fill with integer scaling"]
    lines.append(f"player_screen_scaling = {scaling}")
    # Note: Sharpness is automatic - player forces Sharp for Native/Cropped modes

    # Add any extra lines for this combo
    key = (platform, core, variant)
    if key in EXTRA_CONFIG_LINES:
        lines.append("")
        lines.extend(EXTRA_CONFIG_LINES[key])

    return "\n".join(lines) + "\n"


def get_recommendations():
    """Calculate all recommendations based on current rules."""
    recommendations = []

    # Base platforms
    for platform, (screen_w, screen_h, inches) in PLATFORMS.items():
        for core, (core_w, core_h) in CORES.items():
            fill, scale = calc_fill(screen_w, screen_h, core_w, core_h)

            # Check for scaling override
            override_key = (platform, core, None)
            if override_key in SCALING_OVERRIDES:
                scaling = SCALING_OVERRIDES[override_key]
                recommendations.append({
                    "platform": platform,
                    "core": core,
                    "variant": None,
                    "scaling": scaling,
                    "fill": fill,
                    "scale": scale,
                    "screen": f"{screen_w}x{screen_h}",
                    "inches": inches,
                    "reason": "override",
                })
            elif should_use_native(fill, inches):
                recommendations.append({
                    "platform": platform,
                    "core": core,
                    "variant": None,
                    "scaling": "Native",
                    "fill": fill,
                    "scale": scale,
                    "screen": f"{screen_w}x{screen_h}",
                    "inches": inches,
                    "reason": "calculated",
                })

    # Variants
    for platform, variants in VARIANTS.items():
        for variant, (screen_w, screen_h, inches) in variants.items():
            for core, (core_w, core_h) in CORES.items():
                fill, scale = calc_fill(screen_w, screen_h, core_w, core_h)

                # Check for scaling override
                override_key = (platform, core, variant)
                if override_key in SCALING_OVERRIDES:
                    scaling = SCALING_OVERRIDES[override_key]
                    recommendations.append({
                        "platform": platform,
                        "core": core,
                        "variant": variant,
                        "scaling": scaling,
                        "fill": fill,
                        "scale": scale,
                        "screen": f"{screen_w}x{screen_h}",
                        "inches": inches,
                        "reason": "override",
                    })
                elif should_use_native(fill, inches):
                    recommendations.append({
                        "platform": platform,
                        "core": core,
                        "variant": variant,
                        "scaling": "Native",
                        "fill": fill,
                        "scale": scale,
                        "screen": f"{screen_w}x{screen_h}",
                        "inches": inches,
                        "reason": "calculated",
                    })

    # Also add configs that only have extra lines (no scaling change needed)
    for (platform, core, variant), extra_lines in EXTRA_CONFIG_LINES.items():
        # Check if we already have a recommendation for this combo
        existing = any(
            r["platform"] == platform and r["core"] == core and r["variant"] == variant
            for r in recommendations
        )
        if not existing:
            # Get screen info
            if variant and platform in VARIANTS and variant in VARIANTS[platform]:
                screen_w, screen_h, inches = VARIANTS[platform][variant]
            else:
                screen_w, screen_h, inches = PLATFORMS.get(platform, (0, 0, 0))

            core_w, core_h = CORES.get(core, (0, 0))
            fill, scale = calc_fill(screen_w, screen_h, core_w, core_h) if core_w else (0, 0)

            recommendations.append({
                "platform": platform,
                "core": core,
                "variant": variant,
                "scaling": None,  # No scaling override, just extra lines
                "fill": fill,
                "scale": scale,
                "screen": f"{screen_w}x{screen_h}",
                "inches": inches,
                "reason": "extra_lines_only",
            })

    return recommendations


# =============================================================================
# FILE OPERATIONS
# =============================================================================

def get_config_path(base_dir, platform, core, variant):
    """Get the path for a config file."""
    filename = f"default-{variant}.cfg" if variant else "default.cfg"
    return base_dir / platform / core / filename


def clean_platform_configs(base_dir):
    """Remove all platform config directories (not base/)."""
    removed = []
    for item in base_dir.iterdir():
        if item.is_dir() and item.name != "base":
            shutil.rmtree(item)
            removed.append(item.name)
    return removed


def write_configs(base_dir, recommendations):
    """Write config files for all recommendations."""
    written = []
    for rec in recommendations:
        path = get_config_path(base_dir, rec["platform"], rec["core"], rec["variant"])
        path.parent.mkdir(parents=True, exist_ok=True)

        # Generate content
        if rec["scaling"]:
            content = generate_config_content(
                rec["platform"], rec["core"], rec["variant"],
                rec["scaling"], rec["fill"]
            )
        else:
            # Extra lines only, no scaling
            lines = EXTRA_CONFIG_LINES.get(
                (rec["platform"], rec["core"], rec["variant"]), []
            )
            content = "\n".join(lines) + "\n"

        path.write_text(content)
        written.append(path)

    return written


# =============================================================================
# MAIN
# =============================================================================

def print_analysis(recommendations):
    """Print analysis of recommendations."""
    print(f"\n{'=' * 70}")
    print(f"INTEGER SCALING ANALYSIS")
    print(f"  Large screens (>= {MIN_SCREEN_INCHES}\"): {FILL_THRESHOLD_LARGE_SCREEN}%+ fill -> Native")
    print(f"  Small screens (<  {MIN_SCREEN_INCHES}\"): {FILL_THRESHOLD_SMALL_SCREEN}%+ fill -> Native")
    print(f"{'=' * 70}\n")

    # Group by platform
    by_platform = {}
    for rec in recommendations:
        key = rec["platform"] + (f"-{rec['variant']}" if rec["variant"] else "")
        if key not in by_platform:
            by_platform[key] = []
        by_platform[key].append(rec)

    for platform in sorted(by_platform.keys()):
        recs = by_platform[platform]
        print(f"{platform} ({recs[0]['screen']}, {recs[0]['inches']}\"):")
        for rec in sorted(recs, key=lambda r: -r["fill"]):
            scaling = rec["scaling"] or "(extra lines only)"
            reason = f" [{rec['reason']}]" if rec["reason"] != "calculated" else ""
            print(f"  {rec['core']:<5} {rec['fill']:>5.0f}% -> {scaling}{reason}")
        print()

    print(f"Total configs to generate: {len(recommendations)}")


def main():
    # Find config directory
    script_dir = Path(__file__).parent
    config_dir = script_dir.parent / "workspace" / "all" / "paks" / "Emus" / "configs"

    if not config_dir.exists():
        print(f"Error: Config directory not found: {config_dir}")
        sys.exit(1)

    # Parse arguments
    apply_changes = "--apply" in sys.argv
    clean_only = "--clean" in sys.argv

    if clean_only:
        print(f"Cleaning platform configs in {config_dir}...")
        removed = clean_platform_configs(config_dir)
        print(f"Removed: {', '.join(removed) if removed else '(none)'}")
        return

    # Calculate recommendations
    recommendations = get_recommendations()

    # Print analysis
    print_analysis(recommendations)

    if apply_changes:
        print(f"\nApplying changes to {config_dir}...")

        # Clean old configs
        removed = clean_platform_configs(config_dir)
        if removed:
            print(f"Removed old platform dirs: {', '.join(removed)}")

        # Write new configs
        written = write_configs(config_dir, recommendations)
        print(f"Created {len(written)} config files")
    else:
        print("\nDry run - use --apply to generate configs")


if __name__ == "__main__":
    main()
