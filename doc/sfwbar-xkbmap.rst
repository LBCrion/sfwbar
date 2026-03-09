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

The XkbMap module provides an interface to convert between xkb keyboard layout
types.

Functions
=========

XkbMap(Name, SourceType, DestType)
----------------------------------

Convert between various name types for an XkbLayout. The function converts
layout `Name` from `SourceType` to `DestType`. The supported types are:
`"description"`, `"name"`, `"variant"` and `"brief"`.

Triggers
========

None
