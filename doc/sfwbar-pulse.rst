sfwbar-pulse
############

########################
Sfwbar PulseAudio module
########################

:Copyright: GPLv3+
:Manual section: 1

Filename: pulsectl.so

Requires: libpulse

SYNOPSIS
========

The PulseAudio module provides an interface to query and adjust the settings of
the PulseAudio server. 

Expression Functions
====================

Pulse(Query[,Interface])
------------------------

function Pulse queries the state of the Pulse server, the Query parameter
specifies the information to query, the optional Interface parameter
specifies the interface to which the query applies. If the Interface isn't
given the query is applied to the current sink or the current source. The
supported query types are:

"sink-volume"
  the volume of a sink.
"sink-mute"
  muted state of the sink.
"sink-icon"
  sink icon.
"sink-form"
  the form of the sink.
"sink-port"
  the port name of the sink.
"source-volume"
  the volume of a source.
"source-mute"
  muted state of the source.
"source-icon"
  source icon.
"source-form"
  the form of the source.
"source-port"
  the port name of the source.

Actions
=======

PulseCmd [Interface,],Command
-----------------------------

Manipulate the state of the interface, if the interface isn't specified, the
Command will be applied to the current interface.

"sink-volume +/-X"
  Adjust the volume of the sink by X%.
"sink-mute State"
  Change the state of the sink, State can be On, Off or Toggle.
"source-volume +/-X"
  Adjust the volume of the source by X%.
"source-mute State"
  Change the state of the source, State can be On, Off or Toggle.

Triggers
========

The module defines one trigger "Pulse" which is emitted whenever the state of
the pulse server changes.
