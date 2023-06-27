sfwbar-alsa
###########

##################
Sfwbar ALSA module
##################

:Copyright: GPLv3+
:Manual section: 1

Filename: alsactl.so

Requires: libasound

SYNOPSIS
========

The ALSA  module provides an interface to query and adjust the settings of
the ALSA sound system. 

Expression Functions
====================

Alsa(Query[,Element])
------------------------

function Alsa queries the state of the ALSA system, the Query parameter
specifies the information to query, the optional Element parameter
specifies the mixer element to which the query applies. If the Element isn't
given the query is applied to the "Master" element or the current card. The
supported query types are:

"playback-volume"
  the volume of a sink.
"playback-mute"
  muted state of the sink.
"capture-volume"
  the volume of a source.
"capture-mute"
  muted state of the source.

Actions
=======

AlsaCmd [Interface,],Command
-----------------------------

Manipulate the state of the interface, if the interface isn't specified, the
Command will be applied to the current interface.

"playback-volume +/-X"
  Adjust the playback volume by X%. If the value isn't prefixed by either
  `+` or `-`, the volume is set to the percentage value X.
"playback-mute State"
  Change the playback mute state, State can be On, Off or Toggle.
"capture-volume +/-X"
  Adjust the capture volume by X%. If the value isn't prefixed by either
  `+` or `-`, the volume is set to the percentage value X.
"capture-mute State"
  Change the capture mute state, State can be On, Off or Toggle.

AlsaSetCard Card
-----------------------------

Select ALSA sound card. This action accepts one paramter with a card name, by
default the module will use "default" card.

Triggers
========

The module defines one trigger "Alsa" which is emitted whenever the state of
the ALSA system changes.
