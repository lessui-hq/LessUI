ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = init
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS = -marm -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -mcpu=cortex-a53
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
LDFLAGS = -lSDL

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)