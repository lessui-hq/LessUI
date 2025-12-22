###########################################################
# Emus - Emulator paks
# Downloads libretro cores from LessUI-Cores repository
#
# Hook interface:
#   setup   - Download cores for both architectures
#   build   - (not implemented - cores are pre-built)
#   install - (not implemented - cores already in place)
#   clean   - Remove downloaded cores
###########################################################

PLAYER_CORES_VERSION ?= 20251222-0
CORES_BASE = https://github.com/lessui-hq/LessUI-Cores/releases/download/$(PLAYER_CORES_VERSION)

# All core management happens in cores/ directory
CORES_DIR = $(CURDIR)/cores

# Setup hook - download cores (runs once)
# DESTDIR is absolute path to build/SYSTEM/common/bin
# Cores go in build/.system/cores (3 levels up from SYSTEM, then down to .system)
setup:
	@BUILD_CORES_DIR="$(DESTDIR)/../../../.system/cores"; \
	mkdir -p "$$BUILD_CORES_DIR/arm32" "$$BUILD_CORES_DIR/arm64" "$(CORES_DIR)"; \
 \
	if [ -f "$(CORES_DIR)/override/linux-arm32.zip" ]; then \
		OVERRIDE_VERSION=$$(date -r "$(CORES_DIR)/override/linux-arm32.zip" +"%Y%m%d%H%M%S" 2>/dev/null || echo "unknown"); \
		if [ -f "$(CORES_DIR)/extracted/override/.version-arm32" ] && [ "$$(cat $(CORES_DIR)/extracted/override/.version-arm32)" = "$$OVERRIDE_VERSION" ]; then \
			echo "Using cached override arm32 cores ($$OVERRIDE_VERSION)..."; \
			rsync -a "$(CORES_DIR)/extracted/override/arm32/" "$$BUILD_CORES_DIR/arm32/"; \
		else \
			echo "Extracting override arm32 cores ($$OVERRIDE_VERSION)..."; \
			rm -rf "$(CORES_DIR)/extracted/override/arm32"; \
			mkdir -p "$(CORES_DIR)/extracted/override/arm32"; \
			7z e -y -o"$(CORES_DIR)/extracted/override/arm32" "$(CORES_DIR)/override/linux-arm32.zip" >/dev/null; \
			echo "$$OVERRIDE_VERSION" > "$(CORES_DIR)/extracted/override/.version-arm32"; \
			rsync -a "$(CORES_DIR)/extracted/override/arm32/" "$$BUILD_CORES_DIR/arm32/"; \
		fi; \
	else \
		if [ -f "$(CORES_DIR)/extracted/remote/.version-arm32" ] && [ "$$(cat $(CORES_DIR)/extracted/remote/.version-arm32)" = "$(PLAYER_CORES_VERSION)" ]; then \
			echo "Using cached remote arm32 cores ($(PLAYER_CORES_VERSION))..."; \
			rsync -a "$(CORES_DIR)/extracted/remote/arm32/" "$$BUILD_CORES_DIR/arm32/"; \
		else \
			echo "Downloading remote arm32 cores (ARM32) from LessUI-Cores $(PLAYER_CORES_VERSION)..."; \
			mkdir -p "$(CORES_DIR)/extracted/remote"; \
			curl -sL $(CORES_BASE)/linux-arm32.zip -o /tmp/lessui-cores-arm32.zip; \
			rm -rf "$(CORES_DIR)/extracted/remote/arm32"; \
			mkdir -p "$(CORES_DIR)/extracted/remote/arm32"; \
			7z e -y -o"$(CORES_DIR)/extracted/remote/arm32" /tmp/lessui-cores-arm32.zip >/dev/null; \
			rm /tmp/lessui-cores-arm32.zip; \
			echo "$(PLAYER_CORES_VERSION)" > "$(CORES_DIR)/extracted/remote/.version-arm32"; \
			rsync -a "$(CORES_DIR)/extracted/remote/arm32/" "$$BUILD_CORES_DIR/arm32/"; \
		fi; \
	fi; \
 \
	if [ -f "$(CORES_DIR)/override/linux-arm64.zip" ]; then \
		OVERRIDE_VERSION=$$(date -r "$(CORES_DIR)/override/linux-arm64.zip" +"%Y%m%d%H%M%S" 2>/dev/null || echo "unknown"); \
		if [ -f "$(CORES_DIR)/extracted/override/.version-arm64" ] && [ "$$(cat $(CORES_DIR)/extracted/override/.version-arm64)" = "$$OVERRIDE_VERSION" ]; then \
			echo "Using cached override arm64 cores ($$OVERRIDE_VERSION)..."; \
			rsync -a "$(CORES_DIR)/extracted/override/arm64/" "$$BUILD_CORES_DIR/arm64/"; \
		else \
			echo "Extracting override arm64 cores ($$OVERRIDE_VERSION)..."; \
			rm -rf "$(CORES_DIR)/extracted/override/arm64"; \
			mkdir -p "$(CORES_DIR)/extracted/override/arm64"; \
			7z e -y -o"$(CORES_DIR)/extracted/override/arm64" "$(CORES_DIR)/override/linux-arm64.zip" >/dev/null; \
			echo "$$OVERRIDE_VERSION" > "$(CORES_DIR)/extracted/override/.version-arm64"; \
			rsync -a "$(CORES_DIR)/extracted/override/arm64/" "$$BUILD_CORES_DIR/arm64/"; \
		fi; \
	else \
		if [ -f "$(CORES_DIR)/extracted/remote/.version-arm64" ] && [ "$$(cat $(CORES_DIR)/extracted/remote/.version-arm64)" = "$(PLAYER_CORES_VERSION)" ]; then \
			echo "Using cached remote arm64 cores ($(PLAYER_CORES_VERSION))..."; \
			rsync -a "$(CORES_DIR)/extracted/remote/arm64/" "$$BUILD_CORES_DIR/arm64/"; \
		else \
			echo "Downloading remote arm64 cores (ARM64) from LessUI-Cores $(PLAYER_CORES_VERSION)..."; \
			mkdir -p "$(CORES_DIR)/extracted/remote"; \
			curl -sL $(CORES_BASE)/linux-arm64.zip -o /tmp/lessui-cores-arm64.zip; \
			rm -rf "$(CORES_DIR)/extracted/remote/arm64"; \
			mkdir -p "$(CORES_DIR)/extracted/remote/arm64"; \
			7z e -y -o"$(CORES_DIR)/extracted/remote/arm64" /tmp/lessui-cores-arm64.zip >/dev/null; \
			rm /tmp/lessui-cores-arm64.zip; \
			echo "$(PLAYER_CORES_VERSION)" > "$(CORES_DIR)/extracted/remote/.version-arm64"; \
			rsync -a "$(CORES_DIR)/extracted/remote/arm64/" "$$BUILD_CORES_DIR/arm64/"; \
		fi; \
	fi; \
 \
	echo "Cores deployed successfully:"; \
	echo "  arm32: $$(ls "$$BUILD_CORES_DIR/arm32"/*.so 2>/dev/null | wc -l | tr -d ' ') cores"; \
	echo "  arm64: $$(ls "$$BUILD_CORES_DIR/arm64"/*.so 2>/dev/null | wc -l | tr -d ' ') cores"

# Build hook - not needed (cores are pre-built)
build:
	@true

# Install hook - not needed (cores installed during setup)
install:
	@true

# Clean hook
clean:
	@# Cores are in build/.system/cores, cleaned by main makefile
	@true

.PHONY: setup build install clean