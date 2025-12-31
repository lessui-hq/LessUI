###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rk3566 make)
endif

###########################################################

# LessOS provides SDL2 natively - no sdl12-compat needed

# keymon.elf is now built in workspace/all/utils
# LessOS doesn't need DinguxCommander (file manager for stock firmware)
all: readmes show

.PHONY: show
show:
	@$(MAKE) -C show

# No early dependencies for rk3566
early:

clean:

include ../all/readmes/Makefile