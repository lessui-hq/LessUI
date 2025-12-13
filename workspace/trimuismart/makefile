###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER = other/DinguxCommander

# show.elf and keymon.elf are now built in workspace/all/utils
all: readmes

early: $(REQUIRES_COMMANDER)
	mkdir -p other
	cd other/DinguxCommander && $(MAKE) -j

clean:
	cd other/DinguxCommander && $(MAKE) clean

$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch trimui-smart https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile