# MinUI Build System
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
PLATFORMS = miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 gkdpixel my282 magicmini
endif

###########################################################
# Release versioning

BUILD_HASH:=$(shell git rev-parse --short HEAD)
RELEASE_TIME:=$(shell TZ=GMT date +%Y%m%d)
RELEASE_BETA=
RELEASE_BASE=MinUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find -E ./releases/. -regex ".*/${RELEASE_BASE}-[0-9]+-base\.zip" | wc -l | sed 's/ //g')
RELEASE_NAME=$(RELEASE_BASE)-$(RELEASE_DOT)

###########################################################
# Build configuration

.PHONY: build test lint format all shell name clean setup done

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

format:
	@make -f makefile.qa format

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

# Copy libretro cores to build directory
# TODO: can't assume every platform will have the same stock cores (platform should be responsible for copy too)
cores:
	# stock cores
	cp ./workspace/$(PLATFORM)/cores/output/fceumm_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gambatte_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gpsp_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/picodrive_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/snes9x2005_plus_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/pcsx_rearmed_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	
	# extras
ifeq ($(PLATFORM), trimuismart)
	cp ./workspace/miyoomini/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
else ifeq ($(PLATFORM), m17)
	cp ./workspace/miyoomini/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
else ifneq ($(PLATFORM),gkdpixel)
	cp ./workspace/$(PLATFORM)/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
endif
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/MGBA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SGB.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_pce_fast_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PCE.pak
	cp ./workspace/$(PLATFORM)/cores/output/pokemini_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PKM.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGP.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGPC.pak
ifneq ($(PLATFORM),gkdpixel)
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_supafaust_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SUPA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_vb_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/VB.pak
endif

# Build everything for a platform: binaries, system files, cores
common: build system cores

# Remove build artifacts
clean:
	rm -rf ./build
	rm -rf ./workspace/readmes

# Prepare fresh build directory and skeleton
setup: name
	# ----------------------------------------------------
	# Make sure we're running in an interactive terminal (not piped/redirected)
	tty -s 
	
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
	cp ./skeleton/SYSTEM/res/installing@2x.bmp ./workspace/rg35xx/boot/
	cp ./skeleton/SYSTEM/res/updating@2x.bmp ./workspace/rg35xx/boot/
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

# Signal build completion (macOS only - harmless on Linux)
done:
	say "done" 2>/dev/null || true

# Platform-specific packaging for Miyoo/Trimui family
special:
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
	cp -R ./build/BOOT/.tmp_update ./build/PAYLOAD/
	
	cd ./build/PAYLOAD && zip -r MinUI.zip .system .tmp_update
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE
	
	# TODO: can I just add everything in BASE to zip?
	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui rg35xx rg35xxplus gkdpixel miyoo355 magicx miyoo285 em_ui.sh MinUI.zip README.txt
	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Tools README.txt
	echo "$(RELEASE_NAME)" > ./build/latest.txt
	
###########################################################
# Dynamic platform targets

# Match any platform name and build it
.DEFAULT:
	# ----------------------------------------------------
	# $@
	@echo "$(PLATFORMS)" | grep -q "\b$@\b" && (make common PLATFORM=$@) || (exit 1)
	
