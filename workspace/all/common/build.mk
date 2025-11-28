# workspace/all/common/build.mk
# Shared makefile patterns for paks and utils that compile native code
#
# Usage in pak/util makefile:
#   TARGET = clock
#   include path/to/common/build.mk
#   # Optionally override SOURCE, add extra CFLAGS, etc.
#
# Provides:
#   - Hook interface with setup gate (skips PLATFORM check for setup target)
#   - Standard compiler flags and paths
#   - Common source files
#   - msettings.h dependency
#   - Standard hook targets (setup, build, install, clean)
#
# Required variables (set before include):
#   TARGET      - Binary name (e.g., "clock", "minui-keyboard")
#
# Optional variables (set before include):
#   EXTRA_SOURCE  - Additional source files beyond common set
#   EXTRA_INCDIR  - Additional include paths (e.g., -I../parson)
#   EXTRA_CFLAGS  - Additional compiler flags
#   EXTRA_LDFLAGS - Additional linker flags
#   BUILD_DIR     - Override build output directory (default: build)
#   INSTALL_DEST  - Override install destination

###########################################################
# PLATFORM is needed for install (to find binaries) but not for setup/clean
ifeq (,$(PLATFORM))
PLATFORM := $(UNION_PLATFORM)
endif

# Hook interface gate - setup and clean don't need PLATFORM/CROSS_COMPILE
# install needs PLATFORM but not CROSS_COMPILE (runs on host after Docker builds)
ifeq ($(filter setup clean,$(MAKECMDGOALS)),)

# install needs PLATFORM to find the built binaries
ifeq ($(filter install,$(MAKECMDGOALS)),)
# build target - needs everything
ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=miyoomini make)
endif

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

# Include platform-specific environment
# Caller must set PLATFORM_DEPTH to number of ../ needed to reach workspace/
PLATFORM_DEPTH ?= ../../../
include $(PLATFORM_DEPTH)$(PLATFORM)/platform/makefile.env

endif  # not install
endif  # not setup/clean

###########################################################
# Defaults

SDL ?= SDL
BUILD_DIR ?= build
COMMON_DIR ?= $(PLATFORM_DEPTH)all/common
PLATFORM_DIR ?= $(PLATFORM_DEPTH)$(PLATFORM)/platform

###########################################################
# Paths and sources

INCDIR = -I. -I$(COMMON_DIR)/ -I$(PLATFORM_DIR)/ $(EXTRA_INCDIR)

COMMON_SOURCE = \
	$(COMMON_DIR)/utils.c \
	$(COMMON_DIR)/api.c \
	$(COMMON_DIR)/log.c \
	$(COMMON_DIR)/collections.c \
	$(COMMON_DIR)/pad.c \
	$(COMMON_DIR)/gfx_text.c \
	$(COMMON_DIR)/scaler.c \
	$(PLATFORM_DIR)/platform.c

SOURCE ?= $(TARGET).c $(COMMON_SOURCE) $(EXTRA_SOURCE)
HEADERS = $(wildcard $(COMMON_DIR)/*.h) $(wildcard $(PLATFORM_DIR)/*.h)

###########################################################
# Compiler configuration

CC = $(CROSS_COMPILE)gcc
CFLAGS  = $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) $(LOG_FLAGS) -Ofast
CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS  = -ldl $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz
LDFLAGS += -lmsettings
LDFLAGS += $(EXTRA_LDFLAGS)

###########################################################
# Build output

# BINARY_EXT: Extension for output binary (.elf for paks, empty for utils)
BINARY_EXT ?=
PRODUCT = $(BUILD_DIR)/$(PLATFORM)/$(TARGET)$(BINARY_EXT)

###########################################################
# Targets

$(PRODUCT): $(SOURCE) $(HEADERS) | $(PREFIX)/include/msettings.h
	mkdir -p $(BUILD_DIR)/$(PLATFORM)
	$(CC) $(SOURCE) -o $@ $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

# Hook interface targets
setup:
	@true  # No setup needed - compiled per-platform

build: $(PRODUCT)

install:
	@if [ -n "$(DESTDIR)" ]; then \
		mkdir -p $(DESTDIR); \
		cp $(PRODUCT) $(DESTDIR)/; \
	fi

clean:
	rm -rf $(BUILD_DIR)

# Dependency: ensure libmsettings is built
$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make

.PHONY: all setup build install clean
