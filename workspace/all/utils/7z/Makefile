###########################################################
# 7z - High compression file archiver
# Downloads prebuilt static binaries from 7-zip.org
#
# Hook interface:
#   setup   - Download both architectures, install to common/bin
#   build   - (not implemented - no compilation needed)
#   install - (not implemented - happens during setup)
#   clean   - Remove downloads
###########################################################

7Z_VERSION := 2501

# Download URLs (static console builds)
URL_ARM := https://www.7-zip.org/a/7z$(7Z_VERSION)-linux-arm.tar.xz
URL_ARM64 := https://www.7-zip.org/a/7z$(7Z_VERSION)-linux-arm64.tar.xz

# Download targets - extract 7zzs (static build) and rename to 7z
bin/arm/7z:
	mkdir -p bin/arm
	wget -q -O - $(URL_ARM) | tar -xJf - -C bin/arm 7zzs
	mv bin/arm/7zzs $@
	chmod +x $@

bin/arm64/7z:
	mkdir -p bin/arm64
	wget -q -O - $(URL_ARM64) | tar -xJf - -C bin/arm64 7zzs
	mv bin/arm64/7zzs $@
	chmod +x $@

# Legacy target for direct invocation
all: setup

# Setup hook - download and install to common/bin (runs once)
setup: bin/arm/7z bin/arm64/7z
	@if [ -n "$(DESTDIR)" ]; then \
		mkdir -p $(DESTDIR)/arm $(DESTDIR)/arm64; \
		cp bin/arm/7z $(DESTDIR)/arm/; \
		cp bin/arm64/7z $(DESTDIR)/arm64/; \
	fi

# Build hook - not needed (no compilation)
build:
	@true

# Install hook - not needed (installed during setup)
install:
	@true

# Clean hook
clean:
	rm -rf bin

.PHONY: all setup build install clean