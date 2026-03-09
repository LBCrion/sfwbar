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

Triggers
========

The idle module defines trigger `resumed`. This trigger is emitted on the first
user interaction after a timeout. Timeout triggers are defined using the
`IdleTimeout` action.

Functions
=========

IdleTimeout(Trigger, Timeout)
-----------------------------

Create a new trigger with a name `Trigger` that will be emitted after `Timeout`
seconds of idleness. I.e. `IdleTimeout("timeout1", 10)`. A trigger can be
removed by calling this function again with a `Timeout` value of zero.
