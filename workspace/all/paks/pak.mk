# workspace/all/paks/pak.mk
# Shared build configuration for native paks
#
# Usage in pak's src/Makefile:
#   TARGET = clock
#   include ../../../pak.mk

# Paks are at paks/Tools/Name/src/, need to go up to workspace/
PLATFORM_DEPTH = ../../../../../

# Output to pak's build directory (../build relative to src/)
BUILD_DIR = ../build

# Pak binaries use .elf extension
BINARY_EXT = .elf

# Include the main build system
include ../../../../common/build.mk
