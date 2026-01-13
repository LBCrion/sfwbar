sfwbar-mpd
############

#################################
Sfwbar Music Player Daemon module
#################################

:Copyright: GPLv3+
:Manual section: 1

Filename: mpd.so

Requires: none

SYNOPSIS
========

The Music Player Daemon module provides an interface to control an MPD server.

Functions
=========

MpdServer(<address>)
-------------------------

Specify an address for an MPD server to use. The address is specified using the
MPD_HOST convention ([password@]host[:port], where a host can be a local socket
or an abstract socket prefixed by @). If MpdServer is not called, the MPD
module will use MPD_HOST/MPD_PORT environment variables and failing that, will
fallback to $XDG_RUNTIME_DIR/mpd/socket, /run/mpd/socket and localhost:6600.

MpdCmd(<command>)
--------------------

Send a command to the MPD server. You can send any command supported by an MPD
protocol. Output of some MPD commands is processed by the module and made
available to the configuation layer. (Please note that while you can send
command lists, the output of a command list will not be processed). The output
of the following commands is processed:

"list"
  upon completion `mpd-search` trigger is emitted and the results are available
  using `MpdList` function.

"find"
  upon completion `mpd-search` trigger is emitted and the results are available
  using `MpdList` function.

"playlistinfo"
  upon completion `mpd-queue` trigger is emitted and the results are
  available using `MpdList` function.

"listplaylistinfo"
  upon completion `mpd-playlist` trigger is emitted and the results are
  available using `MpdList` function.

"list"
  upon completion `mpd-list` trigger is emitted and the results are
  available using `MpdInfo("list")` function.

"listplaylists"
  upon completion `mpd-playlists` trigger is emitted and the results are
  available using `MpdInfo("playlists")` function.

MpdInfo(<Query>)
------------------------

Function MpdInfo queries the state of the MPD server. The function provides an
interface to query the current value of any tag of the `status` and
`currentsong` MPD protocol commands. The values are returned as strings. In
addition, MpdInfo supports a few additional query values:

"age"
  time (in microseconds) ellapsed since the MPD server state was last queried
  (this is useful in computing the progress of the current song playback).

"cover"
  the coverart of the currently playing album.

"list"
  output of the last executed `list` protocol command. This query returns an
  array of string values.

"playlists"
  output of the last executed `listplaylists` protocol command. This query
  returns an array of string values.

Triggers
========

The module defines two triggers:

"mpd"
  Emitted whenever the state of the MPD server changes.
"mpd-error"
  Emitted when MPD server returns an error. An error string is provded in the
  string variable `message`.
"mpd-list"
  Emitted upon completion of the `list` MPD protocol command.
"mpd-playlists"
  Emitted upon completion of the `listplaylists` MPD protocol command.
"mpd-search"
  Emitted upon completion of the `search` or `find` MPD protocol commands.
"mpd-queue"
  Emitted upon completion of the `playlistinfo` MPD protocol command.
"mpd-playlist"
  Emitted upon completion of the `listplaylistinfo` MPD protocol command.
