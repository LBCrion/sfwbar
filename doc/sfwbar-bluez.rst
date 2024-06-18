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

Expression Functions
====================

BluezGet(Property)
------------------

function BluezGet fetches a property for a currently advertized bluez device.
The supported properties are:

"Path"
  an object path of the device, this is used as a unique id to provide
  notifivations of changes or destruction of the device.

"RemovedPath"
  an object path of the most recently removed device. This is populated
  upon emission of the "bluez_removed" trigger.

"Address"
  a bluetooth address of the device

"Name"
  a name of the device

"Icon"
  an icon for the device type

"MajorClass"
  a major class of the bluetooth device

"MinorClass"
  a minor class of the bluetooth device

BluezState(Property)
--------------------

function BluezState fetches a boolean state property of the currently
advertized bluez device. The supported properties are:

"Paired"
  specifies whether the device is paired to the adapter

"Trusted"
  specifies whether the device is trusted by the adapter

"Connected"
  specifies whether the device is connected to the adpater

"Running"
  queries the global state of the bluez module. If the module is connected
  to the server and sees at least one bluetooth adapter, this query will
  return true. Otherwise it will return false.

Actions
=======

BluezScan <Duration>
--------------------

Initiate scan for the bluetooth devices. The scan will last for the duration
specified in milliseconds.

BluezAck
--------

Notify the module that all information for the currently advertized device has
been processed. The module may then emit another bluez_changed event if further
device updates are available.

BluezAckRemoved
---------------

Notify the module that all information for the currently advertized device
removal has been processed. The module may then emit another bluez_removed
event if further device removals are queued.

BluezConnect <path>
-------------------

Attempt to connect to the bluetooth device specified by the path.

BluezPair <path>
----------------

Attempt to pair and connect to the bluetooth device specified by the path.

BluezDisconnect <path>
----------------------

Attempt to diconnect from the bluetooth device specified by the path.

BluezRemove <path>
------------------

Attempt to remove  the bluetooth device specified by the path.

Triggers
========

The module defines the following triggers:

"bluez_running"
  this trigger is emitted when connected to the first bluetooth adapter
  has been made or when connection to the last adapter is lost (i.e. 
  when the value of BluezState("Running") query has changed)

"bluez_changed"
  this trigger is emitted when a new bluetooth device is discovered or
  properties of a device are changed. Once the trigger is emitted, the
  information for the relevant device will be available via BluezGet and
  BluezState expression functions. Once the config finished handling the
  device update, it should call action BluezAck. Upon receipt of this
  action, the module may emit "bluez_changed" again if further device
  updates are queued.

"bluez_removed"
  this trigger is emitted when a bluetooth device is no longer available
  and should be removed from the layout. The path of the removed device is
  available via BluezGet("RemovedPath"). Once the config finished removing
  the device, it should call action BluezAckRemoved. Upon receipt of this
  action, the module may emit another "bluez_removed" trigger if further
  devices have been removed.

"bluez_scan"
  this trigger is emitted when scan for bluetooth devices has been initiated.

"bluez_scan_complete"
  this trigger is emitted when scan for bluetooth devices is complete.
