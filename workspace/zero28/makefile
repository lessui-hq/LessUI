# zero28

###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER = other/DinguxCommander-sdl2

all: readmes
	cd bl && $(MAKE)

early: $(REQUIRES_COMMANDER)
	mkdir -p other
	cd $(REQUIRES_COMMANDER) && $(MAKE) -j

clean:
	cd bl && $(MAKE) clean
	cd $(REQUIRES_COMMANDER) && $(MAKE) clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile