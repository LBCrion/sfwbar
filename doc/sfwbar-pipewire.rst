sfwbar-pipewire
###############

######################
Sfwbar Pipewire module
######################

:Copyright: GPLv3+
:Manual section: 1

Filename: pipewire.so

Requires: none

SYNOPSIS
========

Pipewire module exposes basic information about pipewire connections.
Currently, the support is limited to counting various connection types to
allow implementation of the privacy notifier.

Triggers
========

The module defines trigger `pipewire` which is emitted whenever a pipewire
connection is created or destroyed.

Functions
=========

PipewireCOunt([type:string])
-----------------------------

Request the count of pipewire connections of a given type. The supported
types are:

`AudioOut`
  Audio output connections
`AudioIn`
  Audio input connections
`VideoIn` 
  Video input connections
