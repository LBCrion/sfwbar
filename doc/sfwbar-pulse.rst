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
specifies the interface to which the query applies. The interface contains
either a device name or a device name and a channel separated by a colon.
If the Interface isn't given the query is applied to the current sink or
the current source. The supported query types are:

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
"sink-monitor"
  name of the source monitor for the sink.
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
"source-monitor"
  name of the sink monitor for the source.

PulseChannel("Query")
---------------------

Query device and channel information upon receipt of `pulse_channel` or
`pulse_removed` event. The valid `Query` values are:

"Device"
  name of a device advertised by `pulse_channel` event.
"Channel"
  name of a channel advertised by `pulse_channel` event.
"ChannelNumber"
  ordinal of a channel advertised by `pulse_channel` event.
"RemovedDevice"
  name of a removed device advertised by `pulse_removed` event.

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

PulseAck
--------

Notify the module that all information for the currently advertized
device/channel combination has been processed. The module may then emit another
`pulse_channel` event if further channel updates are available.

PulseAckRemoved
---------------

Notify the module that all information for the currently advertized device
removal has been processed. The module may then emit another `pulse_removed`
event if further device removals are queued.

Triggers
========

pulse
  a signal emitted whenever the state of the pulse server changes.
pulse_channel
  emitted when a device/channel combination information is available.
pulse_removed
  emitted when a device is removed.
