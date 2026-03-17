![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-dark.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-preview.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-mpd.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-tray.png)

![](https://scan.coverity.com/projects/22494/badge.svg)
![](https://github.com/LBCrion/sfwbar/actions/workflows/main.yml/badge.svg)

### SFWBar

SFWBar (S\* Floating Window Bar) is a flexible taskbar application for
wayland compositors, designed with a stacking layout in mind. It supports
the following features:
  - Taskbar (including windows style or tint2 style groupings).
  - Pager
  - Window switcher (i.e. alt-tab support).
  - Window placement engine.
  - Tray (using system notification protocol).
  - Volume control (using PulseAudio or ALSA).
  - Keyboard layout notification and control.
  - Start menu implementation.
  - Network status monitoring (using netlink / AF_ROUTE).
  - Wifi configuration (using NetworkManager or IWD).
  - Bluetooth management (using bluez).
  - Desktop notifications.
  - MPD control (playback and playlist management).
  - Privacy notification (using Pipewire).
  - Clock / calendar.
  - Session management control panel.
  - Power management control (using UPower interface).
  - Idle inhibitor.
  - Idle monitoring support.

In addition SFWBar implements a flexible control language for construction of
complex user defined widgets.

# Configuration format has changed significantly between 1.0beta16 and beta17.
# Most old configuration file should still be supported. But it's recommended
# to migrate configs to a new format.

SFWBar is licensed under GNU GPL.
Weather icons are from yr.no and are licensed under MIT license 

## Compiling from Source

Install dependencies:
* gtk3
* gtk-layer-shell
* json-c

Runtime dependencies:
* python is used by some widgets (i.e. battery and start menu widgets)
* symbolic icons are used by battery.widget

Compile instructions:
```no-highlight
meson setup build
ninja -C build
sudo ninja -C build install
```

## Install packages

* [Fedora](https://src.fedoraproject.org/rpms/sfwbar): `sudo dnf install sfwbar`
* [ArchLinux](https://aur.archlinux.org/packages/sfwbar): `yay -S sfwbar`
* [Debian](https://tracker.debian.org/pkg/sfwbar): `sudo apt install sfwbar`

## Configuration
Copy sfwbar.config from /usr/share/sfwbar/ to ~/.config/sfwbar/
If you prefer to start with something more like tint2 bar, you can
copy [t2.config](config/t2.config) into ~/.config/sfwbar/sfwbar.config
instead. If you want something like waybar, you can copy
[wbar.config](config/wbar.config) and if you prefer something from the
darker side, [w10.config](config/w10.config) could be for you.
For more information on the format of configuration file, please see the
[man page](doc/sfwbar.rst)
