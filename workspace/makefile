# workspace (all)

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

.PHONY: all

all:
	cd ./$(PLATFORM)/libmsettings && make
	cd ./$(PLATFORM) && make early # eg. other libs
	cd ./$(PLATFORM)/keymon && make
	cd ./all/minui/ && make
	cd ./all/minarch/ && make
	cd ./all/paks/ && make
	cd ./all/utils/minui-keyboard/ && make
	cd ./all/utils/minui-list/ && make
	cd ./all/utils/minui-presenter/ && make
	cd ./all/utils/jq/ && make
	cd ./all/syncsettings/ && make
	cd ./$(PLATFORM) && make

clean:
	cd ./$(PLATFORM)/libmsettings && make clean
	cd ./$(PLATFORM)/keymon && make clean
	cd ./all/minui/ && make clean
	cd ./all/minarch/ && make clean
	cd ./all/paks/ && make clean
	cd ./all/utils/minui-keyboard/ && make clean
	cd ./all/utils/minui-list/ && make clean
	cd ./all/utils/minui-presenter/ && make clean
	cd ./all/utils/jq/ && make clean
	cd ./all/syncsettings/ && make clean
	cd ./$(PLATFORM) && make clean