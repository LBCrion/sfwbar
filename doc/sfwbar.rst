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
**SFWBar** is a taskbar for Sway wayland compositor. SFWBar assists in
handling of floating windows on a sway desktop. It provdes a taskbar, provides
a task switcher, implements a window placement policy and facilitates 
monitoring of system files via a set of widgets linked to data exposed in 
system files. SFWBar can work with any wayland compositor supporting layer 
shell protocol, but window handling functionality relies on i3/sway IPC.

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
SFWBar is configured via a libucl formatted file sfwbar.config, the program
checks users XDG config directory (usually ~/.config/sfwbar/), followed by 
/usr/share/sfwbar for the presence of the config file. Additionally, user can
specify a location of the config file via ``-f`` command line option.
Appearance of the program can be further adjusted via CSS properties, these
are sourced from a file sfwbar.css located in the same directory as the config
file. The location of the css file can be also specifies via ``-c`` option.

The config file consists of the following top level sections sections:

Placement
---------
Placement section enables intelligent placement of new floating windows. If
enabled the program will first attemp to place the window in a location, where
it won't overlap with other windows. If such location doesn't exist, the window
will be placed in a cascading pattern from top-left to bottom-right. Placement
declaration accepts parameters "xcascade" and "ycascade" that specify the
steps in the window cascade. These are specified in percentage of the desktop
dimensions. The cascade placement will start at a location specified by "xorigin"
"yorigin" parameters. I.e.::

  placement {
    xcascade = 5
    ycascade = 5
    xorigin = 5
    yorigin = 5
    children = false
  }

"children" parameter specifies if new windows opened by a program with other
windows already opened should be placed. These are usually dialog windows and
SFWBar won't place them by default. If the placement section is not present in 
the file, SFWBar will let the compositor determine the locations for new windows.

Task Switcher
-------------
Task switcher implements a keyboard shortcut to cycle focus across windows
(i.e. Alt-Tab). The action is triggered upon receiving a change in a bar
hidden_state property. This can be configured in Sway, by the following
binding: ::

  bindsym Alt-Tab bar hidden_state toggle

Task switcher is configured in the "switcher" section of the configuration file.
The following parameters are accepted:

delay
      an timeout after the last task switch event after which the selected
      window is activated

title [true|false]
      display window title in the task list

icon [true|false]
      display window icon in the task list

columns
      a number of columns in the task list

css
      css code applicable to the switcher window and any widgets in it. You can
      also specify css code in the main CSS file. Using style name #switcher for
      the task switcher window and the main grid and names #switcher_normal and 
      #switcher_active for inactive and active window representations respectively.

Scanner
-------
SFWBar displays the data it obtains by reading various files. These
files can be simple text files that SFWBar will read or programs whose 
output SFWBar will parse.

Each file contains one or more variables that SFWBar will poll periodically
and use to update the display. Configuration file section ``scanner`` contains
the information on files and patters to extract: ::

  scanner {
    "/proc/swaps" {
      flags = NoGlob
      SwapTotal.add = "[\t ]([0-9]+)"
      SwapUsed.add = "[\t ][0-9]+[\t ]([0-9]+)"
    }
  }

Each section within the ``scanner`` block starts with a filename and contains
a list of variables and regular expression patterns used to populate them. 
Optionally, a file section may also contain a ``flags`` option, used to modify 
how the file is handled. If the file is declared multiple times, the flags from
the last declaration will be used. The supported flags are:

NoGlob    
          specifies that SFWBar shouldn't attempt to expand the pattern in 
          the file name.

CheckTime 
          indicates that the program should only update the variables from 
          this file when file modification date/time changes.

Exec      
          tells SFWBar to execute this program and parse its output. Please 
          note that SFWBar will execute this program as often as needed to 
          update variables based on its output. This may take up a significant
          part of system resourses. The program is executed synchironouslyr. 
          If it takes a while to execute, SFWBar will freeze while waiting for
          the executed program to finish. In these situatuations, it may be
          better to execute the program periodically via cron and save it's 
          output to a temp file that would in turn be read by SFWBar.

``Variables`` are populated using a regular expression specified to the scanner. The
scanner reads the file looking for the regular expression and populates the 
variable with data from the first capture buffer in the regular expression. If
the name of the variable doesn't contain a dot ``.``, the variable is treated as
a string variable and the scanner copies the data from the capture buffer as is.
If the variable name contains a dot, the scanner treats the variable as a
numeric variable. In this case the text before the dot specifies the variable
name and the text after the dot is the modifier, specifying how multiple 
occurences of the pattern within the file should be handled.

The following modifiers are supported: ``add``, ``replace``, ``product`` and 
``first``. By default, if SFWBar matches the regular expression more than once,
the variable will be set to the value of the last occurence (``replace``). If 
the modifier is set to ``add``, the variable will be set to the sum of all 
matches. Modifier ``product`` will similarly return the product of all values,
while ``first`` will return the first occurence.

Layout
------
Specifies what items are displayed on the taskbar. The layout section contains
a list of widget definitions. These can be nested in case of a ``grid`` widget,
which can be used as a container.  ::

  layout {
    MyLabel {
    type = label
    style = mystyle
    value = "SwapUsed/SwapTotal+'%'"
    x = 2, y = 1, w = 1, h = 1
    }
  }

External widgets can be included in layout using the following syntax: ::

  layout {
    widget1 = MyWidget.widget
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

grid
  a layout grid used to fine tune placement of widgets. You can use these to
  further subdivide each cell of the main grid and arrange items therein.

label
  a label displaying text (either static or sourced from scan variables).

scale
  a progress bar with a progress value sourced from a scan variable

image
  display an image from a file specified in "value"

button
  add a clickable button with an option to launch external programs on click

You can also include files containing "scanner" and "layout" section by adding
a layout element in the form of ``id = "filename.config"``.

Each widget is placed within the parent grid. By default, widgets are placed next
to the previous widget along the "direction" of the grid (left to right by default).
You can specify widget's  positions within a grid by using properties "x" and "y"
and size of the widget within the grid using properties "w" and "h".

Widgets can have the following properties:

value 
  an expression specifying the value to display. This can be a static value
  i.e. "'string'" or "1" or an expression, i.e. "'Value is:'+$MyString" or 
  "MyNumber.val" (see ``expressions`` section for more detail)

style 
  assign a style to the widget. Styles can be used in CSS to theme widgets.
  Multiple widgets can have the same style. A style name can be used in css
  using gtk+ named widget convention, i.e. ``label#mystyle``

freq
  specify update frequency in milliseconds 

css
  specify additional css properties for the widget. These propertes will
  only applyy for the widget in question.

children
  Add children to widget. Applicable to grid widget only. Syntax is the same
  as for the main "layout".

``Button`` widget may contain the following options

action
  An action to execute upon a button click. Applicable to buttons only.

icon
  An icon to display within the button

``Taskbar`` widget may contain the following options

title [true|false]
  An indicator whether to display an application title within the taskbar.

icon [true|false]
  An indicator whether to display application icons within the taskbar.

rows
  Specify number of rows in a taskbar.

cols
  Specify number of columns in a taskbar.
  If both rows and cols are specified, rows will be used. If neither is
  specified, the default is rows=1

``Pager`` widget may contain the following options

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
Extract     extract a regex pattern i.e.  ``Extract($Var,'FindThis: (GrabThat)')``
Time        get current time as a string, you can specify a timezone as an
            optional argument.
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
-GtkWidget-icon-size  specify icon size for a taskbar or button.
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

