###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

TARGET = syncsettings
SOURCE = $(TARGET).c
HEADERS = $(wildcard ../../$(PLATFORM)/platform/msettings.h)

CC = $(CROSS_COMPILE)gcc
CFLAGS = $(ARCH) -DPLATFORM=\"$(PLATFORM)\"
LDFLAGS = -Os -lmsettings -lrt -ldl -Wl,--gc-sections -s

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

$(PRODUCT): $(SOURCE) $(HEADERS) | $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && $(MAKE)