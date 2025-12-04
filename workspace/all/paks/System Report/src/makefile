# System Report pak - CPU benchmark binary
#
# Minimal standalone build - no SDL or common infrastructure needed

TARGET = benchmark
BUILD_DIR = ../build
BINARY_EXT = .elf

###########################################################
# Platform check and toolchain setup

ifeq (,$(PLATFORM))
PLATFORM := $(UNION_PLATFORM)
endif

ifeq ($(filter setup clean,$(MAKECMDGOALS)),)
ifeq ($(filter install,$(MAKECMDGOALS)),)

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=miyoomini make)
endif

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

# Include platform environment for ARCH flags
PLATFORM_DEPTH = ../../../../
include $(PLATFORM_DEPTH)$(PLATFORM)/platform/makefile.env

endif  # not install
endif  # not setup/clean

###########################################################
# Compiler configuration

CC = $(CROSS_COMPILE)gcc
SOURCE = benchmark.c
CFLAGS = $(ARCH) -O2 -Wall
LDFLAGS = -lm

PRODUCT = $(BUILD_DIR)/$(PLATFORM)/$(TARGET)$(BINARY_EXT)

###########################################################
# Targets

$(PRODUCT): $(SOURCE)
	mkdir -p $(BUILD_DIR)/$(PLATFORM)
	$(CC) $(SOURCE) -o $@ $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

setup:
	@true

build: $(PRODUCT)

install:
	@if [ -n "$(DESTDIR)" ]; then \
		mkdir -p $(DESTDIR); \
		cp $(PRODUCT) $(DESTDIR)/; \
	fi

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all setup build install clean
