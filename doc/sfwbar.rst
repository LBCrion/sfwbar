SFWBar
######

############################
Sway Floating Window taskBar
############################

:Copyright: GPLv3+
:Manual section: 1

SYNOPSIS
========
| **sfwbar** [options]

DESCRIPTION
===========
**SFWBar** is a taskbar for wayland compositors. Originally written for Sway,
it should work with any compositor supporting layer-shell protocol. SFWBar
assists in handling of floating windows on a wayland desktop. It provides a
taskbar, a pager, a task switcher, a system tray, a floating window placement
engine, a simple widget set for display data extracted from various system
files.
SFWBar can work with any wayland compositor supporting layer shell protocol.
Taskbar and switcher require either sway or wlr-foreign-toplevel protocol
support. Placer and  pager require sway.

OPTIONS
=======
SFWBar executable can be invoked with the following options:

-f | --config
  Specify a filename of a configuration file

-c | --css
  Specify a filename of a css file

-s | --socket
  Specify a location of sway ipc socket

-m | --monitor
  Specify a monitor to display the bar on ("-m list" to list available monitors)

-b | --bar_id
  Specify a sway bar_id on which sfwbar will listen for status changes

CONFIGURATION
=============
SFWBar reads configuration from a config file (sfwbar.config by default). The
program checks users XDG config directory (usually ~/.config/sfwbar/) for this
file, followed by system xdg data directories. Additionally, user can specify
a location and a name of the config file using ``-f`` command line option.

Appearance of the program can be specified using CSS properties, these
are sourced either from the css section of the main configuration file or
from a file with a .css extension with the same base name as the config file
located in the same directory as the config file. The name of the css file 
can be also specified using ``-c`` option.

The config file consists of the following top level sections:

Placer
------
Placer section enables intelligent placement of new floating windows. If
enabled the program will first attempt to place the window in a location, where
it won't overlap with other windows. If such location doesn't exist, the window
will be placed in a cascading pattern from top-left to bottom-right. The Placer
declaration accepts parameters "xstep" and "ystep" that specify the
steps in the window cascade. These are specified in percentage of the desktop
dimensions. The cascade placement will start at a location specified by "xorigin"
"yorigin" parameters. I.e.::

  placer {
    xorigin = 5
    yorigin = 5
    xstep = 5
    ystep = 5
    children = false
  }

"children" parameter specifies if new windows opened by a program with other
windows already opened should be placed. These are usually dialog windows and
SFWBar won't place them by default. If the placer section is not present in 
the file, SFWBar will let the compositor determine the locations for new windows.

Task Switcher
-------------
Task switcher implements a keyboard shortcut to cycle focus across windows
(i.e. Alt-Tab). The action is triggered upon receiving a change in a bar
hidden_state property or signal SIGUSR1. This can be configured in Sway, via
one of the following bindings: ::

  bindsym Alt-Tab bar hidden_state toggle
  or
  bindsym Alt-Tab exec killall -SIGUSR1 sfwbar

(for non-sway compositors, use SIGUSR1 trigger)

Task switcher is configured in the "switcher" section of the configuration file.
The following parameters are accepted:

interval
      an timeout after the last task switch event after which the selected
      window is activated

labels [true|false]
      display window titles in the task list

icons [true|false]
      display window icons in the task list

cols
      a number of columns in the task list

css
      css code applicable to the switcher grid. 
      You can specify more detailed css code in the main CSS file. Using style
      name #switcher for the task switcher window and the main grid and names
      #switcher_normal and #switcher_active for inactive and active window 
      representations respectively.

Layout
------
Defines the layout of the taskbar. The layout holds a set of widgets. Widgets
can be nested in case of a ``grid`` widget, which acts as a container.

The following widget types are supported:

taskbar
  a special widget displaying a list of all floating windows.
  (requires a compositor supporting wlr-foreign-toplevel protocol or i3 ipc)

pager
  a special widget displaying a list of all workspaces.
  (requires a compositor supporting wlr-foreign-toplevel protocol or i3 ipc)

tray
  a special widget displaying a list of tray icons received via status
  notifier item interface

grid
  a layout grid capable of containing other widgets. You can use these to
  further subdivide each cell of the main grid and arrange items within it.

label
  a label displaying text sourced from an expression. Labels accept pango
  markup to further theme text withing them.

scale
  a progress bar with a progress value specified by an expression

chart
  a chart plotting the value of the expression over time

image
  display an icon or an image from a file. The name of an icon or a file is
  specified by an expression and can change dynamically.

button
  add a clickable button with an icon/image.

Each widget is placed within the parent grid. By default, widgets are placed
next to the previous widget along the "direction" of the grid (left to right
by default). You can specify widget's positions within a grid by using a
property "loc(x,y[,w,h])" with the first two parameters specifying the location
of the widget within the parent grid and the last two parameters specifying the
widget dimensions in grid cells::

  layout "id" {
    label {
    style = "mystyle"
    value = SwapUsed / SwapTotal + "%"
    loc(2,1,1,1)
    }
  }

The optional "id" string of the layout, specifies the bar to populate and can
control positioning of the grid within a bar using syntax of "name:position",
valid positions are start, center and end. This allows placement of some
widgets in the center of the bar. In case of a single bar, the name of a bar
can be omitted, i.e. ":center".
External widgets can be included in layout using the following syntax: ::

  layout {
    include("MyWidget.widget")
  }

The above will include all scanner variables data and widget sub-layout from
file MyWidget.widget

Grid widgets can contain other widgets, these are declared within the grid
definition i.e. ::

  grid {
    css = "* { border: none }"

    label "id" {
      ...
    }
  }

Widgets can optionally have unique id's assigned to them in order to allow
manipulating them in the future.  Widgets can have the following properties:

value 
  an expression specifying the value to display. This can be a static value
  (i.e. ``"string"`` or ``1``) or an expression (i.e.
  ``"Value is:" + $MyString`` or ``2 * MyNumber.val``). See ``expressions``
  section for more detail.
  For ``Label`` widgets value tells text to display.
  For ``Scale`` widgets it specifies a fraction to display.
  For ``Chart`` widgets it specifies a fraction of the next datapoint.
  For ``Image`` widgets and buttons it provides an icon or an image file name.

style 
  a style name for the widget. Styles can be used in CSS to theme widgets.
  Multiple widgets can have the same style. A style name can be used in css
  using gtk+ named widget convention, i.e. ``label#mystyle``. Style property
  can be set to an expression to change styles in response to changes in
  system parameters.

tooltip
  sets a tooltip for a widget. A tooltip can be a static value or an
  expression. In case of the latter, the tooltip will be dynamically
  updated every time it pops up.

interval
  widget update frequency in milliseconds.. 

trigger 
  trigger on which event updates. Triggers are emitted by Client sources
  a widget should not have both an interval and a trigger specified.
  (if both are specified, interval is ignored and trigger is used).

css
  additional css properties for the widget. These properties will only apply to
  the widget in question.

action
  an action to execute upon a button click. Actions can be attached to any
  widget. Multiple actions can be attached to different mouse buttons using
  ``action[n] = <type> <string>`` syntax. For mouse buttons, n = 1,2,3 for
  left, middle and right button respectively. For mouse scroll events, use
  n = 4,5,6,7 for up, down, left and right respectively. If no index is
  specified the action is attached to a left mouse button click. Additionally,
  action[0] will be executed on startup. You can use this action to set
  initial configuration for a widget.  See ``Actions`` section for more
  details on how actions are specified.

``Taskbar`` widget may contain the following options

labels [true|false]
  an indicator whether to display an application title within the taskbar

icons [true|false]
  an indicator whether to display application icons within the taskbar

filter_output [true|false]
  This property is deprecated, please use ``filter`` instead.
  specifies whether taskbar should only list windows present on the same
  output as the taskbar

filter [output|workspace]
  Specifies whether taskbar should only list windows present on the same
  output or workspace as the taskbar itself.

title_width
  set maximum width of an application title in characters

sort [true|false]
  setting of whether taskbar items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.

rows
  a number of rows in a taskbar.

cols
  a number of columns in a taskbar.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

group [true|false]
  if set to true, the taskbar items will be grouped by app_id, the main
  taskbar will contain one item per app_id with an icon and a label set
  to app_id. On over, it will popup a "group taskbar" containing items
  for individual windows.
  You can specify taskbar parameters for the group taskbars using group
  prefix, i.e. ``group cols = 1``. The properties supported for groups 
  are cols, rows, style, css, title_width, labels, icons.
  

``Pager`` widget may contain the following options

preview [true|false]
  specifies whether workspace previews are displayed on mouse hover over
  pager buttons

sort [true|false]
  setting of whether pager items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.

numeric [true|false]
  if true, the workspaces will be sorted as numbers, otherwise they will be
  sorted as strings (defaults to true).

pins
  a list of "pinned" workspaces. These will show up in the pager even if the
  workspace is empty.

rows
  a number of rows in a pager.

cols
  a number of columns in a pager.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

``tray`` widget my contain the following options

rows
  a number of rows in a pager.

cols
  a number of columns in a pager.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

sort [true|false]
  setting of whether tray items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.

PopUp
-----

Popup windows can be defined the same way as layouts. The only difference is
that popup's are not part of a bar and will not be displayed by default.
Instead they are displayed when a PopUp action is invoked on a widget. i.e.: ::

  PopUp "MyPopup" {
    label { value = "test" }
  }

  Layout {
    label {
      value = "click me"
      action = PopUp "MyPopup"
    }
  }

The PopUp action toggles visibility of the popup window. I.e. the first time
it's invoked, the window will pop up and on the second invocation it will pop
down. As a result it should be safe to bind the PopUp to multiple widgets.

Menus
-----

User defined menus can be attached to any widget (see ``action`` widget
property). Menus are defined using a Menu section in the config file.
The example syntax is as following: ::

  menuclear("menu_name")
  menu ("menu_name") {
    item("item1", Exec "command")
    separator
    submenu("sub","mysubmenu") {
      item("item2", SwayCmd "focus next")
    }
  }

Command MenuClear deletes any existing items from a menu.
Each menu has a name used to link the menu to the widget action and a
list of menu items. If a menu with the same name is defined more than
once, the items from subsequence declarations will be appended to the
original menu. If you want to re-define the menu, use MenuClear action
to clear the original menu.

The following menu items are supported:

item
  an actionable menu item. This item has two parameters, the first one
  is a label, the second is an action to execute when the item is activated.
  See ``Actions`` section for more details on supported actions.

separator
  a menu separator. This item has no parameters

submenu
  attach a submenu. The first parameter parameter is a label to display in the
  parent menu, the second optional parameter is a menu name, if a menu name is
  assigned, further items can be added to a submenu as to any other menu.

Actions
-------
Actions can be attached to click and scroll events for any widget or to items
within a menu. Actions can be conditional on a state of a window or a widget
they refer to and some actions may require a parameter. Conditions are specified
in square brackets prior to the action i.e. ``[Minimized]`` and can be inverted
using ``!`` or joined using ``|`` i.e. ``[!Minimized | Focused]``. All
conditions on the list must be satisfied. Supported conditions are: 
``Minimized``, ``Maximized``, ``Focused``, ``FullScreen``, ``IdleInhibit`` and
``UserState``

Actions can be activated upon receipt of a trigger from one of the client type
sources, using TriggerAction top-level keyword. I.e. ::

  TriggerAction "mytrigger", Exec "MyCommand"

Parameters are specified as strings immediately following the relevant action.
I.e. ``Menu "WindowOps"``. Some actions apply to a window, if the action is
attached to taskbar button, the action will be applied to a window referenced
by the button, otherwise, it will apply to the currently focused window. The
following action types are supported:

Config <string>
  Process a snippet of configuration file. This action permits changing the
  bar configuration on the fly

Exec <string>
  execute a shell command

Function [<addr>,]<string>
  Execute a function. Accepts an optional address, to invoke a function on a
  specific widget.

Menu <string>
  open a menu with a given name

MenuClear <string>
  delete a menu with a given name (This is useful if you want to generate
  menus dynamically via PipeRead and would like to delete a previously
  generated menu)

PipeRead <string>
  Process a snippet of configuration sourced from an output of a shell command

SwayCmd <string>
  send a command over Sway IPC

SwayWinCmd <string>
  send a command over Sway IPC applicable to a current window

MpdCmd <string>
  send a command to Music Player Daemon

ClientSend <string>, <string>
  send a string to a client. The string will be written to client's standard
  input for execClient clients or written into a socket for socketClient's.
  The first parameter is the client id, the second is the string to send.

SetMonitor [<bar_name>,]<string>
  move bar to a given monitor. Bar_name string specifies a bar to move.
  monitor name can be prefixed by "static:", i.e. "static:eDP-1", if this
  is set and the specified monitor doesn't exist or gets disconnected, 
  the bar will not jump to another montior, but will be hidden and won't
  reappear until the monitor is reconnected.

SetLayer [<bar_name>,]<string>
  move bar to a specified layer (supported parameters are "top", "bottom",
  "background" and "overlay". 

SetBarSize [<bar_name>,]<string>
  set size of the bar (width for top or bottom bar, height for left or right
  bar). The argument is a string. I.e. "800" for 800 pixels or "50%" for 
  50% of screen size

SetBarID <string>
  specify bar ID to listen on for mode and hidden_state signals. If no
  bar ID is specified, SfwBar will listen to signals on all IDs

SetExclusiveZone [<bar_name>,]<string>
  specify exclusive zone policy for the bar window. Acceptable values are
  "auto", "-1", "0" or positive integers. These have meanings in line with
  exclusive zone setting in the layer shell protocol. Default value is "auto"

SetValue [<widget>,]<string>
  set the value of the widget. This action applies to the widget from which
  the action chain has been invoked. I.e. a widget may popup a menu, which
  in turn will call a function, which executed SetValue, the SetValue will
  still ac upon the widget that popped up the menu. 

SetStyle [<widget>,]<string>
  set style name for a widget

SetTooltip [<widget>,]<string>
  set tooltip text for a widget

IdleInhibit <string>
  set idle inhibitor for a given widget. The string parameters accepted are
  "or" and "off. You can toggle this action by using IconInhibit condition
  in your action. I.e. [!IdleInhibit] IdleInhibit "on"

UserState <string>
  Set boolean user state on a widget. Valid values are "On" or "Off".

Focus
  set window to focused

Close
  close a window

Minimize
  minimize a window (send to scratchpad in sway)

UnMinimize
  unset a minimized state for the window

Maximize
  maximize a window (set fullscreen in sway)

UnMaximize
  unset a maximized state for the window

Functions
---------

Functions are sequences of actions. They are used when multiple actions need
to be execute on a single triggeer. A good example of this functionality is
dynamically constructed menus generated by an external script: ::

  function("fancy_menu") {
    MenuClear "dynamic_menu"
    PipeRead "$HOME/bin/buildmenu.sh"
    Menu "dynamic_menu"
  }

The above example clears a menu, executes a script that builds a menu again
and opens the resulting menu.

Function "SfwBarInit" executed on startup. You can use this functions to set
initial parameters for the bar, such as default monitor and layer.

Scanner
-------
SFWBar widgets display data obtained from various sources. These can be files
or output of commands.

Each source section contains one or more variables that SFWBar will poll
periodically and populate with the data parsed from the source. The sources
and variables linked to them as configured in the section ``scanner`` ::

  scanner {
    file("/proc/swaps",NoGlob) {
      SwapTotal = RegEx("[\t ]([0-9]+)")
      SwapUsed = RegEx("[\t ][0-9]+[\t ]([0-9]+)")
    }
    exec("getweather.sh") {
      $WeatherTemp = Json(".forecast.today.degrees")
    }
  }

Each declaration within the ``scanner`` section specifies a source. This can
be one of the following:

File
        Read data from a file

Exec
        Read data from an output of a shell command

ExecClient
        Read data from an executable, this source will read a burst of data
        using it to populate the variables and emit a trigger event once done.
        This source accepts two parameters, command to execute and an id. The
        id is used to address the socket via ClientSend and to identify a
        trigger emitted upon variable updates.
        USE RESPONSIBLY: If a trigger causes the client to receive new data
        (i.e. by triggering a ClientSend command that in turn triggers response
        from the source, you can end up with an infinite loop.

SocketClient
        Read data from a socket, this source will read a bust of data
        using it to populate the variables and emit a trigger event once done.
        This source accepts two parameters, a socket address and an id. The
        id is used to address the socket via ClientSend and to identify a
        trigger emitted upon variable updates.
        USE RESPONSIBLY: If a trigger causes the client to receive new data
        (i.e. by triggering a ClientSend command that in turn triggers response
        from the source, you can end up with an infinite loop.

MpdClient
        Read data from Music Player Daemon IPC (data is polled whenever MPD
        responds to an 'idle player' event).
        MpdClient emits trigger "mpd"

SwayClient
        Receive updates on Sway state, updates are the json objects sent by
        sway, wrapped into an object with a name of the event i.e.
        ``window: { sway window change object }``
        SwayClient emits trigger "sway"

The file source also accepts further optional arguments specifying how
scanner should handle the source, these can be:

NoGlob    
          specifies that SFWBar shouldn't attempt to expand the pattern in 
          the file name. If this flag is not specified, the file source will
          attempt to read from all files matching a filename pattern.

CheckTime 
          indicates that the program should only update the variables from 
          this file when file modification date/time changes.

``Variables`` are extracted from sources using parsers, currently the following
parsers are supported:

Grab([Aggregator])
  specifies that the data is copied from the file verbatim

RegEx(Pattern[,Aggregator])
  extracts data using a regular expression parser, the variable is assigned
  data from the first capture buffer

Json(Path[,Aggregator])
  extracts data from a json structure. The path starts with a separator
  character, which is followed by a path with elements separated by the
  same character. The path can contain numbers to indicate array indices
  i.e. ``.data.node.1.string`` and key checks to filter arrays, i.e.
  ``.data.node.[key="blah"].value``

Optional aggregators specify how multiple occurrences of numeric data are
treated. The following aggregators are supported:

First
  Variable should be set to the first occurrence of the pattern in the source

Last
  Variable should be set to the last occurrence of the pattern in the source

Sum
  Variable should be set to the sum of all  occurrences of the pattern in the
  source

Product
  Variable should be set to the product of all  occurrences of the pattern in
  the source

For string variables, Sum and Product aggregators are treated as Last.

Global Options
--------------

Theme <string>
  Override a Gtk theme to name specified.

DisownMinimized <boolean>
  Disassociate windows from their workplaces when they are minimized.
  If this option is set, selecting a minimize window will unminimize
  it on the active workplace. If set to False (default), the window 
  will be unminimzied to it's last workplace.
  This option only applies to Sway and Hyprland comositors

EXPRESSIONS
-----------
Values in widgets can contain basic arithmetic and string manipulation
expressions. These allow transformation of data obtained by the scanner before
it is displayed by the widgets.

The numeric operations are:

=========== ==================================================================
Operation   Description
=========== ==================================================================
``+``       addition
``-``       subtraction
``*``       multiplication
``/``       division
``%``       remainder of an integer division
``>``       greater than
``>=``      greater than or equal
``<``       less than
``>=``      less than or equal
``=``       equal
``Val``     convert a string into a number, the argument is a string or a
            string expression to convert.
``If``      conditional: If(condition,expr1,expr2)
``Cached``  get last value from a variable without updating it:
            Cached(identifier)
=========== ==================================================================

The string operations are:

=========== ==================================================================
Operation   Description
=========== ==================================================================
``+``       concatenate strings i.e. ``"'String'+$Var"``.
``Mid``     extract substring i.e. ``Mid($Var,2,5)``
``Extract`` extract a regex pattern i.e.
            ``Extract($Var,'FindThis: (GrabThat)')``
``Str``     convert a number into a string, the first argument is a number (or
            a numeric expression), the second argument is decimal precision.
            If precision is omitted, the number is rounded to the nearest 
            integer.
``Pad``     pad a string to be n characters long, the first parameter is a
            string to pad, the second is the desired number of characters,
            if the number is negative, the string is padded at the end, if
            positive, the string is padded at the front.
``If``      conditional: If(condition,expr1,expr2)
``Lookup``  lookup a numeric value within a list of tuplets, the function call
            is ``Lookup(Value, Threshold1, String1, ..., DefaultString)``. The
            function checks value against a thresholds and returns a String
            associated with the highest threshold matched by the Value. If the
            Value is lower than all thresholds, DefaultString is returned. 
            Thresholds in the function call must be in decreasing order.
``Map``     Match a string within a list of tuplets, the usage is:
            ``Map(Value, Match1,String`,...,DefaultString)``. THe function will
            match Value against all Match strings and will return a
            corresponding String, if none of the Match strings match, the
            function will return DefaultString.
``Cached``  get last value from a variable without updating it:
            Cached(identifier)
=========== ==================================================================

In addition the following query functions are supported

=========== ==================================================================
Function    Description
=========== ==================================================================
Time        get current time as a string, the first optional argument specifies
            the format, the second argument specifies a timezone. Return a
            string
Disk        get disk utilization data. You need to specify a mount point as a
            first argument and data field as a second. The supported data
            fields are "total", "avail", "free", "%avail", "%free" or "%used".
            Returns a number.
ActiveWin   get the title of currently focused window. Returns a string.
=========== ==================================================================

Each numeric variable contains four values

.val
  current value of the variable
.pval
  previous value of the variable
.time
  time elapsed between observing .pval and .val
.age
  time elapsed since variable was last updated
.count
  a number of time the pattern has been matched
  during the last scan

By default, the value of the variable is the value of .val. 
String variables are prefixed with $, i.e. $StringVar
The following string operation are supported. For example: ::

  $MyString + Str((MyValue - MyValue.pval)/MyValue.time),2)

User defined expression macros are supported via top-level ``define``
keyword. I.e. ::
  
  define MyExpr = VarA + VarB * VarC + Val($Complex)
  ...
  value = Str(MyExpr,2)

The above will expand the expression into: ::

  value = Str(VarA + VarB * VarC + Val($Complex),2)

Macro's don't have types, as they perform substitution before the
expression is evaluated.

Intermediate variables can be declared using a toplevel ``set`` keyword
I.e. ::

  set MyExpr = VarA + VarB * VarC + Val($Complex
  ...
  value = Str(MyExpr,2)

In the above example, value of the MyExpr variable will be calculated and
the result will be used in computing the value expression. Intermediate
varibles have type and have all of the fields of a scan variable (i.e. val,
pval, time etc). They can be used the same way as scan variables.

Miscellaneous
=============

If the icon is missing for a specific program in the taskbar or switcher, it
is likely due to an missing icon or application not setting app_id correctly.
You can check app_id's of running programs by running sfwbar -d -g app_id.
if app_id is present, you need to add an icon with the appropriate name to 
your icon theme. If it's blank, you can try mapping it from the program's title
(please note that the title may change during runtime, so matching it can be
tricky). Mapping is supported via top-level ``MapAppId`` keyword. I.e. ::

  MapAppId app_id, pattern

where app_id is the desired app_id and pattern is a regular expression to
match the title against.

If you are using an XWayland app, they usually do not have an `app_id` set. If
an icon is not showing, you can add your icon to the following locations:
1. `$HOME/.icons`
2. One of the directories listed in `$XDG_DATA_DIRS/icons`
3. `/usr/share/pixmaps`
4. Location of the main config file currently in use
5. `$XDG_CONFIG_HOME/sfwbar/`

If an `app_id` is not set, and sway is being used, sfwbar will fallback to
using the `instance` in the `window-properties`.

You can find the `app_id` that is being used with sfwbar by using the
`sfwbar -d -g app_id` command, which will show a list of running applications
if your compositor supports the
wlr-foreign-toplevel protocol (i.e. labwc, wayfire, sway):
```
14:49:25.41 app_id: 'jetbrains-clion', title 'sfwbar – pager.c'
```

Alternatively your desktop environment might have a command to display a list:
- Sway: `swaymsg -t get_tree`
- Hyperland: `hyprctl -j clients`

When using `swaymsg -t get_tree`, with CLion this will show the following: ::

  "window_properties": {
    "class": "jetbrains-clion",
    "instance": "jetbrains-clion",
    "title": "sfwbar – trayitem.c",
    "transient_for": null,
    "window_type": "normal"
  }

So we can put an icon called jetbrains-clion.svg (or other formats, see the
[Arch wiki](https://wiki.archlinux.org/title/desktop_entries#Icons)) for
information about file formats.

CSS Style
=========
SFWBar uses gtk+ widgets and can accept all css properties supported by 
gtk+. SFWBar widgets correspond to gtk+ widgets as following:

============= =============== ===============
SFWBar widget gtk+ widget      css class
============= =============== ===============
label         GtkLabel        label
image         GtkImage        image
button        GtkButton       button
scale         GtkProgressBar  progressbar, trough, progress
============= =============== ===============

Taskbar, Pager, Tray and Switcher use combinations of these widgets and can
be themed using gtk+ nested css convention, 
i.e. ``grid#taskbar button { ... }``
(this example assumes you assigned ``style = taskbar`` to your taskbar
widget).

In addition to standard gtk+ css properties SFWBar implements several
additional properties. These are:

===================== =============
property              description
===================== =============
-GtkWidget-align      specify text alignment for a label, defined as a fraction.
                      (0 = left aligned, 1 = right aligned, 0.5 = centered)
-GtkWidget-direction  specify a direction for a widget.
                      For scale, it's a direction towards which scale grows.
                      For a grid, it's a direction in which a new widget is 
                      position relative to the last placed widget. For a
                      window it's an edge along which the bar is positioned.
                      Possible values [top|bottom|left|right]
-GtkWidget-max-width  Limit maximum width of a widget (in pixels)
-GtkWidget-max-height Limit maximum height of a widget (in pixels)
-GtkWidget-hexpand    specify if a widget should expand horizontally to occupy
                      available space. [true|false]
-GtkWidget-vexpand    as above, for vertical expansion.
-GtkWidget-halign     Horizontally align widget within any free space allocated
                      to it, values supported are: fill, start, end, center and
                      baseline. The last vertically aligns widgets to align
                      text within.
-GtkWidget-valign     Vertically align widget.
-GtkWidget-visible    Control visibility of a widget. If set to false, widget
                      will be hidden.
-ScaleImage-color     Specify a color to repaint an image with. The image will
                      be painted with this color using image's alpha channel as
                      a mask. The color's own alpha value can be used to tint
                      an image.
-ScaleImage-symbolic  Render an image as a symbolic icon. If set to true, the
                      image will be re-colored to the gtk theme foreground
                      color, preserving the image alpha channel. This property
                      is ignored if -ScaleImage-color is specified.
===================== =============

Taskbar and pager buttons are assigned the following styles

===================== =============
style name            description
===================== =============
sfwbar                toplevel bar window
layout                top level layout grid
taskbar_normal        taskbar button for a window
taskbar_active        taskbar button for currently focused window
pager_normal          pager button for a workspace
pager_visible         pager button for a visible workspace
pager_focused         pager button for a currently focused workspace
switcher              switcher window and top level grid
switcher_active       switcher active window representation
switcher_normal       switcher inactive window representation
tray                  tray menus and menu items
tray_active           active tray icon
tray_attention        tray icon requiring user attention
tray_passive          passive tray icon
===================== =============

For example you can style top level grid using ``grid#layout { }``.
