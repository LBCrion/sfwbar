sfwbar-bsdctl
#############

########################
Sfwbar BSD sysctl module
########################

:Copyright: GPLv3+
:Manual section: 1

Filename: bsdctl.so

Requires: BSD libc

SYNOPSIS
========

The BSDCtl module provides an interface to query BSD sysctl values

Expression Functions
====================

BSDCtl(Query)
-------------

function BSDCtl queries the value of a sysctl variable. It's equivalent to
calling sysctl program on BSD system. Please note that you can only query
individual nodes rather than groups.

Actions
=======

None

Triggers
========

None
