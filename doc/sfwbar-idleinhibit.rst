sfwbar-idleinhibit
##################

###########################
Sfwbar IdleInhibitor module
###########################

:Copyright: GPLv3+
:Manual section: 1

Filename: idleinhibit.so

Requires: none

SYNOPSIS
========

The IdleInhibit module, allows attaching idle inhibitor to any widget in the
taskbar. If an idle inhibitor is active on a visible widget, it will prevent
the compositor to going into an idle state (i.e. blanking a screen,
going into a suspend mode or activating a screensaver)

Triggers
========

The module defines trigger `IdleInhibitor` which is emitted whenever the
state of any idle inhibitor changes.

Functions
=========

IdleInhibitState([id:string])
-----------------------------

Query an idle inhibitor state of a widget specified by `id`. If omitted, the
state of a widget calling the function will be returned. Returns a string
containing a value `On` or `Off`.

SetIdleInhibitor([id:string,] state:string)
-------------------------------------------

Set idle inhibitor state for a widget. If parameter `id` is specified, the
function will set inhibitor state for a specified widget, otherwise it will be
set for the current widget.  The possible `state` values are:

"On"
  turn on an idle inhibitor
"Off" 
  turn off an idle inhibitor
"Toggle" 
  toggle the state of an idle inhibitor
