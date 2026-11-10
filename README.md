# Desktop Weather

Rain that falls on your desktop — and on your windows.

Desktop Weather draws a transparent, click-through overlay across every connected
monitor and simulates raindrops over it. A KWin script feeds live window geometry
to the app over D-Bus, so the drops don't just fall past your windows: they land
on title bars, run down the sides, and pool where they hit a surface.

## About the project

The app is a Qt 6 / KDE Frameworks 6 desktop application written in C++.

- **Overlay** (`src/pixel_overlay.cpp`) — one `wlr-layer-shell` surface per
  connected screen, on the `top` layer. A layer surface is not a window: the
  compositor owns its placement, so it never appears in the taskbar, the task
  switcher or the pager, and it cannot take focus. An empty input region lets
  the pointer straight through, so everything underneath stays clickable. A
  timer advances and repaints ~1000 drops per screen at roughly 60 FPS.
- **Raindrops** (`src/raindrop.cpp`) — each drop has its own position, velocity,
  gravity and fall angle, integrated with real delta time so the animation speed
  is frame-rate independent. Drops spawn at a random screen edge, collide with
  the nearest window surface (top, left or right), and evaporate after 30
  seconds if they get stuck.
- **Window tracking** (`src/WindowTracker.cpp` + `kwin/`) — a KWin JavaScript
  script snapshots the stacking order (title, app id, screen name, position,
  size, fullscreen and minimized state) and pushes it to the app via the
  `org.kde.desktopweather` D-Bus service. Snapshots are sent on window
  add/remove/move/activate and every 2 seconds as a fallback; identical
  snapshots are discarded. Positions are reported relative to the window's own
  screen, which is why each overlay only collides with windows whose
  `screenName` matches its own output.
- **Tray icon** (`src/main.cpp`) — a `KStatusNotifierItem` with a menu action
  that re-rolls the rain angle on every screen at once. A screen whose windows
  cover ≥85% of it stops rendering completely rather than merely hiding, so
  fullscreen video and games pay no CPU cost; the other screens keep raining.

`plasmashell`, `desktop-weather` itself and `xwaylandvideobridge` are ignored as
collision targets (see `src/GLOBAL_VARS.h`).

## Compatibility

| | |
|---|---|
| Desktop | KDE Plasma 6 (requires KWin — the window tracker is a KWin script) |
| Session | Wayland (needs the `zwlr_layer_shell_v1` protocol, which KWin provides) |
| OS | Linux |

The overlay is a layer-shell surface, and layer-shell is Wayland-only. On an
X11 session the app still runs, but it falls back to an ordinary top-level
window — which the compositor places itself and which may show up in the
taskbar — and logs a warning saying so. GNOME, XFCE and other non-KWin desktops
are not supported.

### Build requirements

- CMake ≥ 3.20 and a C++ compiler with C++17 support
- Extra CMake Modules (ECM) ≥ 6.0
- Qt 6.5+ — Core, DBus, Gui, Quick, QuickControls2, Widgets
- LayerShellQt 6 (`layer-shell-qt`)
- KDE Frameworks 6 — CoreAddons, I18n, IconThemes, Kirigami,
  QQC2DesktopStyle, StatusNotifierItem

Install them with your distribution's packages:

```bash
# Fedora / KDE Spin
sudo dnf install cmake gcc-c++ extra-cmake-modules \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kiconthemes-devel \
    kf6-kirigami-devel kf6-kstatusnotifieritem-devel qqc2-desktop-style \
    layer-shell-qt-devel

# Arch Linux
sudo pacman -S cmake extra-cmake-modules qt6-base qt6-declarative \
    kcoreaddons ki18n kiconthemes kirigami kstatusnotifieritem qqc2-desktop-style \
    layer-shell-qt

# Debian / Ubuntu / KDE neon
sudo apt install cmake g++ extra-cmake-modules qt6-base-dev qt6-declarative-dev \
    libkf6coreaddons-dev libkf6i18n-dev libkf6iconthemes-dev \
    libkf6kirigami-dev libkf6statusnotifieritem-dev qt6-qqc2-desktop-style \
    liblayershellqtinterface-dev
```

## Install

Clone and build:

```bash
git clone https://github.com/Shmankus/desktopWeather.git
cd desktopWeather
./reinstall.sh
```

`reinstall.sh` configures, builds and installs into `~/.local` — no root
needed. It is equivalent to:

```bash
cmake -B build/ --install-prefix ~/.local
cmake --build build/
cmake --install build/
```

This installs the `desktop-weather` binary, the desktop entry, and the KWin
script to `~/.local/share/kwin/scripts/desktop-weather-window-tracker`.

### Enable the KWin script

The rain can only react to your windows once the tracker script is enabled:

**System Settings → Window Management → KWin Scripts → Desktop Weather Window
Tracker** → tick it and apply.

Or from the terminal:

```bash
kwriteconfig6 --file kwinrc --group Plugins \
    --key desktop-weather-window-trackerEnabled true
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.start
```

The app checks on startup whether the script is loaded and warns on stderr if it
isn't.

### Run

```bash
desktop-weather        # installed in ~/.local/bin
```

If `~/.local/bin` isn't on your `PATH`, launch it from your application menu
("Desktop Weather"), or run the development build straight from the source tree:

```bash
./run.sh
```

`run.sh` sources `build/prefix.sh` so the locally installed QML modules and
libraries are found, then execs the binary from `build/bin/`. It requires that
`./reinstall.sh` has been run at least once.

### Uninstall

```bash
cmake --build build/ --target uninstall
kwriteconfig6 --file kwinrc --group Plugins \
    --key desktop-weather-window-trackerEnabled false
```

## Known limitations

- Screens are enumerated once at startup, so plugging in or unplugging a
  monitor while the app runs needs a restart.
- The drop count (1000 per screen) and the rain angle range (70–90°) are
  compile-time constants; there is no settings UI yet.
- `src/Main.qml` is still the Kirigami starter window and is not shown by the
  running app.

## License

The KWin script is GPL-3.0-or-later.
