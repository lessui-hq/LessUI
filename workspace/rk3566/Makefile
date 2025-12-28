###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_SDLCOMPAT = other/sdl12-compat

# keymon.elf is now built in workspace/all/utils
# LessOS doesn't need DinguxCommander (file manager for stock firmware)
all: readmes show

.PHONY: show
show:
	@$(MAKE) -C show

early: $(REQUIRES_SDLCOMPAT)/build
	@cd $(REQUIRES_SDLCOMPAT) && cmake --build build >/dev/null

clean:
	@cd $(REQUIRES_SDLCOMPAT) && rm -rf build

###########################################################

$(REQUIRES_SDLCOMPAT):
	git clone --depth 1 --branch minui-rgb30 https://github.com/shauninman/sdl12-compat.git $(REQUIRES_SDLCOMPAT)

$(REQUIRES_SDLCOMPAT)/build: $(REQUIRES_SDLCOMPAT)
	cd $(REQUIRES_SDLCOMPAT) && cmake --toolchain ./toolchain.cmake -Bbuild -DCMAKE_BUILD_TYPE=Release .

include ../all/readmes/Makefile