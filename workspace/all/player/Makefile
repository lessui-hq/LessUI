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

TARGET = player
INCDIR = -I. -I./libretro-common/include/ -I../common/ -I../launcher/ -isystem ../vendor/stb/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/nointro_parser.c \
         ../common/api.c ../common/ui_layout.c ../common/log.c ../common/pad.c \
         ../common/gfx_text.c ../launcher/launcher_file_utils.c ../common/platform_variant.c \
         player_archive.c player_memory.c player_state.c \
         player_paths.c player_cpu.c player_input.c player_mappings.c \
         player_video_convert.c player_rotation.c player_config.c player_context.c \
         player_menu.c player_env.c player_game.c player_scaler.c player_core.c \
         frame_pacer.c \
         ../../$(PLATFORM)/platform/platform.c

# Add shared rendering modules
SOURCE += ../common/effect_system.c ../common/effect_generate.c ../common/render_common.c

# Add effect support - SDL2 platforms use render_sdl2 + effect_utils, SDL1 platforms use effect_surface
ifeq ($(SDL),SDL2)
SOURCE += ../common/render_sdl2.c ../common/effect_utils.c
else
SOURCE += ../common/effect_surface.c
endif
HEADERS = $(wildcard *.h) $(wildcard ../common/*.h) $(wildcard ../launcher/*.h) $(wildcard ../../$(PLATFORM)/platform/*.h)

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS = $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) $(LOG_FLAGS) $(OPT_FLAGS) -std=gnu99
CFLAGS += -flto
LDFLAGS = -ldl $(LIBS) -lmsettings -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz
CFLAGS += $(WARN_FLAGS)
# CFLAGS  += -fsanitize=address -fno-common
# LDFLAGS += -lasan

BUILD_DATE != date +%Y.%m.%d
BUILD_HASH ?= unknown
CFLAGS += -DBUILD_DATE=\"${BUILD_DATE}\" -DBUILD_HASH=\"${BUILD_HASH}\"

# CFLAGS += -DBUILD_DATE=\"20230326\" -DBUILD_HASH=\"d34db33f\"

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

$(PRODUCT): $(SOURCE) $(HEADERS) | libretro-common $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

all: $(PRODUCT)

clean:
	rm -f $(PRODUCT)

libretro-common:
	@[ -d libretro-common ] || git clone https://github.com/libretro/libretro-common

$(PREFIX)/include/msettings.h:
	@cd /root/workspace/$(PLATFORM)/libmsettings && $(MAKE)

.PHONY: all clean