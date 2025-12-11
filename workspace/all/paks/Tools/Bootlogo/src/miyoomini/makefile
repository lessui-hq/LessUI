###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=miyoomini make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../../../../../$(PLATFORM)/platform/makefile.env

###########################################################

CC = $(CROSS_COMPILE)gcc
# OPT_FLAGS from parent makefile (-O3 for release, -O0 -g for debug)
OPT_FLAGS ?= -O3
CFLAGS = $(ARCH) -fomit-frame-pointer $(OPT_FLAGS)
CFLAGS += -Wall -Wextra -Wsign-compare -Wshadow -Wnull-dereference -Wundef \
          -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter \
          -Wno-cast-align -Wno-missing-field-initializers -Wno-format -Werror

# Output goes to parent directory's build folder
OUTDIR = ../../build/$(PLATFORM)

all:
	mkdir -p $(OUTDIR)
	$(CC) logoread.c -o $(OUTDIR)/logoread.elf $(CFLAGS)
	$(CC) logomake.c -o $(OUTDIR)/logomake.elf $(CFLAGS)
	$(CC) logowrite.c -o $(OUTDIR)/logowrite.elf $(CFLAGS)

clean:
	rm -rf ../../build