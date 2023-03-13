sfwbar-xkbmap
#############

##############################
Sfwbar xkbcommon layout module
##############################

:Copyright: GPLv3+
:Manual section: 1

Filename: xkbmap.so

Requires: libxkbregistry

SYNOPSIS
========

The XkbMap module provides an interface to convert between keyboard layout
types.

Expression Functions
====================

XkbMap(Name,SourceType,DestType)
--------------------------------

Function XkbMap converts between various name types for an XkbLayout. The
function converts layout name Name from SourceType to DestType, the supported
types are: "description","name","variant" and "brief".

Actions
=======

None

Triggers
========

None
