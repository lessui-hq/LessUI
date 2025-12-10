# workspace/makefile - builds all components for a platform

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

# Components that can run in parallel after libs
# keymon is now built in workspace/all/utils/keymon (unified implementation)
COMPONENTS = minui minarch paks utils syncsettings platform-late

.PHONY: all libs $(COMPONENTS)

all: libs $(COMPONENTS)

# libs must complete first (libmsettings + early platform deps)
libs:
	$(MAKE) -C ./$(PLATFORM)/libmsettings
	$(MAKE) -C ./$(PLATFORM) early

$(COMPONENTS): libs

minui:
	$(MAKE) -C ./all/minui/
minarch:
	$(MAKE) -C ./all/minarch/
paks:
	$(MAKE) -C ./all/paks/
utils:
	$(MAKE) -C ./all/utils/
syncsettings:
	$(MAKE) -C ./all/syncsettings/
platform-late:
	$(MAKE) -C ./$(PLATFORM)

clean:
	cd ./$(PLATFORM)/libmsettings && $(MAKE) clean
	cd ./all/minui/ && $(MAKE) clean
	cd ./all/minarch/ && $(MAKE) clean
	cd ./all/paks/ && $(MAKE) clean
	cd ./all/utils/ && $(MAKE) clean
	cd ./all/syncsettings/ && $(MAKE) clean
	cd ./$(PLATFORM) && $(MAKE) clean