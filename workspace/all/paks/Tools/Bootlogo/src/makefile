# Bootlogo pak build system
#
# This pak has platform-specific native code:
# - miyoomini: logoread.elf, logomake.elf, logowrite.elf
# - other platforms: shell script only
#
# Note: Does not use common/build.mk because it builds multiple
# standalone utilities without the common MinUI source files.

###########################################################
# Hook interface gate - setup, install, and clean don't need PLATFORM

ifeq ($(filter setup install clean,$(MAKECMDGOALS)),)

ifeq (,$(PLATFORM))
PLATFORM := $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=miyoomini make)
endif

endif  # ifeq (no setup/clean)

###########################################################

.PHONY: all setup build install clean

# Only build native code for miyoomini
ifeq ($(PLATFORM),miyoomini)
all:
	@echo "Building Bootlogo native utilities for miyoomini"
	@$(MAKE) -C miyoomini PLATFORM=$(PLATFORM)

clean:
	@$(MAKE) -C miyoomini clean
else
all:
	@echo "Bootlogo: No native build needed for $(PLATFORM) (shell script only)"

clean:
	@echo "Bootlogo: No build artifacts to clean for $(PLATFORM)"
endif

# Hook interface
setup:
	@true

build:
	@$(MAKE) all PLATFORM=$(PLATFORM)

install:
	@true  # Handled by main makefile pak construction loop
