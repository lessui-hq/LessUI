# shellcheck shell=bash
# Sourced by generated launch.sh
# rgb30 post-env hook
# SDL environment, FSCK cleanup

# Clean up JELOS filesystem check litter
rm -f "$SDCARD_PATH"/FSCK*.REC

# SDL environment for sdl12-compat
export SDL_VIDEODRIVER=kmsdrm
export SDL_AUDIODRIVER=alsa
