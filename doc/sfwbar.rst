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
assists in handling of floating windows on a wayland desktop. It provdes a
taskbar, a pager and a task switcher, a tray (using a status notifier item
protocol), implements a window placement policy and facilitates display of
data extracted from ssytem files using a set of widgets.
SFWBar can work with any wayland compositor supporting layer shell protocol,
but some functionality relies on presence of i3/sway IPC.
Taskbar and switcher require either sway or wlr-foreign-toplevel protocol
support. Placer and  pager require sway.

USAGE
=====
SFWBar executable can be invoked with the following options:

-f | --config
  Specify a filename of a configuration file

-c | --css
  Specify a filename of a css file

-s | --socket
  Specify a location of sway ipc socket

CONFIGURATION
=============
SFWBar is configured via a config file  usually  sfwbar.config), the program
checks users XDG config directory (usually ~/.config/sfwbar/), followed by 
/usr/share/sfwbar for the presence of the config file. Additionally, user can
specify a location of the config file via ``-f`` command line option.
Appearance of the program can be further adjusted via CSS properties, these
are sourced either from a css section of the main configuration file or
from a file sfwbar.css located in the same directory as the config
file. The location of the css file can be also specifies via ``-c`` option.

The config file consists of the following top level sections sections:

Placer
---------
Placer section enables intelligent placement of new floating windows. If
enabled the program will first attemp to place the window in a location, where
it won't overlap with other windows. If such location doesn't exist, the window
will be placed in a cascading pattern from top-left to bottom-right. The Placer
declaration accepts parameters "xstep" and "ystep" that specify the
steps in the window cascade. These are specified in percentage of the desktop
dimensions. The cascade placement will start at a location specified by "xorigin"
"yorigin" parameters. I.e.::

  placement {
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
      css code applicable to the switcher window and any widgets in it. You can
      also specify css code in the main CSS file. Using style name #switcher for
      the task switcher window and the main grid and names #switcher_normal and 
      #switcher_active for inactive and active window representations respectively.

Scanner
-------
SFWBar displays the data it reads from various sources. These can be files or
output of commands.

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
be either ``file`` or ``exec`` specifying whether to read a file or output of
a command respectively. The first parameter of a ``file`` source specifies a
file to read, for an ``exec`` source, this specifies command to run.
The file source also accepts further optional argumens specifying how
scanner should handle the source, these can be  :

NoGlob    
          specifies that SFWBar shouldn't attempt to expand the pattern in 
          the file name. If this flag is not specified, the file source will
          attempt to read from all files matching a filename pattern.

CheckTime 
          indicates that the program should only update the variables from 
          this file when file modification date/time changes.

``Variables`` are extracted from a file using parsers, currently the following
parsers are supported:

Grab([Aggregator])
  specifies that the data is copied from the file verbatim

RegEx(Pattern[,Aggregator])
  extracts data using a regular expression parser, the variable is assigned
  data from the first capture buffer

Json(Path[,Aggregator])
  extracts data from a json structure. The path starts with a separator
  character, which is followed by a path with elements separated by the
  same character. The path can contain numbers to indicate array indices.
  I.e. ".data.node.1.string".

Optional aggregators specify how multiple occurences of numeric data are treated.
The following aggregators are supported:

First
  Variable should be set to the first occurence of the pattern in the source

Last
  Variable should be set to the last occurence of the pattern in the source

Sum
  Variable should be set to the sum of all  occurences of the pattern in the
  source

Product
  Variable should be set to the product of all  occurences of the pattern in the
  source

For string variables, Sum and Product aggregators are treated as Last.

Layout
------
Defines the layout of the taskbar. The layout section contains a tree of
widgets. Widgets can be nested in case of a ``grid`` widget,
which can be used as a container.  ::

  layout {
    label {
    style = "mystyle"
    value = SwapUsed / SwapTotal + "%"
    loc(2,1,1,1)
    }
  }

External widgets can be included in layout using the following syntax: ::

  layout {
    include("MyWidget.widget")
  }

The above will include all scanner data and widget sub-layout from file
MyWidget.widget

The following widget types are supported:

taskbar
  a special widget displaying a list of all floating windows.
  (requires a compositor supporting i3 ipc)

pager
  a special widget displaying a list of all workspaces.
  (requires a compositor supporting i3 ipc)

tray
  a special widget displaying a list of tray icons received via
  status notifier item interface

grid
  a layout grid used to fine tune placement of widgets. You can use these to
  further subdivide each cell of the main grid and arrange items therein.

label
  a label displaying text (either static or sourced from scan variables).

scale
  a progress bar with a progress value sourced from a scan variable

image
  display an image from a file specified in "value" ( the image displayed can
  change as the value changes)

button
  add a clickable button with an option to launch external programs on click

Each widget is placed within the parent grid. By default, widgets are placed
next to the previous widget along the "direction" of the grid (left to right
by default). You can specify widget's  positions within a grid by using a
property "loc(x,y[,w,h])" with the first two parameters specifying the location
of the widget in the parent grid and the last two parameters specifying the
widget dimensions in grid cells.

In a grid widgets, child widget declarations can be placed immediately following
the parent grid properties. i.e. ::

  grid {
    css = "* { border: none }"

    label {
      ...
    }
  }

Widgets can have the following properties:

value 
  an expression specifying the value to display. This can be a static value
  i.e. "'string'" or "1" or an expression, i.e. "Value is: "+$MyString" or 
  2 * MyNumber.val (see ``expressions`` section for more detail), For labels
  value specifies text to display. Scale widgets accept a fraction to
  display. Images and buttons accept an icon or an image file name.

style 
  assign a style to the widget. Styles can be used in CSS to theme widgets.
  Multiple widgets can have the same style. A style name can be used in css
  using gtk+ named widget convention, i.e. ``label#mystyle``

interval
  specify update frequency in milliseconds 

css
  specify additional css properties for the widget. These propertes will
  only applyy for the widget in question.

action
  An action to execute upon a button click. Applicable to buttons only.

``Taskbar`` widget may contain the following options

labels [true|false]
  An indicator whether to display an application title within the taskbar.

icons [true|false]
  An indicator whether to display application icons within the taskbar.

rows
  Specify number of rows in a taskbar.

cols
  Specify number of columns in a taskbar.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

``Pager`` widget may contain the following options

preview [true|false]
  Specifies whether workspace previews are displayed on mouse hover over
  pager buttons

pins
  List "pinned" workspaces. These will show up in the pager even if the 
  workspace is empty.

rows
  Specify number of rows in a pager.

cols
  Specify number of columns in a pager.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

EXPRESSIONS
===========
Values in widgets can contain basic arithmetic and string manipulation
expressions. For numeric variables, the following operators are supported:
``+``, ``-``, ``*``, ``/``. Furthermore any numeric value can be converted
to a string using a specified rounding convention with a function ``Str``,
i.e. ``Str(MyValue.val,2)``. 

Each numeric variable contains four values

.val
  current value of the variable
.pval
  previous value of the variable
.time
  time elapsed between observing .pval and .val
.count
  a number of time the pattern has been matched
  during the last scan

By default, the value of the variable is the value of .val

String variables are prefixed with $, i.e. $StringVar
The following string operation are supported:

=========== ==================================================================
Operation   Description
=========== ==================================================================
+           concatenate strings i.e. ``"'String'+$Var"``.
Mid         extract substring i.e. ``Mid($Var,2,5)``
Extract     extract a regex pattern i.e.
            ``Extract($Var,'FindThis: (GrabThat)')``
Time        get current time as a string, the first optional parameter specifies
            the format, the second argument specifies a timezone
Df          get disk utilization data. You need to specify a mount point as an
            argument.
=========== ==================================================================

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

Taskbar and Pager use combinations of these widgets and can be themed
using gtk+ nested css convention, i.e. ``grid#taskbar button { ... }``
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
                      position relative to the last placed widget.
                      For a window it's an edge along which the bar is positioned.
                      Possible values [top|bottom|left|right]
-GtkWidget-hexpand    specify if a widget should expand horizontally to occupy
                      available space. [true|false]
-GtkWidget-vexpand    as above, for vertical expansion.
===================== =============

Taskbar and pager buttons are assigned the following styles

===================== =============
style name            description
===================== =============
layout                Top level layout grid
taskbar_normal        taskbar button for a window
taskbar_active        taskbar button for currently focused window
pager_normal          pager button for a workspace
pager_visible         pager button for a visible workspace
pager_focused         pager button for a curently focused workspace
switcher              switcher window and top level grid
switcher_active       switcher active window representation
switcher_normal       switcher inactive window representation
===================== =============

For example you can style top level grid using ``grid#layout { }``.

