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

MINARCH_CORES_VERSION ?= 20251130-2
CORES_BASE = https://github.com/lessui-hq/LessUI-Cores/releases/download/$(MINARCH_CORES_VERSION)

# All core management happens in cores/ directory
CORES_DIR = $(CURDIR)/cores

# Setup hook - download cores (runs once)
# DESTDIR is absolute path to build/SYSTEM/common/bin
# Cores go in build/.system/cores (3 levels up from SYSTEM, then down to .system)
setup:
	@BUILD_CORES_DIR="$(DESTDIR)/../../../.system/cores"; \
	mkdir -p "$$BUILD_CORES_DIR/a7" "$$BUILD_CORES_DIR/a53" "$(CORES_DIR)"; \
 \
	if [ -f "$(CORES_DIR)/override/linux-arm32.zip" ]; then \
		OVERRIDE_VERSION=$$(stat -f "%Sm" -t "%Y%m%d%H%M%S" "$(CORES_DIR)/override/linux-arm32.zip" 2>/dev/null || echo "unknown"); \
		if [ -f "$(CORES_DIR)/extracted/override/.version-a7" ] && [ "$$(cat $(CORES_DIR)/extracted/override/.version-a7)" = "$$OVERRIDE_VERSION" ]; then \
			echo "Using cached override a7 cores ($$OVERRIDE_VERSION)..."; \
			rsync -a "$(CORES_DIR)/extracted/override/a7/" "$$BUILD_CORES_DIR/a7/"; \
		else \
			echo "Extracting override a7 cores ($$OVERRIDE_VERSION)..."; \
			rm -rf "$(CORES_DIR)/extracted/override/a7"; \
			mkdir -p "$(CORES_DIR)/extracted/override/a7"; \
			unzip -j -q "$(CORES_DIR)/override/linux-arm32.zip" -d "$(CORES_DIR)/extracted/override/a7"; \
			echo "$$OVERRIDE_VERSION" > "$(CORES_DIR)/extracted/override/.version-a7"; \
			rsync -a "$(CORES_DIR)/extracted/override/a7/" "$$BUILD_CORES_DIR/a7/"; \
		fi; \
	else \
		if [ -f "$(CORES_DIR)/extracted/remote/.version-a7" ] && [ "$$(cat $(CORES_DIR)/extracted/remote/.version-a7)" = "$(MINARCH_CORES_VERSION)" ]; then \
			echo "Using cached remote a7 cores ($(MINARCH_CORES_VERSION))..."; \
			rsync -a "$(CORES_DIR)/extracted/remote/a7/" "$$BUILD_CORES_DIR/a7/"; \
		else \
			echo "Downloading remote a7 cores (ARM32) from LessUI-Cores $(MINARCH_CORES_VERSION)..."; \
			mkdir -p "$(CORES_DIR)/extracted/remote"; \
			curl -sL $(CORES_BASE)/linux-arm32.zip -o /tmp/lessui-cores-a7.zip; \
			rm -rf "$(CORES_DIR)/extracted/remote/a7"; \
			mkdir -p "$(CORES_DIR)/extracted/remote/a7"; \
			unzip -j -q /tmp/lessui-cores-a7.zip -d "$(CORES_DIR)/extracted/remote/a7"; \
			rm /tmp/lessui-cores-a7.zip; \
			echo "$(MINARCH_CORES_VERSION)" > "$(CORES_DIR)/extracted/remote/.version-a7"; \
			rsync -a "$(CORES_DIR)/extracted/remote/a7/" "$$BUILD_CORES_DIR/a7/"; \
		fi; \
	fi; \
 \
	if [ -f "$(CORES_DIR)/override/linux-arm64.zip" ]; then \
		OVERRIDE_VERSION=$$(stat -f "%Sm" -t "%Y%m%d%H%M%S" "$(CORES_DIR)/override/linux-arm64.zip" 2>/dev/null || echo "unknown"); \
		if [ -f "$(CORES_DIR)/extracted/override/.version-a53" ] && [ "$$(cat $(CORES_DIR)/extracted/override/.version-a53)" = "$$OVERRIDE_VERSION" ]; then \
			echo "Using cached override a53 cores ($$OVERRIDE_VERSION)..."; \
			rsync -a "$(CORES_DIR)/extracted/override/a53/" "$$BUILD_CORES_DIR/a53/"; \
		else \
			echo "Extracting override a53 cores ($$OVERRIDE_VERSION)..."; \
			rm -rf "$(CORES_DIR)/extracted/override/a53"; \
			mkdir -p "$(CORES_DIR)/extracted/override/a53"; \
			unzip -j -q "$(CORES_DIR)/override/linux-arm64.zip" -d "$(CORES_DIR)/extracted/override/a53"; \
			echo "$$OVERRIDE_VERSION" > "$(CORES_DIR)/extracted/override/.version-a53"; \
			rsync -a "$(CORES_DIR)/extracted/override/a53/" "$$BUILD_CORES_DIR/a53/"; \
		fi; \
	else \
		if [ -f "$(CORES_DIR)/extracted/remote/.version-a53" ] && [ "$$(cat $(CORES_DIR)/extracted/remote/.version-a53)" = "$(MINARCH_CORES_VERSION)" ]; then \
			echo "Using cached remote a53 cores ($(MINARCH_CORES_VERSION))..."; \
			rsync -a "$(CORES_DIR)/extracted/remote/a53/" "$$BUILD_CORES_DIR/a53/"; \
		else \
			echo "Downloading remote a53 cores (ARM64) from LessUI-Cores $(MINARCH_CORES_VERSION)..."; \
			mkdir -p "$(CORES_DIR)/extracted/remote"; \
			curl -sL $(CORES_BASE)/linux-arm64.zip -o /tmp/lessui-cores-a53.zip; \
			rm -rf "$(CORES_DIR)/extracted/remote/a53"; \
			mkdir -p "$(CORES_DIR)/extracted/remote/a53"; \
			unzip -j -q /tmp/lessui-cores-a53.zip -d "$(CORES_DIR)/extracted/remote/a53"; \
			rm /tmp/lessui-cores-a53.zip; \
			echo "$(MINARCH_CORES_VERSION)" > "$(CORES_DIR)/extracted/remote/.version-a53"; \
			rsync -a "$(CORES_DIR)/extracted/remote/a53/" "$$BUILD_CORES_DIR/a53/"; \
		fi; \
	fi; \
 \
	echo "Cores deployed successfully:"; \
	echo "  a7:  $$(ls "$$BUILD_CORES_DIR/a7"/*.so 2>/dev/null | wc -l | tr -d ' ') cores (ARM32)"; \
	echo "  a53: $$(ls "$$BUILD_CORES_DIR/a53"/*.so 2>/dev/null | wc -l | tr -d ' ') cores (ARM64)"

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