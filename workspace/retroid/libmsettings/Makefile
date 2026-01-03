ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

TARGET = msettings

.PHONY: build
.PHONY: clean

CC = $(CROSS_COMPILE)gcc

SYSROOT := $(shell $(CC) --print-sysroot)

INCLUDEDIR = $(SYSROOT)/usr/include
# OPT_FLAGS and LOG_FLAGS from parent makefile
OPT_FLAGS ?= -O3
LOG_FLAGS ?=
CFLAGS = -I$(INCLUDEDIR) -I../../all/common $(OPT_FLAGS) $(LOG_FLAGS)
LDFLAGS = -ldl -lrt -s

build:
	@$(CC) -c -Werror -fpic "$(TARGET).c" -Wl,--no-as-needed $(CFLAGS)
	@$(CC) -c -fpic "../../all/common/log.c" $(CFLAGS)
	@$(CC) -shared -o "lib$(TARGET).so" "$(TARGET).o" "log.o" $(LDFLAGS)
	@cp "$(TARGET).h" "$(PREFIX)/include"
	@cp "lib$(TARGET).so" "$(PREFIX)/lib"
clean:
	rm -f *.o
	rm -f "lib$(TARGET).so"
	rm -f $(PREFIX)/include/$(TARGET).h
	rm -f $(PREFIX)/lib/lib$(TARGET).so