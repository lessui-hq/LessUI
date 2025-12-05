# shellui - Persistent UI daemon for shell scripts
#
# Uses shared build patterns from common/build.mk

TARGET = shellui

# Utils are at workspace/all/utils/foo/ - 3 levels to workspace/
PLATFORM_DEPTH = ../../../

# Include parson JSON library
EXTRA_INCDIR = -I..
EXTRA_SOURCE = ../parson/parson.c ipc.c ui_message.c ui_list.c ui_keyboard.c
EXTRA_CFLAGS = -std=gnu99

include ../../common/build.mk
