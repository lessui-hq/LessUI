#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM = $(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL2 = other/sdl2
REQUIRES_DTC = other/dtc
REQUIRES_FBSET = other/fbset

# Platform-specific binaries (can run in parallel after readmes)
PLATFORM_TARGETS = boot init show

.PHONY: $(PLATFORM_TARGETS)

all: readmes $(PLATFORM_TARGETS)

boot:
	@cd boot && ./build.sh

init:
	@$(MAKE) -C init

.PHONY: show
show:
	@$(MAKE) -C show

early: $(REQUIRES_SDL2) $(REQUIRES_DTC) $(REQUIRES_FBSET)
	@mkdir -p other
	@cd $(REQUIRES_DTC) && CC=${CROSS_COMPILE}gcc $(MAKE) -s NO_PYTHON=1 dtc
	@cd $(REQUIRES_FBSET) && $(MAKE) -s
	@cd $(REQUIRES_SDL2) && MAKEFLAGS=--no-print-directory CFLAGS="-O2 -mcpu=cortex-a53 -ffast-math -ftree-vectorize" \
		./configure CC=${CROSS_COMPILE}gcc --host=aarch64-buildroot-linux-gnu \
		  --disable-oss \
		  --disable-rpath \
		  --disable-arts \
		  --disable-esd \
		  --disable-dbus \
		  --disable-diskaudio \
		  --disable-dummyaudio \
		  --disable-pulseaudio \
		  --disable-video-kmsdrm \
		  --disable-video-vivante \
		  --disable-video-cocoa \
		  --disable-video-metal \
		  --disable-video-dummy \
		  --disable-video-offscreen \
		  --disable-video-x11 \
		  --disable-video-vulkan \
		  --disable-ime \
		  --disable-ibus \
		  --disable-fcitx \
		  --disable-joystick-mfi \
		  --disable-directx \
		  --disable-xinput \
		  --disable-wasapi \
		  --disable-hidapi-joystick \
		  --disable-hidapi-libusb \
		  --disable-joystick-virtual \
		  --enable-video-mali \
		  --disable-libsamplerate >/dev/null && $(MAKE) -s
	@# copy libs from toolchain (later moved to build)
	@cp $(PREFIX)/lib/libSDL2_image-2.0.so.0.0.1 ../libSDL2_image-2.0.so.0
	@cp $(PREFIX)/lib/libSDL2_ttf-2.0.so.0.14.0 ../libSDL2_ttf-2.0.so.0

clean:
	@cd boot && rm -rf output
	@cd $(REQUIRES_FBSET) && $(MAKE) -s clean
	@rm -f ./boot/*.bmp

$(REQUIRES_SDL2):
	git clone https://github.com/JohnnyonFlame/SDL-malifbdev-rot $(REQUIRES_SDL2)

$(REQUIRES_DTC):
	git clone https://github.com/dgibson/dtc $(REQUIRES_DTC)

$(REQUIRES_FBSET):
	git clone https://github.com/shauninman/union-fbset $(REQUIRES_FBSET)

include ../all/readmes/Makefile