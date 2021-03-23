![](https://github.com/LBCrion/sfwbar/blob/main/.github/sfwbar.png)

### SFWBar

SFWBar (Sway Floating Window Bar) is a flexible taskbar application for
[Sway](https://github.com/swaywm/sway) wayland
compositor, designed with a stacking layout in mind. SFWBar will work 
with other wayland compositors supporting layer shell protocol, but 
taskbar, switcher and pager functionality requires sway (or at least i3
IPC support).

## SFWBar implements the following features:
1. Taskbar - to control floating windows
1. Task Switcher - to allow switching active window with a keyboard (Alt-Tab)
1. Pager - to allow switching between workspaces
1. Window placement engine - to open new windows in more logical locations
1. A simple widget set to display information from system files

## Compiling from Source

Install dependencies:
* gtk3
* gtk-layer-shell
* libucl

Compile instructions:
`meson build
ninja -C build
sudo ninja -C build install`

## Configuration
Copy sfwbar.config and sfwbar.css from /usr/share/sfwbar/ to ~/.config/sfwbar/
For more information on the format of configuration file, please see the
[man page](doc/sfwbar.rst)

you may want to add the following lines to your sway config file to open windows
as floating by default:

`# open new windows as floating by default
for_window [app_id="[.]*"] floating enable
# set Alt-tab as a task switcher combo
bindsym Alt+Tab bar hidden_state toggle 
# set $mod+c to hide/unhide taskbar 
bindsym $mod+c bar mode toggle`

