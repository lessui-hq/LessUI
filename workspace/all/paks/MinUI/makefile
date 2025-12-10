###########################################################
# MinUI - System launcher pak
# Generates launch.sh from template + platform hooks
#
# Hook interface:
#   setup   - Generate launch.sh for all platforms
#   build   - (not needed - no native code)
#   install - (not needed - generated during setup)
#   clean   - (handled by main makefile)
###########################################################

CONFIG_JSON = $(CURDIR)/../config/platforms.json
PLATFORMS := $(shell jq -r '.platforms | keys[]' "$(CONFIG_JSON)" 2>/dev/null)

# Setup hook - generate for all platforms
setup:
	@echo "Generating MinUI paks..."
	@for platform in $(PLATFORMS); do \
		$(CURDIR)/generate.sh "$$platform" "$(BUILD_DIR)"; \
	done
	@echo "Done."

build:
	@true

install:
	@true

clean:
	@true

.PHONY: setup build install clean