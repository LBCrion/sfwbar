sfwbar-idle
###########

###########################
Sfwbar Idle notifier module
###########################

:Copyright: GPLv3+
:Manual section: 1

Filename: idle.so

Requires: none

SYNOPSIS
========

The idle notifier module, provides support for idle triggers, i.e. triggers
that are emitted if the user has been idle for a specified period of time.

Expression Functions
====================

None.

Actions
=======

IdleTimeout Trigger, Timeout
-----------------------------

Create a new trigger with a name Trigger that will be emitted after Timeout
seconds of idleness. I.e. `IdleTimeout "timeout1", "10"`. A trigger can be
removed by invoking this action again with a Timeout value of zero.

Triggers
========

There are no default triggers. User defined triggers are defined using the
`IdleTimeout` action.
