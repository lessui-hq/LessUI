# Toolchain Platform Configuration

## Overview

LessUI uses Docker containers with platform-specific cross-compilation toolchains. Some toolchains require specific CPU architectures (amd64 vs arm64) for proper emulation.

## Configuration File

Platform architectures are defined in `toolchains.json`. Only platforms that require non-default architectures need to be listed:

```json
{
  "toolchains": {
    "trimuismart": {
      "platform": "linux/amd64"
    },
    "tg5040": {
      "platform": "linux/amd64"
    }
  }
}
```

Platforms not listed default to `linux/arm64`.

## How It Works

When building a platform, `Makefile.toolchain` reads the platform architecture from `toolchains.json` and passes it to Docker via the `--platform` flag:

```bash
make PLATFORM=trimuismart build  # Runs container with --platform=linux/amd64
make PLATFORM=miyoomini build    # Runs container with --platform=linux/arm64
```

## Platform Requirements

### ARM64 Platforms (linux/arm64) - Default

Most platforms use ARM64 toolchains and run natively on ARM64 hosts or via emulation on amd64 hosts. These platforms do not need entries in `toolchains.json`:

- miyoomini
- rg35xx
- rg35xxplus
- my355
- zero28
- rgb30
- m17
- my282
- magicmini

### AMD64 Platforms (linux/amd64)

Some platforms require AMD64 for proper emulation and **must** be listed in `toolchains.json`:

- **trimuismart** - Requires linux/amd64
- **tg5040** - Requires linux/amd64

## Adding a New Platform

1. Create toolchain repository in `lessui-hq` organization
2. Add build workflow to generate container
3. **Only if the platform requires amd64**, add entry to `toolchains.json`:

   ```json
   {
     "toolchains": {
       "newplatform": {
         "platform": "linux/amd64"
       }
     }
   }
   ```

   ARM64 platforms (the default) do not need to be added to `toolchains.json`.

## Defaults

If a platform is not defined in `toolchains.json`, it defaults to `linux/arm64`.

## Requirements

- **jq** must be installed on the build system
- GitHub Actions workflows already install jq in the dependency step
