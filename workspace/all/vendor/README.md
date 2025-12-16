# Third-Party Vendor Code

This directory contains third-party libraries used by LessUI. These files are not modified and are excluded from linting and formatting.

## Libraries

### khash (khash.h)

- **Source**: https://github.com/attractivechaos/klib
- **License**: MIT
- **Version**: 0.2.8 (2013-05-02)
- **Used by**: stringmap.c for O(1) string-to-string hash maps

A generic hash table library implemented as C macros. Used for ROM alias maps where linear search would be too slow for large arcade game databases (50k+ entries).

## Adding New Libraries

1. Create a subdirectory: `vendor/<library-name>/`
2. Add the library files
3. Update this README with source, license, and version info
4. The `vendor/` directory is automatically excluded from linting and formatting
