###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_EVTEST = other/evtest
REQUIRES_JSTEST = other/jstest
REQUIRES_COMMANDER = other/DinguxCommander-sdl2

all: readmes show

.PHONY: show
show:
	@$(MAKE) -C show

early: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST) $(REQUIRES_COMMANDER)
	@mkdir -p other
	@cd $(REQUIRES_COMMANDER) && $(MAKE) -s
	@cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	@cd $(REQUIRES_JSTEST) && $(CROSS_COMPILE)gcc -o jstest jstest.c

clean:
	@cd $(REQUIRES_COMMANDER) && $(MAKE) -s clean

###########################################################

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)
$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

$(REQUIRES_JSTEST):
	git clone --depth 1 https://github.com/datrh/joyutils.git $(REQUIRES_JSTEST)

include ../all/readmes/makefile