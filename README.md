![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-oneline.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-preview.png)
![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar-switch.png)

![](https://scan.coverity.com/projects/22494/badge.svg)
![](https://api.travis-ci.com/LBCrion/sfwbar.svg)

### SFWBar

SFWBar (Sway Floating Window Bar) is a flexible taskbar application for
wayland compositors, designed with a stacking layout in mind. 
Originally developed for [Sway](https://github.com/swaywm/sway), SFWBar
will work with other wayland compositors supporting layer shell protocol,
and the taskbar functionality shall for with any compositor supportinig
foreign toplevel protocol, but the pager, switcher and placement 
functionality requires sway (or at least i3 IPC support).

# Please note that configuration file format has changed between 0.9 and 1.0 series 

SFWBar is licensed under GNU GPL
Weather icons are from yr.no and are licensed under MIT license 

## SFWBar implements the following features:
1. Taskbar - to control floating windows
1. Task Switcher - to allow switching active window with a keyboard (Alt-Tab)
1. Pager - to allow switching between workspaces
1. Tray - a systm tray using status notification item protocol
1. Window placement engine - to open new windows in more logical locations
1. A simple widget set to display information from system files

## Compiling from Source

Install dependencies:
* gtk3
* gtk-layer-shell
* json-c

Compile instructions:
```no-highlight
meson build
ninja -C build
sudo ninja -C build install
```

## Configuration
Copy sfwbar.config from /usr/share/sfwbar/ to ~/.config/sfwbar/
For more information on the format of configuration file, please see the
[man page](doc/sfwbar.rst)

you may want to add the following lines to your sway config file to open windows
as floating by default:

```no-highlight
# open new windows as floating by default
for_window [app_id="[.]*"] floating enable
# set Alt-tab as a task switcher combo
bindsym Alt+Tab bar hidden_state toggle 
# set $mod+c to hide/unhide taskbar 
bindsym $mod+c bar mode toggle
```

