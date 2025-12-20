#overclock

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = overclock

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS = $(OPT_FLAGS) -lrt -ldl -Wl,--gc-sections -s
CFLAGS += -I. -DPLATFORM=\"$(UNION_PLATFORM)\"
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-missing-braces -Wno-format -Werror

all:
	$(CC) $(TARGET).c -o $(TARGET).elf $(CFLAGS)
clean:
	rm -rf $(TARGET).elf