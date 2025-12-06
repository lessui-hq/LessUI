# shellcheck shell=bash
# Sourced by generated launch.sh
# magicmini post-env hook
# SDL audio, framebuffer clear, GPU/DMC governors

export SDL_AUDIODRIVER=alsa
amixer cset name='Playback Path' SPK

# Clear framebuffer
cat /dev/zero > /dev/fb0

# Export CPU path for loop
export CPU_PATH
