###########################################################
# jq - Lightweight command-line JSON processor
# Downloads prebuilt binaries from GitHub releases
#
# Hook interface:
#   setup   - Download both architectures, install to common/bin
#   build   - (not implemented - no compilation needed)
#   install - (not implemented - happens during setup)
#   clean   - Remove downloads
###########################################################

JQ_VERSION := 1.7.1

# Download targets
bin/arm/jq:
	mkdir -p bin/arm
	wget -q -O $@ https://github.com/jqlang/jq/releases/download/jq-$(JQ_VERSION)/jq-linux-armhf
	chmod +x $@

bin/arm64/jq:
	mkdir -p bin/arm64
	wget -q -O $@ https://github.com/jqlang/jq/releases/download/jq-$(JQ_VERSION)/jq-linux-arm64
	chmod +x $@

# Legacy target for direct invocation
all: setup

# Setup hook - download and install to common/bin (runs once)
setup: bin/arm/jq bin/arm64/jq
	@if [ -n "$(DESTDIR)" ]; then \
		mkdir -p $(DESTDIR)/arm $(DESTDIR)/arm64; \
		cp bin/arm/jq $(DESTDIR)/arm/; \
		cp bin/arm64/jq $(DESTDIR)/arm64/; \
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