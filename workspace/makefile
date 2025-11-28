# workspace/makefile - builds all components for a platform

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

# Components that can run in parallel after libs
COMPONENTS = keymon minui minarch paks utils syncsettings platform-late

.PHONY: all libs $(COMPONENTS)

all: libs $(COMPONENTS)

# libs must complete first (libmsettings + early platform deps)
libs:
	$(MAKE) -C ./$(PLATFORM)/libmsettings
	$(MAKE) -C ./$(PLATFORM) early

$(COMPONENTS): libs

keymon:
	$(MAKE) -C ./$(PLATFORM)/keymon
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
	cd ./$(PLATFORM)/libmsettings && make clean
	cd ./$(PLATFORM)/keymon && make clean
	cd ./all/minui/ && make clean
	cd ./all/minarch/ && make clean
	cd ./all/paks/ && make clean
	cd ./all/utils/ && make clean
	cd ./all/syncsettings/ && make clean
	cd ./$(PLATFORM) && make clean