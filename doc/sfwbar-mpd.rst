sfwbar-mpd
############

#################################
Sfwbar Music Player Daemon module
#################################

:Copyright: GPLv3+
:Manual section: 1

Filename: mpd.so

Requires: libmpdclient

SYNOPSIS
========

The Music Player Daemon module provides an interface to control an MPD server.

Expression Functions
====================

Mpd(Query)
------------------------

Function Mpd queries the state of the MPD server. The function returns a string.
the following queries are supported:

"title"
  The title of current song.
"track"
  Track number of the current song.
"artist"
  Artist of the current song.
"album"
  Album of the current song.
"genre"
  Genre of the current song.
"volume"
  Current playback volume.
"repeat"
  Repeat flag of the current playlist: "1"/"0".
"random"
  Random flag of the current playlist: "1"/"0".
"queue_len"
  Length of the current playlist.
"queue_pos"
  Number of the current song in the playlist.
"elapsed"
  Elapsed time since the beginning of the song.
"length"
  Length of the current song.
"rate"
  Sample rate of the current song (in kbps).
"state"
  State of the player: "play","pause","stop","unknown".

Actions
=======

MpdSetPassword "Password"
-------------------------

Specify a password used to connect to the MPD server.

MpdCommand "Command"
--------------------

Send a command to the MPD server. The supported commands are:

"play"
  Play the current song.
"pause"
  Pause playback.
"stop"
  Stop playback.
"prev"
  Switch to the previous song in the playlist.
"next"
  Switch to the next song in the playlist.

Triggers
========

The module defines two triggers:

"mpd"
  Emitted whenever the state of the MPD server changes.
"mpd-progress"
  Emitted every second while player is in "play" state.
