# shui - Persistent UI daemon for shell scripts
#
# Uses shared build patterns from common/build.mk

TARGET = shui

# Utils are at workspace/all/utils/foo/ - 3 levels to workspace/
PLATFORM_DEPTH = ../../../

# Include parson JSON library
EXTRA_INCDIR = -I..
EXTRA_SOURCE = ../parson/parson.c shui_utils.c fonts.c ipc.c ui_message.c ui_list.c ui_keyboard.c ui_progress.c animated_value.c
EXTRA_CFLAGS = -std=gnu99

# Skip setsid() on SDL1 platforms - fbdev driver hangs without controlling terminal
ifeq ($(PLATFORM),trimuismart)
EXTRA_CFLAGS += -DSKIP_SETSID
endif

include ../../common/build.mk