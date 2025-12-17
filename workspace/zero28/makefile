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

all: readmes show
	@cd bl && $(MAKE)

.PHONY: show
show:
	@$(MAKE) -C show

early: $(REQUIRES_COMMANDER)
	@mkdir -p other
	@cd $(REQUIRES_COMMANDER) && $(MAKE) -s

clean:
	@cd bl && $(MAKE) -s clean
	@cd $(REQUIRES_COMMANDER) && $(MAKE) -s clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile