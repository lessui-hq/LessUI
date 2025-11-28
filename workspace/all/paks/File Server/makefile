###########################################################
# File Transfer - Web-based file transfer tool
# Downloads dufs binaries from GitHub releases
#
# Hook interface:
#   setup   - Download both architectures, install to common/bin
#   build   - (not implemented - no compilation needed)
#   install - Construct pak (copy scripts to build dir)
#   clean   - Remove downloads
###########################################################

DUFS_VERSION := v0.45.0

bin/arm/dufs:
	mkdir -p bin/arm
	curl -sSL -o bin/arm/dufs.tar.gz "https://github.com/sigoden/dufs/releases/download/$(DUFS_VERSION)/dufs-$(DUFS_VERSION)-armv7-unknown-linux-musleabihf.tar.gz"
	tar -xzf bin/arm/dufs.tar.gz -C bin/arm
	rm bin/arm/dufs.tar.gz
	chmod +x bin/arm/dufs

bin/arm64/dufs:
	mkdir -p bin/arm64
	curl -sSL -o bin/arm64/dufs.tar.gz "https://github.com/sigoden/dufs/releases/download/$(DUFS_VERSION)/dufs-$(DUFS_VERSION)-aarch64-unknown-linux-musl.tar.gz"
	tar -xzf bin/arm64/dufs.tar.gz -C bin/arm64
	rm bin/arm64/dufs.tar.gz
	chmod +x bin/arm64/dufs

# Legacy target
all: setup

# Setup hook - download and install to common/bin (runs once)
setup: bin/arm/dufs bin/arm64/dufs
	@if [ -n "$(DESTDIR)" ]; then \
		mkdir -p $(DESTDIR)/arm $(DESTDIR)/arm64; \
		cp bin/arm/dufs $(DESTDIR)/arm/; \
		cp bin/arm64/dufs $(DESTDIR)/arm64/; \
	fi

# Build hook - not needed (no compilation)
build:
	@true

# Install hook - construct pak for platform (runs per-platform)
install:
	@# Pak construction handled by main makefile loop
	@# This hook exists for future use if needed
	@true

# Clean hook
clean:
	rm -rf bin/arm/dufs bin/arm64/dufs

.PHONY: all setup build install clean
