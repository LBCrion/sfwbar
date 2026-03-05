sfwbar-wifi
###########

#####################
Sfwbar Wifi interface
#####################

:Copyright: GPLv3+
:Manual section: 1

Filename: wifi-iwd.so
Filename: wifi-nm.so

Requires: none

SYNOPSIS
========

Wifi module interface provides means to query and control the wifi network
connections.  There are currently two modules providing this interface:
wifi-iwd provides support for IWD daemon and wifi-nm provides support for
NetworkManager.

*** In order to use wifi-iwd module, the user must have permission to control
iwd daemon. Usually permissions are granted by adding the user to `netdev` or
`wheel` user group ***

Triggers
========

The module defines the following triggers:

wifi-level
  This signal is emitted when signal strength for the current connection
  changes.

wifi-conf
  this trigger is emitted when a new wireless network is discovered or
  properties of a network have changed. Once the trigger is emitted.
  Functions handling this trigger can access variable `id` containing the id
  of the network.

wifi-conf-removed
  this trigger is emitted when a network is no longer available and should be
  removed from the layout.  Functions handling this trigger can access variable
  `id` containing the id of the network.

"wifi-scan"
  this trigger is emitted when scan for networks has been initiated.

"wifi-scan-complete"
  this trigger is emitted when scan for networks is complete.

Functions
=========

WifiGet(ID, Property)
---------------------

Function WifiGet queries a property of a network specified by parameter `ID`
The supported properties are:

"SSID"
  an SSID of the network. Returns <string>

"Type"
  a Type of the network. Returns <string> ('open', 'wep', 'psk' or '8021x')

"Known"
  an indcator of whether this is a known network. Returns <boolean>.

"Strength"
  signal strength of the network. Returns <numeric> (0..100)

"Connected"
  an indicator of whether the daemon is connected to the network.
  Returns <boolean>

WifiScan([<Duration>])
----------------------

Initiate scan for wireless networks. An optional <numeric> parameter specifies
duration of a scan in seconds.

WifiConnect(<ID>)
-----------------

Attempt to connect to a network specified by the `ID`.

WifiDisconnect(<ID>)
--------------------

Disconnect from a network specified by the `ID`.

WifiForget(<ID>)
----------------

Forget a known network specified by the `ID`.
