###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

# LessOS provides SDL2 natively - no sdl12-compat needed

# keymon.elf is now built in workspace/all/utils
# LessOS doesn't need DinguxCommander (file manager for stock firmware)
all: readmes show

.PHONY: all early clean show
show:
	@$(MAKE) -C show

# No early dependencies for rgb30
early:

clean:

include ../all/readmes/Makefile