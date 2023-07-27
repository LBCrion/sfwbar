SFWBar
######

###############################
SFWBar - Widget Wizardry Manual
###############################

Sfwbar provides necessary features to develop reasonably sophisticated and
efficient user defined widgets. There are four main components involved in
developing a widget:

1. Scanner sources - to facilitate gathering of the information to display
1. Expression engine - to manipulate the data sourced by the scanner
1. Layout widgets - to display the data and results of computations
1. Actions - to manipulate the state

WIDGET EFFICIENCY
=================

The original rationale for development of the Sfwbar's predecessor was
facilitation of efficient widget construction. A common approach to status bar
widget implementation involves periodic execution of a shell script which
reads data from output of commands or system files, parses the contents,
performs necessary calculations or text manipulations on the data and outputs
the result in a format useable by the widget.

The above approach can be very inefficient for information requiring high
frequency of updates. The status bar executes a shell, which loads a script,
which may in turn execute a number of other executables to source and
manipulate the data. The outcome is a multitude of fork's and image loads,
which are executed on a regular basis. This ultimately requires a trade-off
between the frequency of updates and the system load.

Sfwbar provides basic functionality that allows polling for data, extracting
information from this data and manipulating it into output useable by status
bar widgets.

BASIC STEPS
===========

These are the basic steps involved in creating a widget:

GET DATA
--------

Data can obtained from variety of sources. On a typical linux system, a lot
of system information is exposed via /proc and /sys filesystems. On BSD systems
this information would be available via a sysctl call. Some systems expose and
update variety of information via sockets or have client utilities that can
provide asynchronous status updates via stdout. As a last resort, we can fall
back to periodic execution of a program that would output the information we
need, although in some cases we can improve the efficientcy of this approach
by using a status change notification from one of the more efficient methods.

EXTRACT DATAPOINTS
------------------

The data we obtain from various sources isn't always presented in a directly
useable format. We may need to parse these data dumps to extract the data
points we need. Sfwbar allows extracting iinfromation the data dump using
regular expression or json parsers. I.e. if the /proc file contains several
strings of text, sfwbar can identify a string matching a pattern, extract a
segment of this string, using a regular expression capture buffer and save this
outout in an internal variable. Sfwbar can extract multiple patterns from the
same data dump and populate multiple variables.

MANIPULATE INFORMATION
----------------------

Once infromation is parsed, we can manipulate it into a format we need for
display. It can be something simple, such as rounding a number for decimal
precision or a complex requiring flow control, mathematical calculations and
text manipulation (i.e. calculating color gradients and generating an svg
image on the fly).

VISUALIZE OUTPUT
----------------

Once we have the data in the format we need, we need to create a visual
representation to be presented to a user. This can be a simple text string
displayed on the status bar, an icon with a name derived from the data we
gathered, an image we generated on the fly, a scale showing a fraction or
a chart showing evolution of the data point over time. 

CONSTRUCTING A SIMPLE WIDGET
============================

Here's an example of a simple widget: ::
 
  scanner {
    file("/proc/meminfo") {
      MemTotal = RegEx("^MemTotal:[\t ]*([0-9]+)[\t ]")
      MemFree = RegEx("^MemFree:[\t ]*([0-9]+)[\t ]")
      MemCache = RegEx( "^Cached:[\t ]*([0-9]+)[\t ]")
      MemBuff = Regex("^Buffers:[\t ]*([0-9]+)[\t ]")
    }
  }
  
  layout {
    label {
      interval = 500
      value = Str((MemTotal-MemFree-MemCache-MemBuff)/MemTotal*100) + "%"
    }
  }

This widget periodically reads file ``/proc/meminfo``, extracts numeric
values for MemTotal, MemFree, Cached and Buffers lines, computes a percentage
of memory in use, formats it into a percentage string and displays it as a
label.

An important performance question to consider is how often will the file be
read. The scanner will process a file whenever an expression accesses the
variables defined in that file. In the example above, the label is updated on
an interval of 500 milliseconds. This would trigger a read of ``/proc/meminfo``
twice a second. All parsing of the file and manipulations of data are performed
internall by Sfwbar, so this shouldn't be too onerous on most systems. It's
worth noting that the label itself will only be updated if the result of the
expression changes. Since the expression rounds the memory utilization figure
to a nearest percentage point, the label will only change if the memory
utilization changes by one percent.

USING TRIGGERS
==============

Using periodic updates is a reasonable approach when the information the widget
needs is only available on request (i.e. we can query memory or CPU utilization
at any time to get the most up to date information. For a lot of other
datapoints, it may be more efficient to listen for specific events and update
the data when these events occur. This is implemented via triggers. These come
from various sources. I.e. modules, compositor IPC's, sockets, RT signals or
executables that produce output on specific events.

Triggers can be used to initiate widget updates or to execute specific actions.

