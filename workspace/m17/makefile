###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

all: readmes
	cd ./boot && ./build.sh

early:
	# early

clean:
	cd ./overclock && $(MAKE) clean
	rm -f ./boot/*.bmp

###########################################################

include ../all/readmes/makefile