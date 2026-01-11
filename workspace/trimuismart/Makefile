###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER = other/DinguxCommander

# keymon.elf is now built in workspace/all/utils
all: readmes show

.PHONY: all early clean show
show:
	@$(MAKE) -C show

early: $(REQUIRES_COMMANDER)
	@mkdir -p other
	@cd other/DinguxCommander && $(MAKE) -s

clean:
	@cd other/DinguxCommander && $(MAKE) -s clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch trimui-smart https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/Makefile