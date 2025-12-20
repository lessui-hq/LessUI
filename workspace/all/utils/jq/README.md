# jq

Lightweight and flexible command-line JSON processor.

## LessUI Integration

This utility is provided as a prebuilt binary available to all paks.

**Building:**
```bash
make build PLATFORM=miyoomini  # Downloads binary as part of workspace/all/utils/
```

The makefile downloads the appropriate binary for each platform's architecture:
- **arm64** (tg5040, rg35xxplus): `jq-linux-arm64`
- **arm** (all others): `jq-linux-armhf`

**Deployed to:** `build/SYSTEM/<platform>/bin/jq`

## Source

**Project:** https://github.com/jqlang/jq

**Version:** 1.7.1

**Binaries from:** https://github.com/jqlang/jq/releases/tag/jq-1.7.1

## Usage

```bash
# Parse JSON from file
jq '.name' config.json

# Parse JSON from stdin
echo '{"name": "test"}' | jq '.name'

# Extract nested values
jq '.settings.wifi' system.json

# Modify values
jq '.wifi = 1' system.json > system.json.tmp
```

See https://jqlang.github.io/jq/manual/ for full documentation.
