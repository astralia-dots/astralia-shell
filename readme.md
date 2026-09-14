# Adastria Shell

## Important

- Supported Display Server: Wayland
- Supported Compositors: ShojiWM, Hyprland
- Requires an OpenGL ES 3.2 driver

## Prerequisite

**Arch**

```bash
sudo pacman -Syu --needed base-devel meson ninja mesa wayland wayland-protocols libxkbcommon sdbus-cpp freetype2 fontconfig cairo pango harfbuzz glib2 libsecret libsodium polkit pipewire wireplumber curl libqalculate libxml2 md4c nlohmann-json libical jemalloc stb ffmpeg
```

## Installation

```bash
git clone https://github.com/Hq9afk/adastria-shell.git
cd adastria-shell
./build.sh install
```

## Running the shell

**Regular mode**
```bash
adastria
```

**Debug mode**
```bash
adastria debug
```
Or run adastria-shell in regular mode and read the logs at `~/.local/state/adastria-shell/adastria-shell.log`
