# my355

###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_EVTEST = other/evtest
REQUIRES_MKBOOTIMG = other/mkbootimg
REQUIRES_RSCEGO = other/rsce-go
REQUIRES_COMMANDER = other/DinguxCommander-sdl2

# show.elf and keymon.elf are now built in workspace/all/utils
all: readmes
	cd other/squashfs && $(MAKE)

early: $(REQUIRES_EVTEST) $(REQUIRES_MKBOOTIMG) $(REQUIRES_RSCEGO) $(REQUIRES_COMMANDER)
	mkdir -p other
	cd $(REQUIRES_COMMANDER) && $(MAKE) -j
	cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	cd $(REQUIRES_MKBOOTIMG) && $(MAKE)
	cd $(REQUIRES_RSCEGO) && go build -o rsce_tool

clean:
	cd $(REQUIRES_COMMANDER) && $(MAKE) clean

###########################################################

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)
$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)
$(REQUIRES_MKBOOTIMG):
	git clone --depth 1 https://github.com/osm0sis/mkbootimg $(REQUIRES_MKBOOTIMG)
$(REQUIRES_RSCEGO):
	git clone --depth 1 https://github.com/Evsio0n/rsce-go $(REQUIRES_RSCEGO)
	cd $(REQUIRES_RSCEGO) && go mod tidy && go mod download

include ../all/readmes/makefile