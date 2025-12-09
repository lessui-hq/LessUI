# LessUI Development Container
# Ubuntu 24.04 LTS - matches GitHub Actions runners
# Provides: clang-tidy, clang-format, scan-build, gcc, shellcheck for QA tasks

FROM ubuntu:24.04

# Install QA tools
RUN apt-get update && apt-get install -y \
    clang-tidy \
    clang-format \
    clang-tools \
    gcc \
    g++ \
    make \
    perl \
    shellcheck \
    lcov \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /lessui

# Default command: bash shell
CMD ["/bin/bash"]
