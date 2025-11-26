# LessUI Build System
# Main makefile for orchestrating multi-platform builds
#
# This makefile runs on the HOST system (macOS/Linux), not in Docker.
# It manages Docker-based cross-compilation for multiple ARM platforms.
#
# Common targets:
#   make shell PLATFORM=<platform>  - Enter platform's build environment
#   make test                       - Run unit tests (uses Docker)
#   make lint                       - Run static analysis
#   make format                     - Format code with clang-format
#   make dev                        - Build minui for macOS (native, for development)
#   make dev-run                    - Build and run minui on macOS
#   make all                        - Build all platforms (creates release ZIPs)
#
# Platform-specific build:
#   make build PLATFORM=<platform>  - Build binaries for specific platform
#   make system PLATFORM=<platform> - Copy binaries to build directory
#
# See makefile.qa for quality assurance targets
# See makefile.toolchain for Docker/cross-compilation setup

# prevent accidentally triggering a full build with invalid calls
ifneq (,$(PLATFORM))
ifeq (,$(MAKECMDGOALS))
$(error found PLATFORM arg but no target, did you mean "make PLATFORM=$(PLATFORM) shell"?)
endif
endif

# Default platforms to build (can be overridden with PLATFORMS=...)
ifeq (,$(PLATFORMS))
PLATFORMS = miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 my282 magicmini
endif

###########################################################
# Release versioning

BUILD_HASH:=$(shell git rev-parse --short HEAD)
RELEASE_TIME:=$(shell TZ=GMT date +%Y%m%d)
RELEASE_BETA=
RELEASE_BASE=LessUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find ./releases/. -name "${RELEASE_BASE}-*-base.zip" 2>/dev/null | wc -l | sed 's/ //g')
# First build has no suffix, subsequent builds use -1, -2, etc.
# Check if unnumbered base exists, if so start numbering from RELEASE_DOT+1
RELEASE_SUFFIX:=$(shell \
	if [ "$(RELEASE_DOT)" = "0" ] && [ ! -f "./releases/${RELEASE_BASE}-base.zip" ]; then \
		echo ""; \
	elif [ "$(RELEASE_DOT)" = "0" ]; then \
		echo "-1"; \
	else \
		echo "-$$(($(RELEASE_DOT) + 1))"; \
	fi)
RELEASE_NAME=$(RELEASE_BASE)$(RELEASE_SUFFIX)

###########################################################
# Build configuration

# Logging configuration (applies to all components)
# Options:
#   -DENABLE_INFO_LOGS              INFO + WARN + ERROR (recommended for production)
#   -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS   All logs (development/troubleshooting)
#   (none)                          WARN + ERROR only (minimal logging)
LOG_FLAGS = -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS
export LOG_FLAGS

# Pre-built cores from minarch-cores repository (nightly builds)
MINARCH_CORES_VERSION ?= 20251119
CORES_BASE = https://github.com/nchapman/minarch-cores/releases/download/$(MINARCH_CORES_VERSION)

.PHONY: build test lint format dev dev-run dev-run-4x3 dev-run-16x9 dev-clean all shell name clean setup done cores-download dev-deploy dev-build-deploy

export MAKEFLAGS=--no-print-directory

# Build everything: all platforms, create release ZIPs
all: setup $(PLATFORMS) special package done

# Enter Docker build environment for a specific platform
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

# Print release name (useful for CI/scripts)
name:
	@echo $(RELEASE_NAME)

# QA convenience targets (forward to makefile.qa)
test:
	@make -f makefile.qa test

lint:
	@make -f makefile.qa lint

analyze:
	@make -f makefile.qa analyze

format:
	@make -f makefile.qa format

format-check:
	@make -f makefile.qa format-check

# macOS development targets (forward to makefile.dev)
dev:
	@make -f makefile.dev dev

dev-run:
	@make -f makefile.dev dev-run

dev-clean:
	@make -f makefile.dev dev-clean

dev-run-4x3:
	@make -f makefile.dev dev-run-4x3

dev-run-16x9:
	@make -f makefile.dev dev-run-16x9

# Deploy to SD card for rapid dev iteration (skips zip/unzip)
# Usage: make dev-deploy              - Deploy all platforms
#        make dev-deploy PLATFORM=X   - Deploy single platform
dev-deploy:
	@if [ -n "$(PLATFORM)" ]; then \
		./scripts/dev-deploy.sh --platform $(PLATFORM); \
	else \
		./scripts/dev-deploy.sh; \
	fi

# Build and deploy in one shot for dev iteration
# Usage: make dev-build-deploy                    - Build all platforms and deploy
#        make dev-build-deploy PLATFORM=miyoomini - Build and deploy single platform
dev-build-deploy:
	@if [ -n "$(PLATFORM)" ]; then \
		make common PLATFORM=$(PLATFORM) && ./scripts/dev-deploy.sh --platform $(PLATFORM); \
	else \
		make all && ./scripts/dev-deploy.sh; \
	fi

# Build all components for a specific platform (in Docker)
build:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM)
	# ----------------------------------------------------

# Copy platform binaries to build directory
system:
	make -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)
	
	# populate system
	cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/syncsettings/build/$(PLATFORM)/syncsettings.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/say/build/$(PLATFORM)/say.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/clock/build/$(PLATFORM)/clock.elf ./build/EXTRAS/Tools/$(PLATFORM)/Clock.pak/
	cp ./workspace/all/minput/build/$(PLATFORM)/minput.elf ./build/EXTRAS/Tools/$(PLATFORM)/Input.pak/

# Deploy shared libretro cores from minarch-cores GitHub releases
# Downloads and extracts cores for both ARM architectures
# Override: Place zips in workspace/cores-override/ to skip download
cores-download:
	@mkdir -p build/.system/cores/a7 build/.system/cores/a53
	@if [ -f workspace/cores-override/linux-cortex-a7.zip ]; then \
		echo "Using local a7 cores from workspace/cores-override/..."; \
		unzip -o -j -q workspace/cores-override/linux-cortex-a7.zip -d build/.system/cores/a7; \
	else \
		echo "Downloading a7 cores (ARM32 - all 32-bit platforms) from minarch-cores $(MINARCH_CORES_VERSION)..."; \
		curl -sL $(CORES_BASE)/linux-cortex-a7.zip -o /tmp/lessui-cores-a7.zip; \
		unzip -o -j -q /tmp/lessui-cores-a7.zip -d build/.system/cores/a7; \
		rm /tmp/lessui-cores-a7.zip; \
	fi
	@if [ -f workspace/cores-override/linux-cortex-a53.zip ]; then \
		echo "Using local a53 cores from workspace/cores-override/..."; \
		unzip -o -j -q workspace/cores-override/linux-cortex-a53.zip -d build/.system/cores/a53; \
	else \
		echo "Downloading a53 cores (ARM64 - all 64-bit platforms) from minarch-cores $(MINARCH_CORES_VERSION)..."; \
		curl -sL $(CORES_BASE)/linux-cortex-a53.zip -o /tmp/lessui-cores-a53.zip; \
		unzip -o -j -q /tmp/lessui-cores-a53.zip -d build/.system/cores/a53; \
		rm /tmp/lessui-cores-a53.zip; \
	fi
	@echo "Cores deployed successfully:"
	@echo "  a7:  $$(ls build/.system/cores/a7/*.so 2>/dev/null | wc -l | tr -d ' ') cores (ARM32)"
	@echo "  a53: $$(ls build/.system/cores/a53/*.so 2>/dev/null | wc -l | tr -d ' ') cores (ARM64)"

# Legacy cores target - now just points to shared cores
cores:
	@echo "Cores are now shared in build/.system/cores/{a7,a53}"
	@echo "No per-platform core deployment needed."

# Build everything for a platform: binaries, system files
common: build system

# Remove build artifacts
clean:
	rm -rf ./build
	rm -rf ./workspace/readmes
	# Clean workspace/all component build directories
	rm -rf workspace/all/minui/build
	rm -rf workspace/all/minarch/build
	rm -rf workspace/all/clock/build
	rm -rf workspace/all/minput/build
	rm -rf workspace/all/say/build
	rm -rf workspace/all/syncsettings/build
	# Clean platform-specific boot outputs
	rm -rf workspace/rg35xxplus/boot/output
	rm -rf workspace/rg35xx/boot/output
	rm -rf workspace/m17/boot/output
	# Clean copied boot assets
	rm -f workspace/rg35xxplus/boot/*.bmp
	rm -f workspace/rg35xx/boot/*.bmp workspace/rg35xx/boot/boot_logo.png
	rm -f workspace/m17/boot/*.bmp

# Prepare fresh build directory and skeleton
setup: name
	# ----------------------------------------------------
	# Make sure we're running in an interactive terminal (not piped/redirected)
	# tty -s  # Disabled: automated builds require non-interactive execution

	# Create fresh build directory
	rm -rf ./build
	mkdir -p ./releases
	cp -R ./skeleton ./build
	
	# remove authoring detritus
	cd ./build && find . -type f -name '.keep' -delete
	cd ./build && find . -type f -name '*.meta' -delete
	echo $(BUILD_HASH) > ./workspace/hash.txt
	
	# Copy READMEs to workspace for formatting (uses Linux fmt in Docker)
	mkdir -p ./workspace/readmes
	cp ./skeleton/BASE/README.md ./workspace/readmes/BASE-in.txt
	cp ./skeleton/EXTRAS/README.md ./workspace/readmes/EXTRAS-in.txt

	# Copy boot assets to workspace for platforms that build them in Docker
	mkdir -p ./workspace/rg35xx/boot
	cp ./skeleton/SYSTEM/res/installing@2x-16bit.bmp ./workspace/rg35xx/boot/installing@2x.bmp
	cp ./skeleton/SYSTEM/res/updating@2x-16bit.bmp ./workspace/rg35xx/boot/updating@2x.bmp
	cp ./skeleton/SYSTEM/res/bootlogo@2x.png ./workspace/rg35xx/boot/boot_logo.png
	mkdir -p ./workspace/rg35xxplus/boot
	cp ./skeleton/SYSTEM/res/installing@2x.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/updating@2x.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/bootlogo@2x.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/installing@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/updating@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/bootlogo@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/installing@2x-square.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/updating@2x-square.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/bootlogo@2x-square.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/installing@2x-wide.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/updating@2x-wide.bmp ./workspace/rg35xxplus/boot/
	cp ./skeleton/SYSTEM/res/bootlogo@2x-wide.bmp ./workspace/rg35xxplus/boot/
	mkdir -p ./workspace/m17/boot
	cp ./skeleton/SYSTEM/res/installing@1x-wide.bmp ./workspace/m17/boot/
	cp ./skeleton/SYSTEM/res/updating@1x-wide.bmp ./workspace/m17/boot/

	# Deploy shared cores
	@make cores-download

	# Generate platform-specific paks from templates
	@echo "Generating paks from templates..."
	@./scripts/generate-paks.sh all

# Signal build completion (macOS only - harmless on Linux)
done:
	say "done" 2>/dev/null || true

# Platform-specific packaging for Miyoo/Trimui family
special:
	# Copy shared install/update functions to BOOT/common
	mkdir -p ./build/BOOT/common/install
	cp ./skeleton/SYSTEM/common/log.sh ./build/BOOT/common/install/
	cp ./skeleton/SYSTEM/common/update-functions.sh ./build/BOOT/common/install/
	# setup miyoomini/trimui/magicx family .tmp_update in BOOT
	mv ./build/BOOT/common ./build/BOOT/.tmp_update
	mv ./build/BOOT/miyoo ./build/BASE/
	mv ./build/BOOT/trimui ./build/BASE/
	mv ./build/BOOT/magicx ./build/BASE/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/miyoo/app/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/trimui/app/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/magicx/
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo354
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo355
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo285
ifneq (,$(findstring my355, $(PLATFORMS)))
	cp -R ./workspace/my355/init ./build/BASE/miyoo355/app/my355
	cp -r ./workspace/my355/other/squashfs/output/* ./build/BASE/miyoo355/app/my355/payload/
endif

# Backward compatibility for platforms that were merged
tidy:
	# ----------------------------------------------------
	# Copy update scripts to old platform directories for smooth upgrades
ifneq (,$(findstring rg35xxplus, $(PLATFORMS)))
	mkdir -p ./build/SYSTEM/rg40xxcube/bin/
	cp ./build/SYSTEM/rg35xxplus/bin/install.sh ./build/SYSTEM/rg40xxcube/bin/
endif
ifneq (,$(findstring tg5040, $(PLATFORMS)))
	mkdir -p ./build/SYSTEM/tg3040/paks/MinUI.pak/
	cp ./build/SYSTEM/tg5040/bin/install.sh ./build/SYSTEM/tg3040/paks/MinUI.pak/launch.sh
endif

# Create final release ZIP files
package: tidy
	# ----------------------------------------------------
	# Package everything into distributable ZIPs
		
	# Move formatted READMEs from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README.txt
	rm -rf ./workspace/readmes
	
	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	mkdir -p ./build/PAYLOAD
	mv ./build/SYSTEM ./build/PAYLOAD/.system
	cp -R ./build/.system/cores ./build/PAYLOAD/.system/
	cp -R ./build/BOOT/.tmp_update ./build/PAYLOAD/

	# Create LessUI.zip for update installer
	cd ./build/PAYLOAD && zip -r LessUI.zip .system .tmp_update
	mv ./build/PAYLOAD/LessUI.zip ./build/BASE

	# TODO: can I just add everything in BASE to zip?
	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui rg35xx rg35xxplus miyoo355 magicx miyoo285 em_ui.sh LessUI.zip README.txt
	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Roms Saves Tools README.txt
	echo "$(RELEASE_NAME)" > ./build/latest.txt
	
###########################################################
# Dynamic platform targets

# Match any platform name and build it
.DEFAULT:
	# ----------------------------------------------------
	# $@
	@echo "$(PLATFORMS)" | grep -q "\b$@\b" && (make common PLATFORM=$@) || (exit 1)
	
