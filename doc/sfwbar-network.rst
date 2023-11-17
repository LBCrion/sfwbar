sfwbar-network
##############

#####################
Sfwbar Network module
#####################

:Copyright: GPLv3+
:Manual section: 1

Filename: network.so

Requires: none

SYNOPSIS
========

The network module tracks te state of the current network connection.

Expression Functions
====================

NetInfo(Query[,Interface])
--------------------------

Function NetInfo queries the parameter of the connection on the network
interface specified. If Interface is not specifies, NetInfo will query the
interface of the default gateway (if one exists). The queries supported are:

"ip"
  IP address of the interface.
"mask"
  Net mask of the interface.
"cidr"
  Net mask in the CIDR notation.
"gateway"
  The default gateway (not necessarily associated with the interface).
"ip6"
  IPv6 IP address of the interface.
"mask6"
  IPv6 netmask of the interfce.
"gateway6"
  The default IPv6 gateway.
"essid"
  ESSID of the wireless connection (if applicable for the interface).

NetInfo returns a string value.

NetStat(Query[,Interface])
--------------------------

Function NetStat queries statistics of the interface. If the interface 
isn't specified, it will be applied to the interface of the default gateway.
The queries supported are:

"rxrate"
  Receive data rate on the interface (in bps).
"txrate"
  Transmit data rate on the interface (in bps).
"signal"
  Signal strength of the wifi connection (if applicable).

NetState returns a numeric value.


Actions
=======
None

Triggers
========
The module defines one trigger "network" which is emitted whenever the interface
data is changes (i.e. ip, netmask, default gateway, wifi essid).
