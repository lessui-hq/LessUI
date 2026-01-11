#keymon

.PHONY: all clean

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS = $(OPT_FLAGS) -lrt -ldl -Wl,--gc-sections -s
CFLAGS += -I.
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror

all:
	$(CC) bl_enable.c -o bl_enable $(CFLAGS)
	$(CC) bl_disable.c -o bl_disable $(CFLAGS)
clean:
	rm -rf bl_enable bl_disable