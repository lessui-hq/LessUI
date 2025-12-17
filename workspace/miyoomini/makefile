###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL = other/sdl
REQUIRES_COMMANDER = other/DinguxCommander

# Platform-specific binaries (can run in parallel)
PLATFORM_TARGETS = blank batmon lumon overclock show

.PHONY: $(PLATFORM_TARGETS)

all: readmes $(PLATFORM_TARGETS)

blank:
	@$(MAKE) -C blank

batmon:
	@$(MAKE) -C batmon

lumon:
	@$(MAKE) -C lumon

overclock:
	@$(MAKE) -C overclock

.PHONY: show
show:
	@$(MAKE) -C show

early: $(REQUIRES_SDL) $(REQUIRES_COMMANDER)
	@cd other/sdl && MAKEFLAGS=--no-print-directory ./make.sh
	@cd other/logotweak && $(MAKE) -s
	@cd other/latency_reduction && $(MAKE) -s
	@cd other/DinguxCommander && $(MAKE) -s

clean:
	@cd blank && $(MAKE) -s clean
	@cd batmon && $(MAKE) -s clean
	@cd lumon && $(MAKE) -s clean
	@cd other/sdl && $(MAKE) -s distclean
	@cd other/latency_reduction && $(MAKE) -s clean
	@cd other/DinguxCommander && $(MAKE) -s clean

###########################################################

$(REQUIRES_SDL):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/SDL-1.2.git $(REQUIRES_SDL)
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile