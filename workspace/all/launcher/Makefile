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

include ../../$(PLATFORM)/platform/Makefile.env
include ../common/cflags.mk
SDL ?= SDL

###########################################################

TARGET = launcher
INCDIR = -I. -I../common/ -I../player/ -I../player/libretro-common/include/ -isystem ../vendor/stb/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/nointro_parser.c \
         ../common/api.c ../common/ui_layout.c ../common/log.c ../common/pad.c ../common/paths.c \
         ../common/gfx_text.c ../common/platform_variant.c ../common/stb_ds_impl.c \
         ../common/cpu.c \
         launcher_entry.c launcher_launcher.c directory_index.c launcher_str_compare.c \
         launcher_state.c launcher_m3u.c launcher_map.c launcher_file_utils.c launcher_directory.c \
         launcher_context.c launcher_navigation.c launcher_thumbnail.c recent_file.c \
         launcher_emu_cache.c launcher_res_cache.c \
         ../../$(PLATFORM)/platform/platform.c

# Add shared rendering modules
SOURCE += ../common/effect_system.c ../common/effect_generate.c ../common/render_common.c

# Add effect support - SDL2 platforms use render_sdl2 + effect_utils, SDL1 platforms use effect_surface
ifeq ($(SDL),SDL2)
SOURCE += ../common/render_sdl2.c ../common/effect_utils.c ../common/gl_video.c
else
SOURCE += ../common/effect_surface.c
endif

# Add libudev support for dynamic input device discovery (retroid, rgb30)
ifeq ($(HAS_LIBUDEV),1)
SOURCE += ../common/udev_input.c
LIBS += -ludev
endif
HEADERS = $(wildcard *.h) $(wildcard ../common/*.h) $(wildcard ../../$(PLATFORM)/platform/*.h)

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS += $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) $(LOG_FLAGS) $(OPT_FLAGS) -std=gnu99
CFLAGS += $(WARN_FLAGS) $(EXTRA_CFLAGS)
LDFLAGS += -ldl -lmsettings $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

$(PRODUCT): $(SOURCE) $(HEADERS) | $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	@cd /root/workspace/$(PLATFORM)/libmsettings && $(MAKE)

.PHONY: all clean