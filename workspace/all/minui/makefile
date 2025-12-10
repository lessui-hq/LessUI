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

include ../../$(PLATFORM)/platform/makefile.env
SDL ?= SDL

###########################################################

TARGET = minui
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/nointro_parser.c \
         ../common/api.c ../common/log.c ../common/collections.c ../common/pad.c \
         ../common/gfx_text.c ../common/platform_variant.c \
         minui_entry.c minui_launcher.c directory_index.c minui_str_compare.c \
         minui_state.c minui_m3u.c minui_map.c minui_file_utils.c minui_directory.c \
         minui_context.c minui_navigation.c minui_thumbnail.c recent_file.c \
         ../../$(PLATFORM)/platform/platform.c

# Add shared rendering modules
SOURCE += ../common/effect_system.c ../common/render_common.c

# Add effect support - SDL2 platforms use render_sdl2 + effect_utils, SDL1 platforms use effect_surface
ifeq ($(SDL),SDL2)
SOURCE += ../common/render_sdl2.c ../common/effect_utils.c
else
SOURCE += ../common/effect_surface.c
endif
HEADERS = $(wildcard *.h) $(wildcard ../common/*.h) $(wildcard ../../$(PLATFORM)/platform/*.h)

CC = $(CROSS_COMPILE)gcc
CFLAGS = $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) $(LOG_FLAGS) -O3 -std=gnu99
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
LDFLAGS = -ldl -lmsettings $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

$(PRODUCT): $(SOURCE) $(HEADERS) | $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && $(MAKE)