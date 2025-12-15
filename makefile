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

BUILD_HASH := $(shell git rev-parse --short HEAD)
RELEASE_TIME := $(shell TZ=GMT date +%Y%m%d)
RELEASE_BETA =
RELEASE_BASE = LessUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT := $(shell find ./releases/. -name "${RELEASE_BASE}-*.zip" 2>/dev/null | wc -l | sed 's/ //g')
# First build has no suffix, subsequent builds use -1, -2, etc.
# Check if unnumbered release exists, if so start numbering from RELEASE_DOT+1
RELEASE_SUFFIX := $(shell \
	if [ "$(RELEASE_DOT)" = "0" ] && [ ! -f "./releases/${RELEASE_BASE}.zip" ]; then \
		echo ""; \
	elif [ "$(RELEASE_DOT)" = "0" ]; then \
		echo "-1"; \
	else \
		echo "-$$(($(RELEASE_DOT) + 1))"; \
	fi)
RELEASE_NAME = $(RELEASE_BASE)$(RELEASE_SUFFIX)

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

.PHONY: build test coverage lint format dev dev-run dev-run-4x3 dev-run-16x9 dev-clean all shell name clean setup dev-deploy dev-build-deploy

export MAKEFLAGS=--no-print-directory

# Build everything: all platforms, create release ZIPs
all: setup $(PLATFORMS) special package

# Enter Docker build environment for a specific platform
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

# Print release name (useful for CI/scripts)
name:
	@echo $(RELEASE_NAME)

# QA convenience targets (forward to makefile.qa)
test:
	@make -f makefile.qa test

coverage:
	@make -f makefile.qa coverage

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

# Build and deploy in one shot for dev iteration (always debug build)
# Usage: make dev-build-deploy                    - Build all platforms and deploy
#        make dev-build-deploy PLATFORM=miyoomini - Build and deploy single platform
dev-build-deploy:
	@if [ -n "$(PLATFORM)" ]; then \
		$(MAKE) common PLATFORM=$(PLATFORM) DEBUG=1 && ./scripts/dev-deploy.sh --platform $(PLATFORM); \
	else \
		$(MAKE) all DEBUG=1 && ./scripts/dev-deploy.sh; \
	fi

# Build all components for a specific platform (in Docker)
build:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM)
	# ----------------------------------------------------

# Copy platform binaries to build directory
system:
	# populate system (binaries that makefile.copy may reference)
	# keymon.elf is installed by utils install hook, show.elf is platform-specific
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/syncsettings/build/$(PLATFORM)/syncsettings.elf ./build/SYSTEM/$(PLATFORM)/bin/
	# Install utils (calls install hook for each util - includes keymon.elf)
	@$(MAKE) -C ./workspace/all/utils install PLATFORM=$(PLATFORM) DESTDIR=$(CURDIR)/build/SYSTEM/$(PLATFORM)/bin
	# Now run platform-specific copy (includes show.elf and BOOT assets)
	$(MAKE) -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)
	# Construct tool paks from workspace/all/paks/Tools/
	# DEBUG=1 includes debug-only paks (those with "debug": true in pak.json)
	@for pak_dir in ./workspace/all/paks/Tools/*/; do \
		[ -d "$$pak_dir" ] || continue; \
		pak_name=$$(basename "$$pak_dir"); \
		[ -f "$$pak_dir/pak.json" ] || continue; \
		is_debug_pak=$$(jq -r '.debug // false' "$$pak_dir/pak.json"); \
		if [ "$$is_debug_pak" = "true" ] && [ -z "$(DEBUG)" ]; then \
			continue; \
		fi; \
		if jq -e '.platforms | index("$(PLATFORM)") or index("all")' "$$pak_dir/pak.json" > /dev/null 2>&1; then \
			echo "  Constructing $${pak_name}.pak for $(PLATFORM)"; \
			output_dir="./build/Tools/$(PLATFORM)/$${pak_name}.pak"; \
			mkdir -p "$$output_dir"; \
			[ -f "$$pak_dir/launch.sh" ] && rsync -a "$$pak_dir/launch.sh" "$$output_dir/" && chmod +x "$$output_dir/launch.sh"; \
			[ -f "$$pak_dir/pak.json" ] && rsync -a "$$pak_dir/pak.json" "$$output_dir/"; \
			[ -f "$$pak_dir/settings.json" ] && rsync -a "$$pak_dir/settings.json" "$$output_dir/"; \
			if [ -d "$$pak_dir/res" ]; then \
				mkdir -p "$$output_dir/res"; \
				for res_file in "$$pak_dir/res"/*; do \
					[ -f "$$res_file" ] && rsync -a "$$res_file" "$$output_dir/res/"; \
				done; \
				if [ -d "$$pak_dir/res/$(PLATFORM)" ]; then \
					rsync -a "$$pak_dir/res/$(PLATFORM)/" "$$output_dir/res/$(PLATFORM)/"; \
				fi; \
			fi; \
			if [ -d "$$pak_dir/bin/$(PLATFORM)" ]; then \
				mkdir -p "$$output_dir/bin"; \
				rsync -a "$$pak_dir/bin/$(PLATFORM)/" "$$output_dir/bin/$(PLATFORM)/"; \
			fi; \
			for script in "$$pak_dir/bin"/*; do \
				if [ -f "$$script" ] && [ -x "$$script" ]; then \
					mkdir -p "$$output_dir/bin"; \
					rsync -a "$$script" "$$output_dir/bin/"; \
				fi; \
			done; \
			if [ -d "$$pak_dir/lib/$(PLATFORM)" ]; then \
				mkdir -p "$$output_dir/lib"; \
				rsync -a "$$pak_dir/lib/$(PLATFORM)/" "$$output_dir/lib/$(PLATFORM)/"; \
			fi; \
			if [ -d "$$pak_dir/$(PLATFORM)" ]; then \
				set +e; \
				rsync -a "$$pak_dir/$(PLATFORM)/" "$$output_dir/" 2>/dev/null; \
				set -e; \
			fi; \
			for elf in "$$pak_dir/build/$(PLATFORM)/"*.elf; do \
				[ -f "$$elf" ] && mkdir -p "$$output_dir/bin" && rsync -a "$$elf" "$$output_dir/bin/" || true; \
			done; \
		fi; \
	done; true
	# Copy platform-specific binaries to paks (after pak construction)
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

# Remove build artifacts
clean:
	rm -rf ./build
	rm -rf ./workspace/readmes
	# Clean workspace/all component build directories
	rm -rf workspace/all/minui/build
	rm -rf workspace/all/minarch/build
	rm -rf workspace/all/paks/Tools/*/build
	rm -rf workspace/all/utils/*/build
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
	rsync -a ./skeleton/ ./build/

	# remove authoring detritus
	cd ./build && find . -type f -name '.keep' -delete
	cd ./build && find . -type f -name '*.meta' -delete
	echo $(BUILD_HASH) > ./workspace/hash.txt

	# Copy README to workspace for formatting (uses Linux fmt in Docker)
	mkdir -p ./workspace/readmes
	rsync -a ./skeleton/BASE/README.md ./workspace/readmes/BASE-in.txt

	# Copy boot assets to workspace for platforms that build them in Docker
	mkdir -p ./workspace/rg35xx/boot
	rsync -a ./skeleton/SYSTEM/res/installing@2x-16bit.bmp ./workspace/rg35xx/boot/installing@2x.bmp
	rsync -a ./skeleton/SYSTEM/res/updating@2x-16bit.bmp ./workspace/rg35xx/boot/updating@2x.bmp
	rsync -a ./skeleton/SYSTEM/res/bootlogo@2x.png ./workspace/rg35xx/boot/boot_logo.png
	mkdir -p ./workspace/rg35xxplus/boot
	rsync -a ./skeleton/SYSTEM/res/installing@2x.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/updating@2x.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/bootlogo@2x.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/installing@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/updating@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-rotated.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/installing@2x-square.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/updating@2x-square.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-square.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/installing@2x-wide.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/updating@2x-wide.bmp ./workspace/rg35xxplus/boot/
	rsync -a ./skeleton/SYSTEM/res/bootlogo@2x-wide.bmp ./workspace/rg35xxplus/boot/
	mkdir -p ./workspace/m17/boot
	rsync -a ./skeleton/SYSTEM/res/installing@1x-wide.bmp ./workspace/m17/boot/
	rsync -a ./skeleton/SYSTEM/res/updating@1x-wide.bmp ./workspace/m17/boot/

	# Setup hooks - download shared binaries (runs once for all components)
	@echo "Running setup hooks..."
	@$(MAKE) -C ./workspace/all/utils setup DESTDIR=$(CURDIR)/build/SYSTEM/common/bin
	@$(MAKE) -C ./workspace/all/paks setup DESTDIR=$(CURDIR)/build/SYSTEM/common/bin
	# Copy 7z binaries to BOOT (will be moved to BASE during special target)
	rsync -a ./build/SYSTEM/common/bin/arm/7z ./build/BOOT/bin/arm/
	rsync -a ./build/SYSTEM/common/bin/arm64/7z ./build/BOOT/bin/arm64/

	# Generate emulator paks from templates
	@echo "Generating emulator paks..."
	@./scripts/generate-paks.sh all

# Platform-specific packaging for Miyoo/Trimui family
special:
	# Copy shared install/update functions to BOOT/common
	mkdir -p ./build/BOOT/common/install
	rsync -a ./skeleton/SYSTEM/common/log.sh ./build/BOOT/common/install/
	rsync -a ./skeleton/SYSTEM/common/update-functions.sh ./build/BOOT/common/install/
	# setup miyoomini/trimui/magicx family .tmp_update in BOOT
	mv ./build/BOOT/common ./build/BOOT/.tmp_update
	mv ./build/BOOT/bin ./build/BASE/
	mv ./build/BOOT/miyoo ./build/BASE/
	mv ./build/BOOT/trimui ./build/BASE/
	mv ./build/BOOT/magicx ./build/BASE/
	rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/miyoo/app/.tmp_update/
	rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/trimui/app/.tmp_update/
	rsync -a ./build/BOOT/.tmp_update/ ./build/BASE/magicx/.tmp_update/
	rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo354/
	rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo355/
	rsync -a ./build/BASE/miyoo/ ./build/BASE/miyoo285/
ifneq (,$(findstring my355, $(PLATFORMS)))
	rsync -a ./workspace/my355/init/ ./build/BASE/miyoo355/app/my355/
	rsync -a ./workspace/my355/other/squashfs/output/ ./build/BASE/miyoo355/app/my355/payload/
endif

# Backward compatibility for platforms that were merged
tidy:
	# ----------------------------------------------------
	# Copy update scripts to old platform directories for smooth upgrades
ifneq (,$(findstring rg35xxplus, $(PLATFORMS)))
	mkdir -p ./build/SYSTEM/rg40xxcube/bin/
	rsync -a ./build/SYSTEM/rg35xxplus/bin/install.sh ./build/SYSTEM/rg40xxcube/bin/
endif
ifneq (,$(findstring tg5040, $(PLATFORMS)))
	mkdir -p ./build/SYSTEM/tg3040/paks/MinUI.pak/
	rsync -a ./build/SYSTEM/tg5040/bin/install.sh ./build/SYSTEM/tg3040/paks/MinUI.pak/launch.sh
endif

# Create final release ZIP files
package: tidy
	# ----------------------------------------------------
	# Package everything into distributable ZIPs

	# Move formatted README from workspace to build
	rsync -a ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	rm -rf ./workspace/readmes

	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	mkdir -p ./build/PAYLOAD
	mv ./build/SYSTEM ./build/PAYLOAD/.system

	# Copy only the cores referenced in cores.json (not all downloaded cores)
	# cores.json stores core names without .so extension, actual files have .so
	mkdir -p ./build/PAYLOAD/.system/cores/arm32 ./build/PAYLOAD/.system/cores/arm64
	jq -r '.cores[].core' ./workspace/all/paks/Emus/cores.json | sort -u | while read core; do \
		if [ -f "./build/.system/cores/arm32/$${core}.so" ]; then cp "./build/.system/cores/arm32/$${core}.so" "./build/PAYLOAD/.system/cores/arm32/"; fi; \
			if [ -f "./build/.system/cores/arm64/$${core}.so" ]; then cp "./build/.system/cores/arm64/$${core}.so" "./build/PAYLOAD/.system/cores/arm64/"; fi; \
			done
	@echo "Copied $$(ls ./build/PAYLOAD/.system/cores/arm32/*.so 2>/dev/null | wc -l | tr -d ' ') arm32 cores, $$(ls ./build/PAYLOAD/.system/cores/arm64/*.so 2>/dev/null | wc -l | tr -d ' ') arm64 cores"
	rsync -a ./build/BOOT/.tmp_update/ ./build/PAYLOAD/.tmp_update/

	# Create LessUI.7z (-md=16m limits dictionary so 128MB RAM devices can decompress)
	cd ./build/PAYLOAD && 7zz a -t7z -mx=9 -md=16m -mmt=on LessUI.7z .system .tmp_update
	mv ./build/PAYLOAD/LessUI.7z ./build/BASE

	# Move Tools to BASE so everything is at the same level
	mv ./build/Tools ./build/BASE/

	# Package final release
	cd ./build/BASE && 7zz a -tzip -mmt=on -mx=5 ../../releases/$(RELEASE_NAME).zip Tools Bios Roms Saves bin miyoo miyoo354 trimui rg35xx rg35xxplus miyoo355 magicx miyoo285 em_ui.sh LessUI.7z README.txt
	echo "$(RELEASE_NAME)" > ./build/latest.txt

###########################################################
# Dynamic platform targets

# Match any platform name and build it
.DEFAULT:
	# ----------------------------------------------------
	# $@
	@echo "$(PLATFORMS)" | grep -q "\b$@\b" && ($(MAKE) common PLATFORM=$@) || (exit 1)