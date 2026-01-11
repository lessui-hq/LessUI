###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=tg5050 make)
endif

###########################################################

REQUIRES_EVTEST = other/evtest
REQUIRES_JSTEST = other/jstest

all: readmes show

.PHONY: all early clean show
show:
	@$(MAKE) -C show

early: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST)
	@mkdir -p other
	@cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	@cd $(REQUIRES_JSTEST) && $(CROSS_COMPILE)gcc -o jstest jstest.c

clean:
	@echo "Clean complete"

###########################################################
$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

$(REQUIRES_JSTEST):
	git clone --depth 1 https://github.com/datrh/joyutils.git $(REQUIRES_JSTEST)

include ../all/readmes/Makefile