ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
          -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
          -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror
FLAGS = -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s $(CFLAGS)

all: $(PREFIX)/include/msettings.h
	$(CC) $(TARGET).c ../../all/common/log.c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make