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
REQUIRES_UNZIP60 = other/unzip60

all: readmes
	cd overclock && $(MAKE)
	cd other/squashfs && $(MAKE)

early: $(REQUIRES_COMMANDER) $(REQUIRES_UNZIP60)
	mkdir -p other
	cd $(REQUIRES_COMMANDER) && $(MAKE) -j
	cd $(REQUIRES_UNZIP60) && $(MAKE) -f unix/Makefile.trimuismart unzip
	cd libmstick && $(MAKE)

clean:
	cd overclock && $(MAKE) clean
	cd other/squashfs && $(MAKE) clean
	cd $(REQUIRES_COMMANDER) && $(MAKE) clean
	cd $(REQUIRES_UNZIP60) && $(MAKE) -f unix/Makefile.trimuismart clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

include ../all/readmes/makefile