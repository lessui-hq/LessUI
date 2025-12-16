# Third-Party Vendor Code

This directory contains third-party libraries used by LessUI. These files are not modified and are excluded from linting and formatting.

## Libraries

### stb_ds (stb/stb_ds.h)

- **Source**: https://github.com/nothings/stb
- **License**: MIT / Public Domain (Unlicense)
- **Version**: 0.67
- **Used by**: launcher_map.c, launcher_emu_cache.c for dynamic arrays and O(1) hash maps

A single-header library providing type-safe dynamic arrays and hash tables. Used for ROM alias maps where linear search would be too slow for large arcade game databases (50k+ entries).

## Adding New Libraries

1. Create a subdirectory: `vendor/<library-name>/`
2. Add the library files
3. Update this README with source, license, and version info
4. The `vendor/` directory is automatically excluded from linting and formatting
