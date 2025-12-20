ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = lumon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS = -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
LDFLAGS = $(OPT_FLAGS) -lmi_sys -lmi_disp

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)