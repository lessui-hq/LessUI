# show - Image display utility
#
# Displays PNG images on screen for boot logos and update screens.
# Uses SDL_image library for PNG loading.
#
# Supports three rendering backends:
# - SDL2 (most platforms): Hardware-accelerated rendering
# - SDL1 (legacy platforms): Software rendering
# - Direct framebuffer (miyoomini): Zero-copy mmap

TARGET = show

# Utils are at workspace/all/utils/show/ - 3 levels to workspace/
PLATFORM_DEPTH = ../../../

# Need SDL_image for PNG loading
EXTRA_LDFLAGS = -l$(SDL)_image

# Use .elf extension for compatibility with boot scripts
BINARY_EXT = .elf

# This is a minimal utility - we don't need the full common source set
# Override SOURCE to only include what we need
SOURCE = $(TARGET).c

# Don't include common source files (api.c, utils.c, etc.)
# show.c is standalone and only needs platform.h for defines
COMMON_SOURCE =

include ../../common/build.mk
