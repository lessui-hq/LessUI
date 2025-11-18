###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

TARGET = minui
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/api.c ../common/log.c ../common/collections.c ../common/pad.c ../common/gfx_text.c ../../$(PLATFORM)/platform/platform.c
HEADERS = $(wildcard ../common/*.h) $(wildcard ../../$(PLATFORM)/platform/*.h)

CC = $(CROSS_COMPILE)gcc
CFLAGS   = $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) $(LOG_FLAGS) -Ofast -std=gnu99
LDFLAGS	 = -ldl -lmsettings $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

$(PRODUCT): $(SOURCE) $(HEADERS) | $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make
