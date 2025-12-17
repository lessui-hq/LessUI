# workspace/all/common/cflags.mk
# Shared compiler warning flags for all LessUI components
#
# Usage in makefiles:
#   include path/to/common/cflags.mk
#   CFLAGS += $(WARN_FLAGS)

# Standard warning flags for LessUI C code
# Enables most warnings, treats them as errors, but disables some
# that trigger on legacy code patterns
WARN_FLAGS = -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
             -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
             -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
