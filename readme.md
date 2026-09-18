# Astralia Shell

## Important

- Supported Display Server: Wayland
- Supported Compositors: ShojiWM, Hyprland
- Requires OpenGL ES 3.2

## Prerequisite

**General**

```bash
sudo pacman -Syu --needed base-devel meson ninja mesa wayland wayland-protocols libxkbcommon sdbus-cpp freetype2 fontconfig cairo pango harfbuzz glib2 libsecret libsodium polkit pipewire wireplumber curl libqalculate libxml2 md4c nlohmann-json libical jemalloc stb ffmpeg
```

**Wayland**

```bash

```

**X11**

```bash

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
