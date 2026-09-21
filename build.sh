#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

cmd_setup() {
	sudo pacman -Syu --needed base-devel cairo curl fd ffmpeg fontconfig freetype2 glib2 harfbuzz jemalloc libical libqalculate libsecret libsodium libxkbcommon libxml2 md4c mesa meson ninja nlohmann-json pango pipewire polkit sdbus-cpp stb wayland wayland-protocols wireplumber
}

cmd_build() {
	if [ -d build ]; then
		meson setup --prefix=/usr --reconfigure build
	else
		meson setup --prefix=/usr build
	fi
	ninja -C build -j "${ASTRALIA_SHELL_BUILD_JOBS:-4}"
}

cmd_install() { cmd_build; sudo ninja -C build install; }
cmd_run() { astralia kill || true; cmd_install; astralia; }
cmd_test() { cmd_build; meson test -C build --print-errorlogs; }
cmd_uninstall() { sudo ninja -C build uninstall; }

main() {
	local cmd="${1:-build}"
	case "$cmd" in
		setup|build|install|run|test|uninstall) "cmd_$cmd" ;;
		*) echo "unknown command: $cmd" >&2; exit 2 ;;
	esac
}

main "$@"
