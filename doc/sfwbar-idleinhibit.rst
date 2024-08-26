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

Expression Functions
====================

IdleInhibitState()
------------------------

Query an idle inhibitor state on a calling widget. It return a string with
possible values of "On", if an idle inhibitor is set on the widget or "Off"
if it isn't. If the function is called from an expression which isn't attached
to a widget, the returned value will be "Off".

Actions
=======

SetIdleInhibitor Command
-----------------------------

Set idle inhibitor state for a widget. The possible command values are:

"On"
  turn on an idle inhibitor
"Off" 
  turn off an idle inhibitor
"Toggle" 
  toggle the state of an idle inhibitor

Triggers
========

The module defines one trigger "IdleInhibitor" which is emitted whenever the
state of any idle inhibitor changes.
