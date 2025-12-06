ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = show
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS  = -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
          -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
          -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
FLAGS	= -Os -lSDL2 -lSDL2_image -lrt -ldl -Wl,--gc-sections -s $(CFLAGS)

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)