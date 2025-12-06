# workspace/all/paks/makefile
# Orchestrates all pak builds using a standard hook interface
#
# Directory structure:
#   Tools/     - Tool paks (Clock, Input, Wifi, etc.)
#   Emus/      - Emulator paks (template-based, handled by generate-paks.sh)
#   MinUI/     - System pak (template-based, handled by generate-minui-pak.sh)
#
# Each tool pak can implement these hooks (all optional):
#   setup   - Runs once before platform builds (download shared binaries)
#   build   - Runs per-platform in Docker (compile platform-specific code)
#   install - Runs per-platform outside Docker (construct pak in build dir)
#   clean   - Clean build artifacts
#
# Paks implement hooks either:
#   - In pak root makefile (e.g., HTTP File Server/makefile)
#   - In src/makefile (e.g., Clock/src/makefile)

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

.PHONY: setup build install clean all

# Default target when called from workspace/makefile (per-platform build)
all: build

# Setup hook - runs once before any platform builds
setup:
	@# Run Emus setup (downloads libretro cores)
	@$(MAKE) -C Emus setup "DESTDIR=$(DESTDIR)"
	@# Run tool pak setup hooks
	@for pak_dir in Tools/*/; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" setup "DESTDIR=$(DESTDIR)"; \
		elif [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" setup "DESTDIR=$(DESTDIR)"; \
		fi; \
	done

# Build hook - runs per-platform in Docker
# Uses shell loop to handle pak names with spaces (Make treats spaces as delimiters)
build:
	@for pak_dir in Tools/*/; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" build "PLATFORM=$(PLATFORM)"; \
		fi; \
	done

# Install hook - runs per-platform outside Docker (construct paks)
install:
	@for pak_dir in Tools/*/; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" install "PLATFORM=$(PLATFORM)" "DESTDIR=$(DESTDIR)"; \
		elif [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" install "PLATFORM=$(PLATFORM)" "DESTDIR=$(DESTDIR)"; \
		fi; \
	done

# Clean hook
clean:
	@for pak_dir in Tools/*/; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" clean; \
		fi; \
		if [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" clean; \
		fi; \
	done
