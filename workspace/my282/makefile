#my282

###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER = other/DinguxCommander-sdl2

all: readmes show
	cd overclock && $(MAKE)
	cd other/squashfs && $(MAKE)

.PHONY: show
show:
	$(MAKE) -C show

early: $(REQUIRES_COMMANDER)
	mkdir -p other
	cd $(REQUIRES_COMMANDER) && $(MAKE) -j
	cd libmstick && $(MAKE)

clean:
	cd overclock && $(MAKE) clean
	cd other/squashfs && $(MAKE) clean
	cd $(REQUIRES_COMMANDER) && $(MAKE) clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile