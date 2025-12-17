# workspace/makefile - builds all components for a platform

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

# Suppress "Entering/Leaving directory" messages
MAKEFLAGS += --no-print-directory

###########################################################

# Components that can run in parallel after libs
# keymon is now built in workspace/all/utils/keymon (unified implementation)
COMPONENTS = launcher player paks utils syncsettings platform-late

.PHONY: all libs $(COMPONENTS)

all: libs $(COMPONENTS)

# libs must complete first (libmsettings + early platform deps)
libs:
	@$(MAKE) -C ./$(PLATFORM)/libmsettings
	@$(MAKE) -C ./$(PLATFORM) early

$(COMPONENTS): libs

launcher:
	@$(MAKE) -C ./all/launcher/
player:
	@$(MAKE) -C ./all/player/
paks:
	@$(MAKE) -C ./all/paks/
utils:
	@$(MAKE) -C ./all/utils/
syncsettings:
	@$(MAKE) -C ./all/syncsettings/
platform-late:
	@$(MAKE) -C ./$(PLATFORM)

clean:
	cd ./$(PLATFORM)/libmsettings && $(MAKE) clean
	cd ./all/launcher/ && $(MAKE) clean
	cd ./all/player/ && $(MAKE) clean
	cd ./all/paks/ && $(MAKE) clean
	cd ./all/utils/ && $(MAKE) clean
	cd ./all/syncsettings/ && $(MAKE) clean
	cd ./$(PLATFORM) && $(MAKE) clean