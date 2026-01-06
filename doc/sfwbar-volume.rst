sfwbar-volume
#############

###############################
Sfwbar volume control interface
###############################

:Copyright: GPLv3+
:Manual section: 1

Filename: alsactl.so
Filename: pulsectl.so

Requires: libpulse (pulsectl.so)
Requires: libasound (alsactl.so)

SYNOPSIS
========

The volume control interface provides means to query and adjust the volume
settings.

Triggers
========

"volume"
  A signal emitted upon change of the state of the sound server.

"volume-conf"
  Emitted when a new or updaded volume control element information is
  available. Functions handling this trigger can access the following
  variables:
  `device_id` - an id of a device.
  `interface` - an interface of the device ("source", "sink" or "client").
  `channels` - an array of channel information, the first column is channel
  indices, the second column is channel id's.

"volume-conf-removed"
  emitted when a volume control element is removed. Functions handling this
  trigger can access variable `device_id` specifying the id of a removed
  device.

Functions
=========

Volume(<Property>[,<ID>])
-------------------------

Function Volume queries the volume settings. The `Property` parameter
specifies the information to query, the optional `ID` parameter
specifies the volume control element to which the query applies.
The ids are server specific. Pulse address is specified in a format
`Device[:Channel]`. Pulse also supports an internal address format
`@pulse-interface-index`, this format is used for elements supplied by the
`volume-conf` trigger and shouldn't be used directly.  ALSA interface is
specified as `Device[:Element][:Channel]`.

This function returns a <numberic> value.

The supported queries are:

"sink-volume"
  the volume of a sink.
"sink-mute"
  muted state of a sink.
"sink-count"
  a number of sinks currently available.
"sink-is-default"
  query whether the sink is the default sink.
"source-volume"
  the volume of a source.
"source-mute"
  muted state of a source.
"source-count"
  a number of sources currently available.
"source-is-default"
  query whether the source is the default source.
"client-volume"
  the volume of a client.
"client-mute"
  muted state of a client.
"client-count"
  a number of clients currently available.

VolumeInfo(Query[,ID])
----------------------

Returns additional information about a volume control element. The
Query parameter specified the information to get. The Address parameter
specifies the address to query. 

This function returns a string value.

"sink-description"
  a description of the sink.
"sink-icon"
  sink icon.
"sink-form"
  the form of the sink.
"sink-port"
  the port name of the sink.
"sink-monitor"
  name of the source monitor for the sink.
"source-description"
  a description of the source.
"source-icon"
  source icon.
"source-form"
  the form of the source.
"source-port"
  the port name of the source.
"source-monitor"
  name of the sink monitor for the source.
"client-description"
  a description of the client.

VolumeCtl([ID,], <Command>)
---------------------------

Manipulate the state of the volume control. if the address isn't specified,
the command will be applied to the default device for the interface.

"sink-volume +/-X"
  Adjust the volume of a sink by X%. If the value is prefixed by neither
  a '+' or a '-', the volume will be set to value X.
"sink-mute State"
  Change the state of a sink, State can be On, Off or Toggle.
"sink-set-default"
  Set a sink as a default sink.
"source-volume +/-X"
  Adjust the volume of a source by X%. If the value is prefixed by neither
  a '+' or a '-', the volume will be set to value X.
"source-mute State"
  Change the state of a source, State can be On, Off or Toggle.
"source-set-default"
  Set a source as a default source.
"client-volume +/-X"
  Adjust the volume of a client by X%. If the value is prefixed by neither
  a '+' or a '-', the volume will be set to value X.
"client-mute State"
  Change the state of a client, State can be On, Off or Toggle.
"client-set-default"
  Set a client as a default client.
"client-set-sink"
  Redirect a client to a specified sink.

VolumeAck Type
--------------

Notify the module that all information for the currently advertised element
been processed. The Type corresponds to a trigger event ack'ed, i.e.
`volume-conf` or `volume-conf-removed`.  The module may then emit another
conf event if further updates are available.

