# FlottyWM

A tiling Wayland compositor built on **wlroots**. Simple, lightweight, and fast.

## Philosophy

- **Minimalism** — only what you need to get work done. No animations, blur, shadows, rounded corners, or other eye candy.
- **Stability** — simple and predictable code. Less code = fewer bugs.
- **Optimization** — all about speed and low resource usage.
- **Practicality** — config is readable at a glance, hot-reload without restart, autostart out of the box.

## Features

- **Master-stack layout** — classic tiling window arrangement
- **10 workspaces**
- **Yellow border** for focused windows, dark gray for inactive
- **Floating mode** — any window can be switched to floating (`$mod+Shift+space`)
- **Clipboard** — full Wayland clipboard support, primary selection (middle-click paste), and external utilities (`wl-copy` / `wl-paste`)
- **Screencast** — screen capture for OBS, Discord, and others via PipeWire
- **Layer Shell** — panels, wallpapers, and overlays (waybar, swaybg, fuzzel, etc.)
- **Variable-based config** — `$mod`, `$term`, `$launcher`
- **Hot-reload** — `$mod+Shift+c` reloads config without restart
- **Autostart** — `exec-once` programs on compositor startup
- **Wallpaper out of the box** — default wallpaper + `swaybg` in config

## Dependencies (Debian 13 / Trixie)

```bash
sudo apt update
sudo apt install -y \
    build-essential meson ninja-build pkg-config \
    libwlroots-0.18-dev libwayland-dev wayland-protocols \
    libxkbcommon-dev libc6-dev
```

> If `libwlroots-0.19-dev` is available, use it and update `wlroots = dependency('wlroots-0.18')` in `meson.build`.

## Building

```bash
git clone https://github.com/Raspunt/flottyWM.git flottywm
cd flottywm
make build        # or make rebuild for a clean rebuild
```

Binary: `./build/flottywm`

## Running

```bash
# Nested mode (inside X11 or another Wayland compositor)
make run

# Or directly
./build/flottywm

# From TTY (DRM backend)
./build/flottywm
```

## Screencast (OBS, Discord)

FlottyWM supports `zwlr_screencopy_manager_v1` and `zwlr_export_dmabuf_manager_v1`.

```bash
sudo apt install pipewire xdg-desktop-portal xdg-desktop-portal-wlr
```

Start `pipewire` and `xdg-desktop-portal` (usually via systemd user session). In OBS select **PipeWire Screen Capture**.

> **Important:** do not run other `xdg-desktop-portal` backends (`-gtk`, `-kde`) alongside `-wlr`.

## Configuration

Config path: `~/.config/flottywm/config`. On first launch the compositor automatically creates the directory and copies default files (config + wallpaper).

Comments via `#` are supported.

```ini
gaps 8
outer_gaps 8
mfact 0.55

$term alacritty
$launcher fuzzel

# Autostart
exec-once swaybg -i ~/.config/flottywm/wallpaper.jpg -m fill
# exec-once waybar

$mod Alt

bind $mod+Return exec $term
bind $mod+d exec $launcher
bind $mod+Escape exit
bind $mod+q kill
bind $mod+Shift+space togglefloating
bind $mod+Shift+c reload

# Workspaces
bind $mod+1 workspace 1
bind $mod+2 workspace 2
# ...
bind $mod+Shift+1 movetoworkspace 1
bind $mod+Shift+2 movetoworkspace 2
# ...

# Focus
bind $mod+h focusprev
bind $mod+l focusnext
bind $mod+Left focusleft
bind $mod+Right focusright
bind $mod+Up focusup
bind $mod+Down focusdown

# Swap windows
bind $mod+Shift+h moveprev
bind $mod+Shift+l movenext

# Resize
bind $mod+Ctrl+Left resizeleft
bind $mod+Ctrl+Right resizeright
bind $mod+Ctrl+Up resizeup
bind $mod+Ctrl+Down resizedown
```

### Available commands

| Command | Description |
|---------|-------------|
| `env <NAME> <VALUE>` | Environment variable |
| `exec <program>` | Run a program |
| `exec-once <command>` | Autostart |
| `exit` | Exit the compositor |
| `workspace <N>` | Switch to workspace |
| `movetoworkspace <N>` | Move window to workspace |
| `focusprev` / `focusnext` | Focus by list order |
| `focusleft/right/up/down` | Focus in direction |
| `moveprev` / `movenext` | Swap window position |
| `togglefloating` | Toggle floating / tiled |
| `reload` | Reload config |
| `kill` | Close window |
| `resizeleft/right` | Resize master area width |
| `resizeup/down` | Resize floating window |

### Settings

| Parameter | Description | Default |
|-----------|-------------|---------|
| `gaps` | Gap between windows | `8` |
| `outer_gaps` | Outer screen margins | `8` |
| `mfact` | Master area screen share | `0.55` |

## Mouse control

- **Win + Right click** — drag floating window
- **Click on window** — focus

## License

MIT
