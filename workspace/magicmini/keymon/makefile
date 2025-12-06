#keymon

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = keymon

CC = $(CROSS_COMPILE)gcc
CFLAGS	= -Os -lmsettings -lrt -ldl -Wl,--gc-sections -s
CFLAGS  += -I. -I../../all/common -I../platform/ -DPLATFORM=\"$(UNION_PLATFORM)\"
CFLAGS  += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
           -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
           -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror

all:
	$(CC) $(TARGET).c ../../all/common/log.c -o $(TARGET).elf $(CFLAGS)
clean:
	rm -rf $(TARGET).elf