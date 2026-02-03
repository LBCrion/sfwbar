SFWBar
######

##########################
S* Floating Window taskBar
##########################

test
:Copyright: GPLv3+
:Manual section: 1

SYNOPSIS
========
| **sfwbar** [options]

DESCRIPTION
===========
**SFWBar** is a taskbar for wayland compositors. It works with wayland
compositors supporting layer shell protocol. Some features rely on wlr
protocols or custom compositor IPCs.

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
SFWBar reads configuration from a file (sfwbar.config by default). The
program checks users XDG config directory (usually ~/.config/sfwbar/) for this
file, followed by system xdg data directories. Additionally, user can specify
a location and a name of the configuration file using ``-f`` command line option.

Appearance of the program is controlled using CSS properties. These
are sourced either from the css section of the main configuration file or
from a file with a .css extension with the same base name and the same location
as the configuration file. The name of the css file can be also specified using
``-c`` option.

Please note that both ``-f`` and ``-c`` options require full path to the file.

Additional configuration files can be included from the main configuration file
using the `include` directive i.e. `include "file.widget"`.

Sfwbar can be restarted and configuration reloaded by sending a SIGHUP signal
to the sfwbar process (i.e. `killall -SIGHUP sfwbar`).

The configuration files described in this document are using API V2 format. To
distinguish between old and new format, the configuration files should begin
with `#Api2` tag. If the file is missing this tag, SFWBar assumes that the file
uses an old configuration format.

Toplevel keywords
-----------------

Theme <string>
  Override a GTK theme to name specified.

IconTheme <string>
  Override a GTK icon theme.

TriggerAction <trigger>, <action>
  Execute an action when a trigger is emitted. Trigger is a string, an
  action is any valid action, as described in the Actions section.

Define <identifier> = <expression>
  Define an expression macro. Once declared, an <identifier> used in any
  expression will be expanded into an <expression>.

bar
---

The layout of the bar is specified in one of the parent structures, these are
`bar`, `popup` or `widget_grid`. Each one is a nested structure consisting of
widgets and their properties. i.e.::

  bar "sfwbar" {
    button {
      value = "icon-name";
      style = "launcher";
      action[LeftClick] = Exec("firefox");
    }
    grid {
      label "mylabel" {
        value = "Some text";
      }
    }
  }

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
  markup to further theme text within them.

scale
  a progress bar with a progress value specified by an expression

chart
  a chart plotting the value of the expression over time

image
  display an icon or an image from a file. The name of an icon or a file is
  specified by an expression and can change dynamically.

button
  add a clickable button with an icon/image.

entry
  display a text entry widget. This widget should not be added within popup
  windows as these are not guaranteed to be able to receive keyboard input.
  The current text from an entry can be obtained using `EntryText` function.

any
  special type used to address existing widgets, this can't be used to declare
  a new widget, only to modify an existing widget.

Each widget is placed within the parent grid. By default, widgets are placed
next to the previous widget along the "direction" of the grid (left to right
by default). You can specify widget's positions within a grid by using a
property "loc(x,y[,w,h])" with the first two parameters specifying the location
of the widget within the parent grid and the last two parameters specifying the
widget dimensions in grid cells::

 bar "id" {
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
External widgets can be included using the following syntax: ::

  bar {
    widget "MyWidget.widget"
  }

The above will process the contents of configuration file `MyWidget.widget` and
place the `widget_grid` object from the included file into the `bar`.

Grid widgets can contain other widgets, these are declared within the grid
definition i.e. ::

  grid {
    css = "* { border: none }"

    label "id" {
      ...
    }
  }

Widgets can optionally have unique id's assigned to them in order to allow
manipulating them in the future.

Properties define the appearance and behaviour of widgets. These are generally
defined as `property = value` with a few exceptions.
All widgets can have the following properties:

value = <expression>
  an expression specifying the value to display. This can be a static value
  (i.e. ``"string"`` or ``1``) or an expression (i.e.
  ``"Value is:" + $MyString`` or ``2 * MyNumber.val``). See ``expressions``
  section for more detail.
  For ``Label`` widgets value specifies text to display.
  For ``Scale`` widgets it specifies a fraction to display.
  For ``Chart`` widgets it specifies a fraction of the next data point.
  For ``Image`` and ``Button`` widgets and buttons it provides an icon or an
  image file name.

style = <expression>
  a style name for the widget. Styles can be used in CSS to theme widgets.
  Multiple widgets can have the same style. A style name can be used in css
  using GTK+ named widget convention, i.e. ``label#mystyle``. Style property
  can be set to an expression to change styles in response to changes in
  system parameters.

tooltip = <expression>
  sets a tooltip for a widget. A tooltip can be a static value or an
  expression. In case of the latter, the tooltip will be dynamically
  updated every time it pops up.

interval = <number>
  widget update frequency in milliseconds.

trigger = <string>
  a trigger event that should cause the widget to update. Triggers are emitted
  by a variety of sources (i.e. modules, compositor events, data available in
  from a client connection etc.).
  (if trigger is specified, the interval property is ignored).

css = <string>
  additional css properties for the widget. These properties will only apply to
  the widget in question. You can have multiple instances of the css property
  in a single widget definition and they all will be applied in the order of
  their occurrence. css property value can only be a static string, not an
  expression.

action
  an action to execute upon interaction with a widget. Actions can be attached
  to any widget. Multiple actions can be attached to various pointer events.
  The notation is ``action[<event>] = <action>``.  Event values are
  LeftClick, MiddleClick or RightClick, DoubleClick, ScrollUp, ScrollDown,
  ScrollLeft, ScrollRight and Drag respectively.
  Please note that if you add a DoubleClick event to a widget, the LeftClick
  event will still be emitted for each click in addition to the DoubleClick
  event.
  Additionally, modifiers can be specified using the notation of
  ``[Modifier+]<event>``. I.e. ``action[Ctrl+LeftClick]``. The following
  modifiers supported: Shift, Ctrl, Mod1, Mod2, Mod3, Mod4, Mod5, Super, Hyper,
  and Meta. Multiple modifiers can be added, i.e.
  ``action[Ctrl+Shift+ScrollUp]``. action[0] will be executed on startup. You
  can use this action to set initial configuration for a widget.  See
  ``Actions`` section for more details on how actions are specified.

disable = [true|false]
  can be sued to disable a widget without commenting out the entire section.
  I.e. setting `disable = true;` will discard the widget definition.

``Taskbar`` widget may contain the following options

icons = [true|false]
  an indicator whether to display application icons within the taskbar

labels = [true|false]
  an indicator whether to display an application title within the taskbar

title_width = <number>
  set maximum width of an application title in characters

filter = [floating|minimized|output|workspace]
  controls which windows are shown in the switcher.
  `floating` will only show flowing windows.
  `minimized` will filter out minimized windows.
  `output` will only show windows from the current display.
  `workspace` will only show window from the current workspace.

sort = [true|false]
  setting of whether taskbar items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.
  Items are sorted by default, set this to false to enable drag-and-drop.

rows = <number>
  a number of rows in a taskbar.

cols = <number>
  a number of columns in a taskbar.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

group = [popup|pager|false]
  if set to true, the taskbar items will be grouped. Supported groupings
  are: popup and pager. In a popup grouping windows are grouped by app_id,
  the main taskbar will contain one item per app_id with an icon and a
  label set to app_id. On over, it will popup a "group taskbar" containing
  items for individual windows.
  In a pager grouping mode, the taskbar is partitioned into workspaces and
  each workspace contains windows belonging to it. Dragging windows from
  one workspace to another moves it to a destination workspace. (currently
  this is only supported with sway and hyprland compositors, support for
  other compositors requires adoption of new wayland protocols).
  You can specify taskbar parameters for the group taskbars using group
  prefix, i.e. ``group cols = 1``. The properties supported for groups 
  are cols, rows, style, css, title_width, labels, icons.

``pager`` widget may contain the following options

preview = [true|false]
  specifies whether workspace previews are displayed on mouse hover over
  pager buttons

sort = [true|false]
  setting of whether pager items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.
  Items are sorted by default, set this to false to enable drag-and-drop.

primary_axis = [rows|columns]
  specifies a primary axis for sorting items, i.e. will the next item be placed
  to the right or below it's sibling.

pins = <string list>
  a list of "pinned" workspaces. These will show up in the pager even if the
  workspace is empty. I.e. ``pins = "1", "2", "3", "4";``

rows = <number>
  a number of rows in a pager.

cols = <number>
  a number of columns in a pager.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

``tray`` widget may contain the following options

rows = <number>
  a number of rows in a pager.

cols = <number>
  a number of columns in a pager.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

sort = [true|false]
  setting of whether tray items should be sorted. If the items are not
  sorted, user can sort them manually via drag-and-drop mechanism.
  Items are sorted by default, set this to false to enable drag-and-drop.

primary_axis = [rows|columns]
  specifies a primary axis for sorting items, i.e. will the next item be placed
  to the right or below it's sibling.

``bar`` objects may have the following options

edge = <direction>
  Specifies the edge against which the bar should be positioned. The valid
  values are `top`, `left`, `right`, `bottom`;

size = <number|string>
  set size of the bar (width for top or bottom bar, height for left or right
  bar). The argument is a number, specifying the size in pixels or a string.
  I.e. "800" for 800 pixels or "50%" for 50% of screen size

halign = <alignment>
  specified horizontal alignment of the bar if the bar occupies less than 100%
  of the monitor. The valid values are `start`, `center`, `end`;

valign = <alignment>
  specified vertical alignment of the bar if the bar occupies less than 100%
  of the monitor. The valid values are `start`, `center`, `end`;

sensor = <number>
  Specifies the interval after the pointer leaves the bar before the bar is
  hidden (autohide mode). Once hidden, the bar will popup again if the pointer
  touches the sensor located along the screen edge along which the bar is
  placed.  A numeric value specifies the bar pop-down delay in milliseconds.
  If the timeout is zero, the bar will always be visible.

sensor_delay = <number>
  Specifies the interval after the pointer enters the bar sensor area and the
  hidden bar pops back up. This property is ignore if the `sensor` property is
  not specified.

transition = <number>
  Specifies the transition period (in milliseconds) for bar appearance
  animation.

monitor = <string>
  assign bar to a given monitor. The  monitor name can be prefixed by
  "static:", i.e. "static:eDP-1". if this is set and the specified monitor
  doesn't exist or gets disconnected, the bar will not jump to another monitor,
  but will be hidden and won't reappear until the monitor is reconnected.

mirror = <string>
  mirror the bar to monitors matching any of the specified patterns.  The
  string parameter specifies a string list of patters to match the monitors
  against, i.e. `"eDP-*", "HDMI-1"` will mirror to any monitor with name
  starting with "eDP-" or monitor named "HDMI-1". Patterns starting with '!'
  will block the bar from being mirrored to a matching monitor. The patterns
  are specified in glob style '*' and '?' are used as wildcards. The simplest
  use is `mirror = "*"`, which will mirror the bar across all monitors.

layer = <layer>
  move bar to a specified layer (supported parameters are `top`, `bottom`,
  `background` and `overlay`.

margin = <string>
  set margin around the bar to the number of pixels specified by string.

exclusive_zone = <string>
  specify exclusive zone policy for the bar window. Acceptable values are
  "auto", "-1", "0" or positive integers. These have meanings in line with
  exclusive zone setting in the layer shell protocol. Default value is "auto"

bar_id = <string>
  specify bar ID to listen on for mode and hidden_state signals. If no
  bar ID is specified, SfwBar will listen to signals on all IDs

PopUps and Windows
------------------

Popup windows and toplevel windows can be defined the same way as bars.
The only difference is that they are not part of a bar and will not be
displayed by default.  Instead they are displayed when an action is invoked
on a widget. i.e.: ::

  popup "MyPopup" {
    label { value = "test"; }
  }

  bar {
    label {
      value = "click me";
      action = PopUp("MyPopup");
    }
  }

The `PopUp` action toggles visibility of the popup window. I.e. the first time
it's invoked, the window will pop up and on the second invocation it will pop
down. As a result it should be safe to bind the PopUp to multiple widgets.

``popup`` window may contain the following options

AutoClose [true|false]
  specify whether the popup window should close if user clicks anywhere outside
  of the window.

Toplevel windows can be defined using a `window` element and are controlled
using `WindowOpen` and `WindowClose` actions. I.e.::

  window "mywindow" {
    button {
      value = "close-icon";
      action = WindowClose("mywindow");
    }
  }

  bar {
    button {
     value = "open-icon";
     action = WindowOpen("mywindow");
    }
  }

unlike popups, toplevel windows can't be placed in a specific location,
compositors can place them anywhere on the desktop. They can receive focus
and are useful for implementing dialogs.

Menus
-----

User defined menus can be creating using a `menu` structure. The format is
similar to the `bar`, but widgets and properties differ. For example: ::

  menu "menu_name" {
    item {
      value = "item1";
      tooltip = "the first item";
      action = Exec("command");
    }
    separator;
    item {
      value = "sub";
      menu "mysubmenu" {
        item {
          value = "item2";
          action = SwayCmd("focus next");
        }
      }
    }
  }

  bar {
    ...
    button {
      value = "menu-icon";
      action = Menu("menu_name");
    }
  }

Each menu has a name used to link the menu to the widget action and a
list of menu items. If a menu with the same name is defined more than
once.
The following menu items are supported:

item
  A menu item. If the item contains a `menu` widget inside it, it will be
  presented as a submenu, otherwise the item will have invoke an `action` upon
  activation if an `action` is defined.

separator
  A separator item. This item does not accept any properties.

Menu structure supports one property:

sort = [true|false]
  if set to true, the menu items will be sorted with the menu. The items are
  sorted using `index` as the primary sort key and item `value` as a secondary
  sort key.

Menu items contain the following properties:

value = <expression>
  a value to be displayed in the menu item, this will change if the result of
  the expression changes.

icon = <string>
  an icon to be displayed next to the item text.

tooltip = <expression>
  a value to be displayed in the tooltip when pointer hovers over the item.

desktopid = <string>
  populate a menu item from a desktop entry file. If any other properties are
  specified for the item, they will override the data extracted from the desktop
  entry file.

action = <action>
  an action to execute if the item is activated.

index = <number>
  a sort index associated with an item. If a menu has a `sort` property set to
  true, the items will be sorted using this index as a primary sort key.

The config file consists of the following top level sections:

Placer
------
Placer structure controls intelligent placement of new floating windows. This
functionality currently relies on side channel IPCs and is not supported for
all compositors. If placer is enabled, SFWBar will first attempt to place a new
floating window in a location, where it won't overlap with other windows. If
such location doesn't exist, the window will be placed in a cascading pattern
from top-left to bottom-right.

The `placer` structure supports the following properties:

children
  place child windows on screen (child windows are windows sharing a pid with
  existing windows).

xorigin = <number>
  a horizontal position (as a percentage of a desktop size) of the first window
  in a cascade.

yorigin = <number>
  a vertical position (as a percentage of a desktop size) of the first window
  in a cascade.

xstep = <number>
  a horizontal step (as a percentage of desktop size) of the window cascade.

ystep = <number>
  a vertical step (as a percentage of desktop size) of the window cascade.

I.e.::

  placer {
    xorigin = 5
    yorigin = 5
    xstep = 5
    ystep = 5
    children = false
  }

Task Switcher
-------------
Task switcher cycles the focus across windows (i.e. Alt-Tab function). Switcher
can be invoked through a `SwitcherEvent` action. The forward switch action is
bound to `SIGUSR1` signal by default,  in `sway`, the action is additionally
bound to a change in a bar hidden_state property.

In sway, you can bind alt-tab using `bindsym Alt-Tab bar hidden_state toggle`
In other compositors, you can bind a key to `killall -SIGUSR1 sfwbar` (you may
need to replace `sfwbar` with the name of the sfwbar executable if it differs
from the default on your system).

Task switcher is configured in the "switcher" section of the configuration file.
The following parameters are accepted:

interval = <number>
  an timeout after the last task switch event after which the selected window
  is activated.

filter = [floating|minimized|output|workspace]
  controls which windows are shown in the switcher.
  `floating` will only show flowing windows.
  `minimized` will filter out minimized windows.
  `output` will only show windows from the current display.
  `workspace` will only show window from the current workspace.

icons = [true|false]
  display window icons in the task list.

labels = [true|false]
  display window titles in the task list.

title_width = <number>
  controls the width of the label (in character).

row = <number>
  a number of rows in the task list

cols = <number>
  a number of columns in the task list
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

sort = [true|false]
  controls whether the items in the switcher should be sorted.

primary_axis = [rows|columns]
  specifies a primary axis for sorting items, i.e. will the next item be placed
  to the right or below it's sibling.

css = <string>
  css code applicable to the switcher grid. This property can only be set to a
  static string, not an expression. You can specify more detailed css code in
  the main CSS file. Using style name `#switcher` for the task switcher window
  and the main grid and names `#switcher_item` for window representations.

Triggers
--------
Triggers are emitted in response to various events, such as compositor state
changes, real time signals or notifications from modules. Some triggers can
be defined as part of the configuration (i.e. SocketClient or ExecClient
scanner sources), others are built in, or defined in modules and user actions.

Built-in triggers are:

===================== =========================================================
SIGRTMIN+X            RT signal SIGRTMIN+X has been received (X is a number)
sway                  Data has been received on SwayClient scanner source
mpd                   Data has been received on MpdClient scanner source
<output>-connected    an output has been connected (i.e. eDP-1-connected)
<output>-disconnected an output has been disconnected
===================== =========================================================

Actions
-------
SFWBar will execute actions in response to certain events. These can be user
input events such as clicking or scroll a mouse over a widget or system events,
such as realtime signals, data arriving via a pipe etc.

To bind an action to user input events, use widgets `action` property. Or for
system events, you can bind an action to a trigger, using `TriggerAction`
keyword. I.e.::

  TriggerAction "mytrigger", Exec("MyCommand")

An action can be a single instruction, i.e. `Exec("firefox");` or a sequence of
instructions enclosed in curly brackets.

An instruction can be a function call using syntax::

  [<variable> = ] my_func ( [<expression>, ... ] );

You can use variables within action `{ }` blocks. Variables are declared using
a `Var` keyword::

  Var <identifier> [ = <expression> ];

Conditional operations can be implemented using `If` keyword::

  If <expression>
    <instruction>
  [else
    <instruction>]

Loops can be implemented using `While` keyword::

  While <expression>
    <instruction>

Functions can be terminated early and return a value using a `Return` keyword::

  Return [<expression>];

For more complex actions, you can define your own functions using a toplevel
`function` keyword. I.e.::

  Function my_func ( x ) {
    Var y = "This is a test " + Str(x);
    Print(y);
    Return x+1;
  }
  TriggerAction "sometrigger", my_func(1);

By default, all actions, functions and expressions share a single namespace, so
any variable is visible to all code instances. Private scopes can be defined
using a `Private` keyword. I.e. ::

  Private {
    Var X = 1;

    export label "mylabel" {
      value = Str(x);
    }
  }

In the above example variable `X` can only be accessible within the scope. You
can access it when addressing label "mylabel" outside of the private block as
the label will inherit the scope it's defined in.

Function "SfwBarInit" is executed on startup. Use it set initial parameters for
the bar, modules etc.

Expressions
-----------
As part of the configuration SFWBar can evaluate expressions. These can be part of
an action or user defined function, but some properties also accept expressions.
In case of the later, the expression is evaluated periodically or in response to
to a trigger (see `interval` and `trigger` widget properties).

A value in an expression can have one of four types: numeric, string, array or
n/a.

Expressions support the following operators:
============ =========================================================================================
Operation    Description
============ =========================================================================================
``+``        addition of numeric values or concatenation of strings, append value operator for arrays.
``-``        subtraction of numeric values.
``*``        multiplication of numeric values.
``/``        division of numeric values.
``%``        remainder of an integer division for numeric values.
``>``        greater than comparison of numeric values.
``>=``       greater than or equal comparison of numeric values.
``<``        less than comparison of numeric values.
``<=``       less than or equal comparison of numeric values.
``=``        equality comparison of two values, returns false if types differ.
``If``       conditional: If(<condition>, <expression>, <expression>)
``Cached``   get last value from a scanner variable without updating it, i.e. `Cached(identifier)`.
``Ident``    Check if an identifier exists either as a variable or a function.
============ =========================================================================================

Expressions can include function calls, i.e.::

  Var my_var = 1 + my_func(2);

Arrays can be declared and array elements can be accessed using `[ ]` operator::

  Var my_array = [1,2,3];
  Var my_var = my_array[2];

Array indices start at 0.

Native functions
----------------
Actions and functions can call user definer or built-in (native) functions, the
following is the list of the functions provided by SFWBar. Modules can expose
their own functions which are documented separately.

SFWBar supports the following native functions:

Config(<string>)
  Process a snippet of configuration file. This action permits changing the bar
  configuration on the fly. Returns n/a.

PipeRead(<string>)
  Process a snippet of configuration sourced from an output of a shell command.
  This function can be used to update SFWBar configuration from a script.
  Returns n/a.

Exec(<string>)
  Execute a shell command. Returns n/a.

Print(<string>)
  Print a string to standard output. Useful for debugging user functions.
  Returns n/a.

USleep(<numeric>)
  Sleep for duration specified in microseconds. Actions and expressions are
  executed in separate threads. USleep will block the relevant thread only.
  Returns n/a.

Exit()
  Terminate SFWBar.

EmitTrigger(<string>)
  Emit a trigger event. The string parameter specifies the name of a trigger.
  Returns n/a.

FileTrigger(<file:string>, <trigger:string>[, <timeout:numeric>])
  Setup a file monitor. Upon any changes to the file, a trigger will be
  emitted. If the timeout is specified, the trigger will be emitted at an
  interval specified by timeout value (in microseconds) until the first
  file monitor event is detected (this is useful for /sys files where
  monitoring may not be effective. Returns n/a.

ClientSend(<id:string>, <string>)
  send a string to a client. The string will be written to client's standard
  input for execClient or to a socket for socketClient. The first parameter is
  the client id, the second is the string to send. Returns n/a.

Eval(<string>, <string>)
  update a value of an intermediate scanner variable with a result of an
  expression. The first parameter is the name of the intermediate variable,
  the second parameter is the expression. Returns n/a.

PopUp(<string>)
  open a popup window. The popup will be attached to a widget executing the
  action. Returns n/a.

Menu(<string>)
  open a menu with a specified name. The menu will be attached to the widget
  executing the action. Returns n/a.

MenuClear(<string>)
  delete a menu with a given name (This is useful if you want to generate
  menus dynamically via PipeRead and would like to delete a previously
  generated menu). Returns n/a.

MenuItemClear(<string>)
  delete a menu item with a given id. The menu item must be declared with an id
  if you want to modify or clear it. Returns n/a.

ClearWidget(<string>)
  delete a widget with a given id. A widget must be declared with id if you want
  to modify or delete it. Returns n/a.

UpdateWidget()
  Triggers an update of a widget invoking the action. Returns n/a.

MapAppId(app_id:string>, <pattern:string>)
  add a fallback title to app_id mapping. If an application is missing an
  application id, SFWBar can assign one based on a title of an application,
  please note that title of an application can vary, so mapping may not be
  stable. The `pattern` parameter specifies a regular expression pattern to
  match titles against. `app_id` parameter specifies the application id to
  assign. Returns n/a.

MapIcon(<app_id:string>, <icon:string>)
  use icon <icon> for applications with app id <app_id>. Both parameters are
  strings. Returns n/a.

FilterAppId(<pattern:string>)
  Any windows with appids matching a regular expression pattern will not be
  shown on the taskbar or switcher. Returns n/a.

FilterTitle(<pattern:string>)
  Any windows with titles matching a regular expression pattern will not be
  shown on the taskbar or switcher. Returns n/a.

DisownMinimized(<boolean>)
  Disassociate windows from their workplaces when they are minimized. If this
  option is set, selecting a minimized window will unminimize it on the active
  workplace. If set to False (default), the window will be unminimzied to it's
  last workplace. This option requires custom IPC support. Returns n/a.

SwitcherEvent(<string>)
  trigger a switcher event, this action will bring up the switcher window and
  cycle the focus either forward or back based on the argument. The string
  argument can be either "forward" or "back". If the argument is omitted, the
  focus will cycle forward.

SetValue([<widget:string>,]<string>)
  set the value of the widget. This action applies to the widget from which
  the action chain has been invoked. I.e. a widget may popup a menu, which
  in turn will call a function, which executed SetValue, the SetValue will
  still ac upon the widget that popped up the menu. 

SetStyle([<widget:string>,]<string>)
  set style name for a widget

SetTooltip([<widget:string>,]<value:string>)
  set tooltip text for a widget

UserState([<widget:string>,]<string>)
  Set boolean user state on a widget. If widget parameter isn't specified, the
  state will be set for a widget invoking the action. Valid values are "On" or
  "Off". Returns n/a.

Focus()
  set window to focused. This action can only be invoked from a taskbar item
  widget. Returns n/a.

Close()
  close a window. This action can only be invoked from a taskbar item
  widget. Returns n/a.

Minimize()
  minimize a window. This action can only be invoked from a taskbar item
  widget. Returns n/a.

UnMinimize()
  unset a minimized state for the window. This action can only be invoked from
  a taskbar item widget. Returns n/a.

Maximize()
  maximize a window. This action can only be invoked from a taskbar item
  widget. Returns n/a.

UnMaximize()
  unset a maximized state for the window. This action can only be invoked from
  a taskbar item widget. Returns n/a.

SetLayout(<string>)
  Switches current keyboard layout. The string parameter can have values "next"
  or "prev" for next or previous layout respectively. Returns n/a.

MpdCmd(<string>)
  send a command to Music Player Daemon client. Returns n/a.

SwayCmd <string>
  Send a command over Sway IPC. Returns n/a.

SwayWinCmd <string>
  Send a command over Sway IPC applicable to a current window, Returns n/a.

Str(<value>, <number>)
  Convert a value to string.If converting a number, the second parameter
  controls decimal precision. Returns <string>.

Val(<string)
  Convert a string to a number. Returns <numeric>

Min(<number>, <number>)
  Return a smaller of the two numbers.

Max(<number>, <number>)
  Return a larger of the two numbers.

Mid(<string>, <numeric>, <numeric>)
  Extract a substring from a string, the first parameter is the string to extract
  the substring from, second and third parameters are the first and last
  characters of the substring. Returns <string>.

Extract(<string>, <pattern:string>)
  Extract a substring using a regular expression. The function will return
  contents of the first capture buffer in the regular expression specified
  by <pattern>. Returns <string>.

Pad(<string>, <length:number>[, <string>])
  Pad the string to a desired length. The first parameter is the string to pad.
  The second is the desired length, the third optional parameter is a character
  to pad with (defaults to space). Returns <string>.

Upper(<string>)
  Convert a string to uppercase. Returns <string>.

Lower(<string>)
  Convert a string to lowercase. Returns <string>.

Escape(<string>)
  Escapes quotes and other special characters in a string making it suitable to
  be included as a substring within double quotes. Returns <string>.

Replace(<string>, <old:string>, <new:string>)
  Replaces an `old` substring with a `new` substring within a string. Returns
  <string>.

ReplaceAll(<string>, <old:string>, <new:string>, ... )
  Perform multiple substitutions within a string. Identical to calling `Replace`
  multiple times. Further parameters must be supplied in pairs of `old` and
  `new` substrings. Returns <string>.

Map(<string>, <match:string>, <result:string>, ..., <default:string>)
  Looks for a `string` in a list of `match` strings and returns a corresponding
  `result`. Further parameters must be supplied in pairs of `match` and
  `result`. If the string doesn't match any `match`'es, returns `default`.
  Returns <string>.

ArrayMap(<string>, <match:array>, <result:array> [, <default:string>)
  Looks up a `string` in a `match` array. If a match is found returns a
  corresponding element of a `result` array. If no match is found and `result`
  array is longer than a `match` array, returns an extra (default) element of
  a `result` array, otherwise returns a `default` string. Returns <string>.

Lookup(<number>, <threshold:number>, <result:string>, ..., <default:string>)
  Looks up a `number` against a list of `threshold`s. Returns a `result` string
  corresponding to a first `threshold` smaller than the `number`. This means
  `thresholds` should be sorted in a descending order. If all `threshold`s are
  greater than the `number`, returns `default` string. Returns <string>.

ArrayLookup(<number>, <threshold:array>, <result:array> [, <default:string>])
  Looks up a `number` in a `threshold` array and returns a `result` with an
  index corresponding to a first element of a `threshold` array smaller than a
  `number`. If all `threshold` elements are greater than the `number`, and a
  `result` array is longer than a `threshold` array, returns an extra (default)
  element of a `result` array, otherwise returns a `default` string.
  Returns <string>.

ArraySize(<array>)
  Returns the size of the array. Valid indices for the array will be
  0 to size-1. Returns <number>.

ArrayBuild(<any>, ... )
  Concatenate values into an array. Equivalent to  [<any>, ...].
  Returns <array>.

ArrayConcat(<array>, <array>)
  Concatenates two arrays. Equivalent to a `+` operator on two arrays.
  Returns <array>.

ArrayIndex(<array>, <index:number>)
  Get a value of an item in the array specified by the `index`. Return value
  is value dependent.

ArrayAssign(<array>, <index:number>, <value>)
  Assigns a value to an index within the array, if the index is out of bounds,
  the array will be resized. This is equivalent to `array[index] = value`.

Read(<string>)
  Reads the contents of a file and returns them as a string. Returns <string>.

ls(<string>)
  Retrieves a list of files in a directory specified by the parameter.
  Returns <array>.

TestFile(<string>)
  Check if the file exists and is readable by the SFWBar process.
  Returns <number>.

GT(<string>)
  Returns a translation of a string corresponding to a current locale. If
  translation is not available, returns the string. Returns <string>.

Layout()
  Returns the current keyboard layout. Returns <string>.

GetLocale()
  Returns current locale. Returns <string>.

Time(<format:string> [, <tz:string>])
  Returns current time in a format specified by a `format` string. If a `tz`
  argument is supplied, returns time corresponding to a supplied time zone.
  Returns <string>.

Disk(<fs:string>, <info:string>)
  queries disk information for a disk. `fs` specifies a mount point to query.
  `info` specifies desired information. Available options are:
  `total` - total space on disk in bytes.
  `avail` - available space on disk.
  `free` - free space on disk.
  `%avail` - available fraction of space on disk.
  `%used` - used fraction of space on disk.
  Returns <number>.

ActiveWin()
  Returns a tile of the currently focused window. Returns <string>.

WindowInfo([<id:string>,] <query:string>)
  Queries information about a window. Optional parameter `id` specifies the
  the widget id of a taskbar item corresponding to a window to query. If omitted
  the widget calling the function is used. `query` parameter specifies the data
  to query. Valid values are:
  "appid" - application id of a window. Returns <string>.
  "title" - title of a window. Returns <string>.
  "minimized" - minimized state of a window. Returns <number>.
  "maximized" - maximized state of a window. Returns <number>.
  "fullscreen" - fullscreen state of a window. Returns <number>.
  "focused" - focused state of a window. Returns <number>.

WidgetId()
  Returns an ID of a widget invoking the action. Returns <string>.

WidgetSetData([<id:string>], <name:string>, <value>)
  Attach a named value to a widget. The optional parameter `id` specifies an
  id of a widget, if omitted, the operation will apply to the current widget.
  A value will be attached to a widget property `name` and can be later
  retrieved using WIdgetGetData. Returns <n/a>.

WidgetGetData([<id:string>], <name:string>)
  Retrieves a named value from a widget.  The optional parameter `id` specifies
  an id of a widget, if omitted, the operation will apply to the current widget.
  This function returns a value previously attached to proeprty `name` of a
  widget.

WidgetState([<id:string>,] <stateid:number>)
  Returns a value of one of two widget `state` boolean. The optional parameter
  `id` specifies an id of a widget to query. If omitted, the state of a widget
  calling the expression will be returned. The `stateid` parameter specifies
  which state variable to query (valid values are 1 or 2). Returns <number>.

WidgetChildren([<id:string>])
  Returns a list of child widgets within a widget. The optional parameter `id`
  specifies an id of a widget to query. If omitted, the state of a widget
  calling the expression will be returned. Returns <array>.

BarDir()
  Returns a direction of a bar containing the current widget. Returns <string>.

GtkEvent(<axis:string>)
  Returns position of a GTK+ event triggering the execution of the current
  action. I.e. location of a click within the widget. The `axis` parameter
  specifies which axis to query. Possible values are "x" for horizontal,
  "y for vertical or "dir" to use the direction property of a widget. The
  returned value is a fraction of a size of a widget. Returns <number>.

EntryText(<id:string>)
  Retrieve the current text in an entry widget identified by `id`.

CustomIPC()
  Returns a name of a custom IPC currently in use (if any). Returns <string>.

InterfaceProvider(<interface:string>)
  Returns a name of a module currently handling the specified interface.
  Returns <string>.

Scanner
-------
Bar often require polling data from system files (i.e. /sys or /proc). To this
end, SFWBar provides a scanner infrastructure. Scanners allow reading system
files and extract multiple data points from them in a single pass. This ensures
that multiple data items are consistent and resources are not wasted reading the
same file multiple times.::


  File("/proc/swaps",NoGlob) {
    SwapTotal = RegEx("[\t ]([0-9]+)")
    SwapUsed = RegEx("[\t ][0-9]+[\t ]([0-9]+)")
  }
  Exec("getweather.sh") {
    WeatherTemp = Json(".forecast.today.degrees")
  }
  ExecClient("stdbuf -oL foo.sh BAR BAZ", "foo") {
    Foo_foo = Json(".foo")
    Foo_bar = Json(".bar")
  }

Scanner declarations consist of a scanner source and one or more parsers used to
populate the scanner variables.

The sources are:

File(<name>, <flags>)
        Read data from a file

Exec(<command>)
        Read data from an output of a shell command

ExecClient(<command> [,<trigger>)
        Read data from an executable, this source will wait for any output from
        the standard output of the executable. Once available (i.e. the program
        flushes its output) the source will populate the variables and emit a
        trigger event.  This source accepts two parameters, command to execute
        and an id. The id can be used to write to the standard input of the 
        executable via ClientSend (provided that the executable takes standard
        input) and to identify a trigger emitted upon variable updates.
        USE RESPONSIBLY: If a trigger causes the client to receive new data
        (i.e. by triggering a ClientSend command that in turn triggers response
        from the source, you can end up with an infinite loop.
        (see alsa.widget and rfkill-wifi.widget as examples).

SocketClient(<address> [,<trigger>)
        Read data from a socket, this source will read a bust of data
        using it to populate the variables and emit a trigger event once done.
        This source accepts two parameters, a socket address and an id. The
        id is used to address the socket via ClientSend and to identify a
        trigger emitted upon variable updates.
        USE RESPONSIBLY: If a trigger causes the client to receive new data
        (i.e. by triggering a ClientSend command that in turn triggers response
        from the source, you can end up with an infinite loop.

MpdClient(<address> [,<trigger>)
        Read data from Music Player Daemon IPC (data is polled whenever MPD
        responds to an 'idle player' event).  MpdClient emits trigger "mpd".
        (see mpd-int.widget as an example)

SwayClient(<command> [,<trigger>)
        Receive updates on Sway state, updates are the json objects sent by
        sway, wrapped into an object with a name of the event i.e.
        ``window: { sway window change object }``.
        SwayClient emits trigger "sway".
        (see sway-lang.widget as an example).


The `File` source also accepts further optional arguments specifying how
scanner should handle the source, these can be:

NoGlob
          specifies that SFWBar shouldn't attempt to expand the pattern in 
          the file name. If this flag is not specified, the file source will
          attempt to read from all files matching a filename pattern.

CheckTime
          indicates that the program should only update the variables from 
          this file when file modification date/time changes.

Scanner variables are extracted from sources using parsers, currently the following
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
  Variable should be set to the sum of all occurrences of the pattern in the
  source

Product
  Variable should be set to the product of all occurrences of the pattern in
  the source

For string values, Sum and Product aggregators are treated as Last.
Each scanner variable holds the following information:

.val
  current numeric value of the variable
.pval
  previous value of the variable
.time
  time elapsed between observing .pval and .val
.age
  time elapsed since variable was last updated
.count
  a number of time the pattern has been matched
  during the last scan
.str
  a string value of the variable (can also be accessed by using $ prefix).

If a suffix is omitted for a scanner variable, the .val suffix is assumed.

Intermediate scanner variables can be declared using a toplevel ``set`` keyword
I.e. ::

  set MyExpr = VarA + VarB * VarC + Val($Complex
  ...
  value = Str(MyExpr,2)

In the above example, value of the MyExpr variable will be calculated and
the result will be used in computing the value expression. Intermediate
variables have type and have all of the fields of a scan variable (i.e. val,
pval, time etc). They can be used the same way as scan variables.

Mapping icons
=============

If the icon is missing for a specific program in the taskbar or switcher, it
is likely due to an missing icon or application not setting app_id correctly.
You can check app_id's of running programs by running `sfwbar -d -g app_id`,
this should produce output like below: ::

  08:08:28,69 [DEBUG] app_id: 'firefox', title 'Mozilla Firefox'

Alternatively your desktop environment might have a command to display a list:
- Sway: `swaymsg -t get_tree`
- Hyperland: `hyprctl -j clients`

If an application id is present, you need to add an icon with the appropriate
name to your icon theme. To do this this, copy a file with a name matching appid
into one of the following directories:

1. `$HOME/.icons`
2. One of the directories listed in `$XDG_DATA_DIRS/icons`
3. `/usr/share/pixmaps`
4. Location of the main config file currently in use
5. `$XDG_CONFIG_HOME/sfwbar/`

In the above example, you can put an icon called `firefox.svg` (you can also use
.png or .xpm) into one of the above directories.

If application id is blank, you can try mapping it from the program's title
(please note that the title may change during runtime, so matching it can be
tricky). Mapping is supported via function ``MapAppId``. You can add a function
call to your `SfwbarInit` function to run it on startup, I.e. ::

  MapAppId("firefox", ".*Mozilla Firefox");

If an `app_id` is not set, and sway is being used, sfwbar will fallback to
using the `instance` in the `window-properties`.

CSS Styling
===========
SFWBar uses GTK+ widgets and can accept all css properties supported by GTK+.
SFWBar widgets correspond to GTK+ widgets as following:

============= =============== ===============
SFWBar widget GTK+ widget      css class
============= =============== ===============
label         GtkLabel        label
image         GtkImage        image
button        GtkButton       button
scale         GtkProgressBar  progress bar, trough, progress
============= =============== ===============

Taskbar, Pager, Tray and Switcher use combinations of these widgets and can
be themed using GTK+ nested css convention,
i.e. ``grid#mytaskbar button { ... }``
(this example assumes you assigned ``style = "mytaskbar"`` to your taskbar
widget).

In addition to standard GTK+ css properties SFWBar implements several
additional properties. These are:

=========================== =============
property                    description
=========================== =============
-GtkWidget-align            specify text alignment for a label, defined as a
                            fraction.  (i.e. 0 = left aligned, 1 = right
                            aligned, 0.5 = centered)
-GtkWidget-hexpand          specify if a widget should expand horizontally to
                            occupy available space. [true|false]
-GtkWidget-vexpand          as above, for vertical expansion.
-GtkWidget-halign           Horizontally align widget within any free space
                            allocated to it, values supported are: fill, start,
                            end, center and baseline. The last vertically
                            aligns widgets to align text within.
-GtkWidget-valign           Vertically align widget.
-GtkWidget-visible          Control visibility of a widget. If set to false,
                            widget will be hidden.
-GtkWidget-max-width        Limit maximum width of a widget (in pixels)
-GtkWidget-max-height       Limit maximum height of a widget (in pixels)
-GtkWidget-ellipsize        specify whether a text in a label should be
                            ellipsized if it's too long to fit in allocated
                            space.
-GtkWidget-wrap             wrap a string if it's too long for it's container
                            (you would usually want to pair it with
                            -GtkWidget-max-width)
-GtkWidget-direction        specify a direction for a widget.  For scale, it's
                            a direction towards which scale grows.  For a grid,
                            it's a direction in which a new widget is position
                            relative to the last placed widget. For a window
                            it's an edge along which the bar is positioned.
                            Possible values [top|bottom|left|right]
-ScaleImage-color           Specify a color to repaint an image with. The image
                            will be painted with this color using image's alpha
                            channel as a mask. The color's own alpha value can
                            be used to tint the image.
-ScaleImage-symbolic        Render an image as a symbolic icon. If set to true,
                            the image will be re-colored to the gtk theme
                            foreground color, preserving the image alpha
                            channel. This property is ignored if
                            -ScaleImage-color is specified.
-ScaleImage-shadow-radius   specify a radius for a drop shadow of an image
                            widget. A drop shadow is rendered if a radius or
                            one of the offsets is specified for an image.
                            (an integer specifying a number of pixels).
-ScaleImage-shadow-x-offset a horizontal offset of a drop shadow relative to an
                            image. (an integer specifying a number of pixels).
-ScaleImage-shadow-y-offset a vertical offset of a drop shadow relative to an
                            image. (an integer specifying a number of pixels).
-ScaleImage-shadow-clip     a boolean specifying whether a shadow is clipped to
                            a padding box. If false, the shadow may spill over
                            a border and a margin of a widget. (default = true)
-ScaleImage-shadow-color    a color of a drop shadow.
=========================== =============

Taskbar and pager buttons are assigned the following styles

===================== =============
style name            description
===================== =============
sfwbar                toplevel bar window
layout                top level layout grid
taskbar_item          taskbar button for a window (supports class .active)
takbar_popup          taskbar popup button (supports class .active)
taskbar_pager         taskbar pager grid (supports class .active)
pager_item            pager button for a workspace (supports classes .focused and .visible)
switcher_item         switcher window and top level grid (supports class .active)
tray                  tray menus and menu items
tray_item             tray item icon (supports classes .passive and .attention)
menu_item             menu items (each contains an image and a label)
===================== =============

For example you can style top level grid using ``grid#layout { }``.
