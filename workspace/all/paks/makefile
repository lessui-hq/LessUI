# workspace/all/paks/makefile
# Orchestrates all pak builds using a standard hook interface
#
# Each pak can implement these hooks (all optional):
#   setup   - Runs once before platform builds (download shared binaries)
#   build   - Runs per-platform in Docker (compile platform-specific code)
#   install - Runs per-platform outside Docker (construct pak in build dir)
#   clean   - Clean build artifacts
#
# Paks implement hooks either:
#   - In pak root makefile (e.g., File Transfer/makefile)
#   - In src/makefile (e.g., Clock/src/makefile)

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

# Discover all paks with src/ makefiles (these need compilation)
PAK_SRC_MAKEFILES := $(wildcard */src/makefile)
PAK_SRC_NAMES := $(patsubst %/src/makefile,%,$(PAK_SRC_MAKEFILES))

.PHONY: setup build install clean all

# Default target when called from workspace/makefile (per-platform build)
all: build

# Setup hook - runs once before any platform builds
setup:
	@for pak_dir in */; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" setup DESTDIR=$(DESTDIR); \
		elif [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" setup DESTDIR=$(DESTDIR); \
		fi; \
	done

# Build hook - runs per-platform in Docker (parallel via make -j)
# Each pak is an independent target, so make -j will parallelize them
build: $(PAK_SRC_NAMES)

$(PAK_SRC_NAMES): FORCE
	@$(MAKE) -C "$@/src" build PLATFORM=$(PLATFORM)

FORCE:

# Install hook - runs per-platform outside Docker (construct paks)
install:
	@for pak_dir in */; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" install PLATFORM=$(PLATFORM) DESTDIR=$(DESTDIR); \
		elif [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" install PLATFORM=$(PLATFORM) DESTDIR=$(DESTDIR); \
		fi; \
	done

# Clean hook
clean:
	@for pak_dir in */; do \
		pak=$${pak_dir%/}; \
		if [ -f "$$pak/makefile" ]; then \
			$(MAKE) -C "$$pak" clean; \
		fi; \
		if [ -f "$$pak/src/makefile" ]; then \
			$(MAKE) -C "$$pak/src" clean; \
		fi; \
	done
