# Astralia Shell

## Important

- Supported Display Server: Wayland
- Supported Compositors: Hyprland, Sway
- Requires OpenGL ES 2.0

## Prerequisite

**Arch**

```bash
sudo pacman -Syu --needed base-devel cairo curl fd ffmpeg fontconfig freetype2 glib2 harfbuzz jemalloc libical libqalculate libsecret libsodium libxkbcommon libxml2 md4c mesa meson ninja nlohmann-json pango pipewire polkit sdbus-cpp stb wayland wayland-protocols wireplumber
```

## Installation

```bash
git clone https://github.com/Hq9afk/astralia-shell.git
cd astralia-shell
./build.sh install
```

## Running the shell

**Regular mode**
```bash
astralia
```

**Debug mode**
```bash
astralia debug
```
Or run astralia-shell in regular mode and read the logs at `~/.local/state/astralia/astralia.log`
