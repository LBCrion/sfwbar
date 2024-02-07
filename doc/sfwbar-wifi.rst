sfwbar-wifi
###############

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

Wifi module interface provides means to query and control the wifi network.
There are currently two modules providing this interface: wifi-iwd provides 
support for IWD daemon and wifi-nm provides support for NetworkManager.

*** In order to use wifi-iwd module, the user must have permission to control
iwd daemon. Usually permissions are granted by adding the user to `netdev` or
`wheel` user group ***

Expression Functions
====================

WifiGet(Property)
------------------

function WifiGet queries a property for a currently advertized iwd network.
The supported properties are:

"Path"
  an object path of the device, this is used as a unique id to provide
  notifications of changes or destruction of the device and to control
  the device. This property is valid upon receipt of `wifi_updated` trigger.

"RemovedPath"
  an object path of the most recently removed device. This is populated
  upon emission of the `wifi_removed` trigger.

"SSID"
  an SSID of the network.

"Type"
  a Type of the network ('open', 'wep', 'psk' or '8021x')

"Known"
  an indcator of whether this is a known network ("1" = known, "0" = new)

"Strength"
  signal strength of the network (a string containing a number in range 1-100 )

"Connected"
  an indicator of whether the daemon is connected to the network
  ("1" = connected, "0" = not connected)

Actions
=======

WifiScan <Duration>
--------------------

Initiate scan for wireless networks.

WifiAck
-------

Notify the module that all information for the currently advertized network has
been processed. The module may then emit another `wifi_updated` event if further
network updates are available.

WifiAckRemoved
--------------

Notify the module that all information for the currently advertized network
removal has been processed. The module may then emit another `wifi_removed`
event if further network removals are queued.

WifiConnect <path>
------------------

Attempt to connect to a netowrk specified by the path.

WifiDisconnect <path>
---------------------

Disconnect from a netowrk specified by the path.

WifiForget <path>
-----------------

Forget a known network specified by the path.

Triggers
========

The module defines the following triggers:

"wifi_level"
  This signal is emitted when signal strength for the current connection
  changes.

"wifi_updated"
  this trigger is emitted when a new network is discovered or properties of
  a network are changed. Once the trigger is emitted, the information for the
  relevant network will be available via `WifiGet` expression function. Once
  the config finished handling the network update, it should call action
  `WifiAck`.  Upon receipt of this action, the module may emit `wifi_updated`
  again if further network updates are queued.

"wifi_removed"
  this trigger is emitted when a network is no longer available and should be
  removed from the layout. The path of the removed network is available via
  `WifiGet("RemovedPath")`. Once the config finished removing the netowrk, it
  should call action `WifiAckRemoved`. Upon receipt of this action, the module
  may emit another `wifi_removed` trigger if further networks have been
  removed.

"wifi_scan"
  this trigger is emitted when scan for networks has been initiated.

"wifi_scan_complete"
  this trigger is emitted when scan for networks is complete.
