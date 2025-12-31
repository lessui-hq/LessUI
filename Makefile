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
#   make dev                        - Build launcher for macOS (native, for development)
#   make dev-run                    - Build and run launcher on macOS
#   make all                        - Build all platforms (creates release ZIPs)
#
# Platform-specific build:
#   make build PLATFORM=<platform>  - Build binaries for specific platform
#   make system PLATFORM=<platform> - Copy binaries to build directory
#
# See Makefile.qa for quality assurance targets
# See Makefile.toolchain for Docker/cross-compilation setup

# prevent accidentally triggering a full build with invalid calls
ifneq (,$(PLATFORM))
ifeq (,$(MAKECMDGOALS))
$(error found PLATFORM arg but no target, did you mean "make PLATFORM=$(PLATFORM) shell"?)
endif
endif

# Default platforms to build (can be overridden with PLATFORMS=...)
ifeq (,$(PLATFORMS))
PLATFORMS = miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 my282 magicmini rk3566
endif

###########################################################
# Release versioning
#
# CI builds (GitHub Actions): Use VERSION file (e.g., v0.0.1)
# Local dev builds: Use dev-YYYYMMDD format
#
# Output:
#   - ZIP: LessUI-<version>.zip
#   - version.txt: <version>\n<git-hash>

BUILD_HASH := $(shell git rev-parse --short HEAD)
export BUILD_HASH
RELEASE_TIME := $(shell TZ=GMT date +%Y%m%d)

# Determine release name based on environment
ifeq ($(CI_RELEASE),true)
# CI release build (tag push): use VERSION file (semver)
RELEASE_VERSION := $(shell cat VERSION 2>/dev/null || echo "v0.0.0")
RELEASE_NAME = LessUI-$(RELEASE_VERSION)
else
# Local dev build: use dev-DATE format with collision detection
RELEASE_BASE = LessUI-dev-$(RELEASE_TIME)
RELEASE_DOT := $(shell find ./releases/. -name "${RELEASE_BASE}-*.zip" 2>/dev/null | wc -l | sed 's/ //g')
RELEASE_SUFFIX := $(shell \
	if [ "$(RELEASE_DOT)" = "0" ] && [ ! -f "./releases/${RELEASE_BASE}.zip" ]; then \
		echo ""; \
	elif [ "$(RELEASE_DOT)" = "0" ]; then \
		echo "-1"; \
	else \
		echo "-$$(($(RELEASE_DOT) + 1))"; \
	fi)
RELEASE_NAME = $(RELEASE_BASE)$(RELEASE_SUFFIX)
RELEASE_VERSION = dev-$(RELEASE_TIME)$(RELEASE_SUFFIX)
endif

###########################################################
# Build configuration
#
# DEBUG=1 controls both optimization and log verbosity:
#   - Release (default): -O3, INFO logs, no debug symbols
#   - Debug (DEBUG=1):   -O0 -g, INFO+DEBUG logs, debug symbols
#
# Usage:
#   make build PLATFORM=X           # Release build
#   make build PLATFORM=X DEBUG=1   # Debug build
#   make dev-build-deploy           # Always debug build
#   make dev-run                    # macOS dev (always debug)

ifdef DEBUG
# Debug: no optimization, debug symbols, all logs
OPT_FLAGS = -O0 -g
LOG_FLAGS = -DENABLE_INFO_LOGS -DENABLE_DEBUG_LOGS
else
# Release: full optimization, INFO logs (useful for user troubleshooting)
OPT_FLAGS = -O3
LOG_FLAGS = -DENABLE_INFO_LOGS
endif
export OPT_FLAGS
export LOG_FLAGS

.PHONY: help build test coverage lint format dev dev-run dev-run-4x3 dev-run-16x9 dev-clean all shell name clean setup lessos special tidy stage compress package dev-deploy dev-build-deploy release release-patch release-minor release-major

export MAKEFLAGS=--no-print-directory

# Display available make targets
help:
	@echo "LessUI Build System"
	@echo ""
	@echo "Development (macOS):"
	@echo "  make dev              Build launcher for macOS native testing"
	@echo "  make dev-run          Build and run launcher on macOS"
	@echo "  make dev-run-4x3      Run in 4:3 aspect ratio (640x480)"
	@echo "  make dev-run-16x9     Run in 16:9 aspect ratio (854x480)"
	@echo "  make dev-clean        Clean macOS build artifacts"
	@echo ""
	@echo "Quality Assurance:"
	@echo "  make test             Run unit tests (in Docker)"
	@echo "  make coverage         Run tests with coverage report"
	@echo "  make lint             Run static analysis (clang-tidy)"
	@echo "  make format           Format code with clang-format"
	@echo ""
	@echo "Cross-Platform Build (composable stages):"
	@echo "  make setup            Prepare build directory and skeleton"
	@echo "  make build PLATFORM=X Compile binaries for platform (Docker)"
	@echo "  make system PLATFORM=X Copy binaries to build/SYSTEM/"
	@echo "  make common PLATFORM=X Build + system combined"
	@echo "  make special          Platform-specific boot setup"
	@echo "  make stage            Reorganize build/SYSTEM → build/PAYLOAD"
	@echo "  make compress         Create release archives (7z, zip)"
	@echo "  make package          Stage + compress (full release)"
	@echo "  make all              Full build: setup + platforms + package"
	@echo "  make shell PLATFORM=X Enter Docker build environment"
	@echo ""
	@echo "Deployment:"
	@echo "  make dev-deploy       Deploy to SD card (requires LESSUI_DEV volume)"
	@echo "  make dev-build-deploy Build and deploy (no compression)"
	@echo ""
	@echo "Release:"
	@echo "  make release-patch    Create patch release (v1.0.0 → v1.0.1)"
	@echo "  make release-minor    Create minor release (v1.0.1 → v1.1.0)"
	@echo "  make release-major    Create major release (v1.1.0 → v2.0.0)"
	@echo ""
	@echo "Housekeeping:"
	@echo "  make clean            Remove all build artifacts"
	@echo "  make name             Print release name"
	@echo ""
	@echo "Available platforms: $(PLATFORMS)"

# Build everything: all platforms, create release ZIPs
all: setup $(PLATFORMS) special package

# Enter Docker build environment for a specific platform
shell:
	$(MAKE) -f Makefile.toolchain PLATFORM=$(PLATFORM)

# Print release name (useful for CI/scripts)
name:
	@echo $(RELEASE_NAME)

# Create a release using git-flow
# Usage: make release-patch (or release-minor/release-major)
release-patch:
	@./scripts/release.sh patch

release-minor:
	@./scripts/release.sh minor

release-major:
	@./scripts/release.sh major

# Legacy alias (deprecated)
release:
	@./scripts/release.sh $(TYPE)

# QA convenience targets (forward to Makefile.qa)
test:
	@$(MAKE) -f Makefile.qa test

test-asan:
	@$(MAKE) -f Makefile.qa test-asan

coverage:
	@$(MAKE) -f Makefile.qa coverage

lint:
	@$(MAKE) -f Makefile.qa lint

analyze:
	@$(MAKE) -f Makefile.qa analyze

format:
	@$(MAKE) -f Makefile.qa format

format-check:
	@$(MAKE) -f Makefile.qa format-check

# macOS development targets (forward to Makefile.dev)
dev:
	@$(MAKE) -f Makefile.dev dev

dev-run:
	@$(MAKE) -f Makefile.dev dev-run

dev-clean:
	@$(MAKE) -f Makefile.dev dev-clean

dev-run-4x3:
	@$(MAKE) -f Makefile.dev dev-run-4x3

dev-run-16x9:
	@$(MAKE) -f Makefile.dev dev-run-16x9

# Deploy to SD card for rapid dev iteration (skips zip/unzip)
# Usage: make dev-deploy              - Deploy all platforms
#        make dev-deploy PLATFORM=X   - Deploy single platform
dev-deploy:
	@if [ -n "$(PLATFORM)" ]; then \
		./scripts/dev-deploy.sh --platform $(PLATFORM); \
	else \
		./scripts/dev-deploy.sh; \
	fi

# Build and deploy in one shot for dev iteration (always debug build)
# Uses 'stage' to prepare files without compression (faster than full 'all')
# Usage: make dev-build-deploy                    - Build all platforms and deploy
#        make dev-build-deploy PLATFORM=miyoomini - Build and deploy single platform
# Note: Single-platform requires 'make setup' to have been run first
dev-build-deploy:
	@if [ -n "$(PLATFORM)" ]; then \
		if [ ! -d ./build/SYSTEM ]; then \
			echo "Error: build/SYSTEM not found. Run 'make setup' first."; \
			exit 1; \
		fi; \
		$(MAKE) common PLATFORM=$(PLATFORM) DEBUG=1 && $(MAKE) stage && ./scripts/dev-deploy.sh --platform $(PLATFORM); \
	else \
		$(MAKE) setup DEBUG=1 && $(MAKE) $(PLATFORMS) DEBUG=1 && $(MAKE) special && $(MAKE) stage && ./scripts/dev-deploy.sh; \
	fi

# Build all components for a specific platform (in Docker)
build:
	@echo "# ----------------------------------------------------"
	@echo "# $(PLATFORM)"
	@echo "# ----------------------------------------------------"
	@$(MAKE) build -f Makefile.toolchain PLATFORM=$(PLATFORM)

# Copy platform binaries to build directory
system:
	@cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	@cp ./workspace/all/launcher/build/$(PLATFORM)/launcher.elf ./build/SYSTEM/$(PLATFORM)/bin/
	@cp ./workspace/all/player/build/$(PLATFORM)/player.elf ./build/SYSTEM/$(PLATFORM)/bin/
	@cp ./workspace/all/syncsettings/build/$(PLATFORM)/syncsettings.elf ./build/SYSTEM/$(PLATFORM)/bin/
	@$(MAKE) -C ./workspace/all/utils install PLATFORM=$(PLATFORM) DESTDIR=$(CURDIR)/build/SYSTEM/$(PLATFORM)/bin
	@$(MAKE) -s -f ./workspace/$(PLATFORM)/platform/Makefile.copy PLATFORM=$(PLATFORM)
	@./scripts/construct-tool-paks.sh $(PLATFORM) $(DEBUG)
	@if [ "$(PLATFORM)" = "rg35xxplus" ]; then \
		mkdir -p ./build/Tools/rg35xxplus/Apply\ Panel\ Fix.pak/bin; \
		mkdir -p ./build/Tools/rg35xxplus/Swap\ Menu.pak/bin; \
		rsync -a ./workspace/rg35xxplus/other/dtc/dtc ./build/Tools/rg35xxplus/Apply\ Panel\ Fix.pak/bin/; \
		rsync -a ./workspace/rg35xxplus/other/dtc/dtc ./build/Tools/rg35xxplus/Swap\ Menu.pak/bin/; \
	fi
	@if [ "$(PLATFORM)" = "my282" ]; then \
		mkdir -p ./build/Tools/my282/Remove\ Loading.pak; \
		rsync -a ./workspace/my282/other/squashfs/output/ ./build/Tools/my282/Remove\ Loading.pak/; \
	fi

# Build everything for a platform: binaries, system files
common: build system

# Remove build artifacts (aligns with .gitignore)
clean:
	@rm -rf ./build
	@rm -rf ./workspace/readmes
	@rm -rf workspace/all/build
	@rm -rf workspace/all/launcher/build
	@rm -rf workspace/all/player/build
	@rm -rf workspace/all/paks/Tools/*/build
	@rm -rf workspace/all/utils/*/build
	@rm -rf workspace/all/syncsettings/build
	@find workspace -type d -name "output" -path "*/boot/output" -exec rm -rf {} + 2>/dev/null || true
	@find workspace -type d -name "output" -path "*/squashfs/output" -exec rm -rf {} + 2>/dev/null || true
	@find workspace -type f -name "*.bmp" -path "*/boot/*.bmp" -delete 2>/dev/null || true
	@find workspace -type f -name "boot_logo.png" -path "*/boot/boot_logo.png" -delete 2>/dev/null || true
	@rm -rf workspace/all/paks/Emus/cores/extracted/

# Prepare fresh build directory and skeleton
setup: name
	@rm -rf ./build
	@mkdir -p ./releases
	@rsync -a ./skeleton/ ./build/
	@cd ./build && find . -type f -name '.keep' -delete
	@cd ./build && find . -type f -name '*.meta' -delete
	@mkdir -p ./workspace/readmes
	@rsync -a ./skeleton/BASE/README.md ./workspace/readmes/BASE-in.txt
	@mkdir -p ./workspace/rg35xx/boot
	@rsync -a ./skeleton/SYSTEM/res/installing@2x-16bit.bmp ./workspace/rg35xx/boot/installing@2x.bmp
	@rsync -a ./skeleton/SYSTEM/res/updating@2x-16bit.bmp ./workspace/rg35xx/boot/updating@2x.bmp
	@rsync -a ./skeleton/SYSTEM/res/bootlogo@2x.png ./workspace/rg35xx/boot/boot_logo.png
	@mkdir -p ./workspace/rg35xxplus/boot
	@rsync -a ./skeleton/SYSTEM/res/installing@2x.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/updating@2x.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/bootlogo@2x.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/installing@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/updating@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/installing@2x-square.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/updating@2x-square.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-square.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/installing@2x-wide.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/updating@2x-wide.bmp ./workspace/rg35xxplus/boot/
	@rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-wide.bmp ./workspace/rg35xxplus/boot/
	@mkdir -p ./workspace/m17/boot
	@rsync -a ./skeleton/SYSTEM/res/installing@1x-wide.bmp ./workspace/m17/boot/
	@rsync -a ./skeleton/SYSTEM/res/updating@1x-wide.bmp ./workspace/m17/boot/
	@echo "Running setup hooks..."
	@$(MAKE) -s -C ./workspace/all/utils setup DESTDIR=$(CURDIR)/build/SYSTEM/common/bin
	@$(MAKE) -s -C ./workspace/all/paks setup DESTDIR=$(CURDIR)/build/SYSTEM/common/bin
	@rsync -a ./build/SYSTEM/common/bin/arm/7z ./build/BOOT/bin/arm/
	@rsync -a ./build/SYSTEM/common/bin/arm64/7z ./build/BOOT/bin/arm64/
	@echo "Generating emulator paks..."
	@./scripts/generate-paks.sh all

# Platform-specific packaging for Miyoo/Trimui/LessOS families
special:
	@mkdir -p ./build/BOOT/common/install
	@rsync -a ./skeleton/SYSTEM/common/log.sh ./build/BOOT/common/install/
	@rsync -a ./skeleton/SYSTEM/common/update-functions.sh ./build/BOOT/common/install/
	@mv ./build/BOOT/common ./build/BOOT/.tmp_update
	@mv ./build/BOOT/bin ./build/BASE/
	@mv ./build/BOOT/miyoo ./build/BASE/
	@mv ./build/BOOT/trimui ./build/BASE/
	@mv ./build/BOOT/magicx ./build/BASE/
	@if [ -d ./build/BOOT/lessos ]; then mv ./build/BOOT/lessos ./build/BASE/; fi
	@rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/miyoo/app/.tmp_update/
	@rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/trimui/app/.tmp_update/
	@rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/magicx/.tmp_update/
	@if [ -d ./build/BASE/lessos ]; then rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/lessos/.tmp_update/; fi
	@rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo354/
	@rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo355/
	@rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo285/
ifneq (,$(findstring my355, $(PLATFORMS)))
	@rsync -a ./workspace/my355/init/ ./build/BASE/miyoo355/app/my355/
	@rsync -a ./workspace/my355/other/squashfs/output/ ./build/BASE/miyoo355/app/my355/payload/
endif

# Backward compatibility for platforms that were merged
# Only copies files if the source platform was actually built
tidy:
ifneq (,$(findstring rg35xxplus, $(PLATFORMS)))
	@if [ -f ./build/SYSTEM/rg35xxplus/bin/install.sh ]; then \
		mkdir -p ./build/SYSTEM/rg40xxcube/bin/; \
		rsync -a ./build/SYSTEM/rg35xxplus/bin/install.sh ./build/SYSTEM/rg40xxcube/bin/; \
	fi
endif
ifneq (,$(findstring tg5040, $(PLATFORMS)))
	@if [ -f ./build/SYSTEM/tg5040/bin/install.sh ]; then \
		mkdir -p ./build/SYSTEM/tg3040/paks/LessUI.pak/; \
		rsync -a ./build/SYSTEM/tg5040/bin/install.sh ./build/SYSTEM/tg3040/paks/LessUI.pak/launch.sh; \
	fi
endif

# Stage: reorganize build/SYSTEM → build/PAYLOAD for deployment
# This prepares the directory structure without creating archives
# Can be run incrementally after single-platform builds
stage: tidy
	@echo "# ----------------------------------------------------"
	@echo "# Staging for deployment..."
	@echo "# ----------------------------------------------------"
	@mkdir -p ./build/PAYLOAD/.system
	@rsync -a ./build/SYSTEM/ ./build/PAYLOAD/.system/
	@cd ./build/PAYLOAD/.system && printf '%s\n%s\n' "$(RELEASE_VERSION)" "$(BUILD_HASH)" > version.txt
	@./commits.sh > ./build/PAYLOAD/.system/commits.txt
	@mkdir -p ./build/PAYLOAD/.system/common/cores/arm32 ./build/PAYLOAD/.system/common/cores/arm64
	@jq -r '.cores[].core' ./workspace/all/paks/Emus/cores.json | sort -u | while read core; do \
		if [ -f "./build/.system/cores/arm32/$${core}.so" ]; then cp "./build/.system/cores/arm32/$${core}.so" "./build/PAYLOAD/.system/common/cores/arm32/"; fi; \
			if [ -f "./build/.system/cores/arm64/$${core}.so" ]; then cp "./build/.system/cores/arm64/$${core}.so" "./build/PAYLOAD/.system/common/cores/arm64/"; fi; \
			done
	@echo "Copied $$(ls ./build/PAYLOAD/.system/common/cores/arm32/*.so 2>/dev/null | wc -l | tr -d ' ') arm32 cores, $$(ls ./build/PAYLOAD/.system/common/cores/arm64/*.so 2>/dev/null | wc -l | tr -d ' ') arm64 cores"
	@if [ -d ./build/BOOT/.tmp_update ]; then \
		rsync -a ./build/BOOT/.tmp_update/ ./build/PAYLOAD/.tmp_update/; \
	fi
	@if [ -d ./build/Tools ]; then rsync -a ./build/Tools/ ./build/BASE/Tools/; fi
	@cd ./build && find . -type f -name '.DS_Store' -delete

# Compress: create archives for release distribution
# Requires stage to have been run first (typically via 'make all')
compress:
	@echo "# ----------------------------------------------------"
	@echo "# Creating release archives..."
	@echo "# ----------------------------------------------------"
	@if [ -f ./workspace/readmes/BASE-out.txt ]; then \
		rsync -a ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt; \
		rm -rf ./workspace/readmes; \
	fi
	@if [ -d ./build/PAYLOAD/.tmp_update ]; then \
		cd ./build/PAYLOAD && 7z a -t7z -mx=9 -md=16m -mmt=on LessUI.7z .system .tmp_update; \
	else \
		cd ./build/PAYLOAD && 7z a -t7z -mx=9 -md=16m -mmt=on LessUI.7z .system; \
	fi
	@mv ./build/PAYLOAD/LessUI.7z ./build/BASE
	@cd ./build/BASE && 7z a -tzip -mmt=on -mx=5 ../../releases/$(RELEASE_NAME).zip Tools Bios Roms Saves bin miyoo miyoo354 trimui rg35xx rg35xxplus miyoo355 magicx miyoo285 lessos em_ui.sh LessUI.7z README.txt
	@echo "$(RELEASE_NAME)" > ./build/latest.txt

# Package: full release build (stage + compress)
package: stage compress

###########################################################
# Dynamic platform targets

# Match any platform name and build it
.DEFAULT:
	@if echo "$(PLATFORMS)" | grep -q "\b$@\b"; then \
		$(MAKE) common PLATFORM=$@; \
	else \
		echo "Error: Unknown target '$@'"; \
		echo ""; \
		echo "Available platforms: $(PLATFORMS)"; \
		echo ""; \
		echo "Run 'make help' for available targets."; \
		exit 1; \
	fi