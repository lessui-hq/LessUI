# workspace/all/utils/Makefile
# Orchestrates all utility builds using a standard hook interface
#
# Each util can implement these hooks (all optional):
#   setup   - Runs once before platform builds (download shared binaries)
#   build   - Runs per-platform in Docker (compile platform-specific code)
#   install - Runs per-platform outside Docker (copy to build directory)
#   clean   - Clean build artifacts
#
# Utils that don't implement a hook are skipped automatically.

# Suppress "Entering/Leaving directory" messages
MAKEFLAGS += --no-print-directory

###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

# Discover all utils with Makefiles (exclude third-party libraries)
UTIL_DIRS := $(dir $(wildcard */Makefile))
UTIL_NAMES := $(filter-out parson,$(patsubst %/,%,$(UTIL_DIRS)))

.PHONY: setup build install clean all

# Default target when called from workspace/Makefile (per-platform build)
all: build

# Setup hook - runs once before any platform builds
setup:
	@for util in $(UTIL_NAMES); do \
		$(MAKE) -C "$$util" setup DESTDIR=$(DESTDIR); \
	done

# Build hook - runs per-platform in Docker (parallel via make -j)
# Each util is an independent target, so make -j will parallelize them
build: $(UTIL_NAMES)

$(UTIL_NAMES): FORCE
	@$(MAKE) -C "$@" build PLATFORM=$(PLATFORM)

FORCE:

# Install hook - runs per-platform outside Docker
install:
	@for util in $(UTIL_NAMES); do \
		$(MAKE) -C "$$util" install PLATFORM=$(PLATFORM) DESTDIR=$(DESTDIR); \
	done

# Clean hook
clean:
	@for util in $(UTIL_NAMES); do \
		$(MAKE) -C "$$util" clean; \
	done