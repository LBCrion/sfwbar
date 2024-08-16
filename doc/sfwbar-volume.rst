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

Expression Functions
====================

Volume(Query[,Address])
------------------------

function Volume queries the volume settings. The Query parameter
specifies the information to query, the optional Address parameter
specifies the volume control element to which the query applies.
The addresses are server specific. Pulse address is specified in a
format `Device[:Channel]`. Pulse also supports an internal address
format `@pulse-interface-index`, this format is used for elements
returned by VolumeConf and shouldn't be used directly.
ALSA interface is specified as `Device[:Element][:Channel]`.

This function returns a numberic value.

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
"sink-count"
  a number of clients currently available.

VolumeInfo(Query[,Address])
---------------------------

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

VolumeConf("Query")
-------------------

Query device and channel information upon receipt of `volume-conf` or
`volume-conf-removed` event. The valid `Query` values are:

"device"
  an id of a device advertised by `volume-conf` event.
"id"
  an address of a device/channel advertised by `volume-conf` event.
"name"
  name of a channel advertised by `volume-conf` event.
"index"
  ordinal of a channel advertised by `volume-conf` event.
"removed-id"
  an address of a removed device advertised by `volume-conf-removed` event.

Actions
=======

VolumeCtl [Address,],Command
-----------------------------

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

Triggers
========

volume
  a signal emitted whenever the state of the sound server changes.
volume-conf
  emitted when a new or updared volume control element information is
  available.
volume-conf-removed
  emitted when a volume control element is removed.
