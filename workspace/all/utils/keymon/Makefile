# keymon - Hardware button monitoring daemon
#
# Monitors physical button presses and handles system shortcuts.
# Features vary by platform (configured via platform.h):
# - Volume/brightness control
# - HDMI detection
# - Headphone jack detection
# - Multi-input device support
#
# NOTE: miyoomini has its own platform-specific keymon implementation
# due to unique AXP223 PMIC I2C hardware requirements.

TARGET = keymon

# Utils are at workspace/all/utils/keymon/ - 3 levels to workspace/
PLATFORM_DEPTH = ../../../

# Use .elf extension for compatibility
BINARY_EXT = .elf

# We need msettings and log.c, but not the full common source set
# keymon.c is relatively standalone
SOURCE = $(TARGET).c ../../../all/common/log.c

# Don't include the default common source files
COMMON_SOURCE =

# Need pthread for hardware monitoring threads (on platforms that use them)
EXTRA_LDFLAGS = -lpthread

# Skip build for miyoomini (uses platform-specific implementation in workspace/miyoomini/keymon)
ifneq ($(PLATFORM),miyoomini)
include ../../common/build.mk

# Add libudev support for dynamic input device discovery (retroid, rgb30)
# NOTE: This must be after include to access HAS_LIBUDEV from Makefile.env
ifeq ($(HAS_LIBUDEV),1)
SOURCE += ../../../all/common/udev_input.c
EXTRA_LDFLAGS += -ludev
endif

else
# Dummy targets for miyoomini
.PHONY: setup build install clean
setup:
	@true
build:
	@true
install:
	@true
clean:
	@true
endif