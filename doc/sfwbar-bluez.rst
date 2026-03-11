sfwbar-bluez
############

###################
Sfwbar Bluez module
###################

:Copyright: GPLv3+
:Manual section: 1

Filename: bluez.so

Requires: none

SYNOPSIS
========

The bluez module provides an interface to connect and disconnect bluetooth
devices via the bluez server.

Triggers
========

The module defines the following triggers:

"bluez-adapter"
  this trigger is emitted when state of one of the bluetooth adapters changes.

"bluez-conf"
  this trigger is emitted when a new bluetooth device is discovered or
  properties of a device are changed. Functions handling this trigger can
  access variable `path` containing an address of an updated device.

"bluez-conf-removed"
  this trigger is emitted when a bluetooth device is no longer available
  and should be removed from the layout.  Functions handling this trigger can
  access variable `path` containing an address of a removed device.

Functions
=========

BluezDevice(<path>, <property>)
-------------------------------
function BluezGet fetches a property for a bluez device specified by `path`.
The supported `property` valued are:

"Name"
  a name of the device. Returns <string>.

"Address"
  a bluetooth address of the device. Returns <string>.

"Icon"
  an icon for the device type. Returns <string>.

"MajorClass"
  a major class of the bluetooth device. Returns <string>.

"MinorClass"
  a minor class of the bluetooth device. Returns <string>.

"Paired"
  specifies whether the device is paired to the adapter. Returns <boolean>.

"Trusted"
  specifies whether the device is trusted by the adapter. Returns <boolean>.

"Connected"
  specifies whether the device is connected to the adpater. Returns <boolean>.

"Connecting"
  specifies whether the adapter is currently attempting to connect to the
  device. Returns <boolean>.

"Running"
  queries the global state of the bluez module. If the module is connected
  to the server and sees at least one bluetooth adapter, this query will
  return true. Otherwise it will return false.

BluezAdapter(<property>)
------------------------
Function `BluezAdapter` queries the state of the default adapter. Valid
`property` values are:

"Count"
  count of bluetooth adapters present in the system. Returns <numeric>.

"Discovering"
  check if the default adapter is currently scanning for devices.
  Returns <boolean>.

"Discoverable"
  check if the adapter is discoverable by other devices. Returns <boolean>.

"Powered"
  check if the device is powered. Returns <boolean>.

BluezScan(<Duration>)
---------------------
Initiate scan for bluetooth devices. The scan will last for the `duration`
specified in seconds.

BluezConnect(<path>)
--------------------
Attempt to connect to the bluetooth device specified by `path`.

BluezPair(<path>)
-----------------
Attempt to pair and connect to the bluetooth device specified by `path`.

BluezDisconnect(<path>)
-----------------------
Attempt to disconnect from the bluetooth device specified by `path`.

BluezRemove(<path>)
-------------------
Attempt to remove  the bluetooth device specified by `path`.
