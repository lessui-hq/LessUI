# shellcheck shell=bash
# Sourced by generated launch.sh
# rg35xxplus loop hook
# Source HDMI export before each minui/pak run

pre_minui_hook() {
	. "$HDMI_EXPORT_PATH"
}

pre_pak_hook() {
	. "$HDMI_EXPORT_PATH"
}
