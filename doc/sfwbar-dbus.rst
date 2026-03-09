sfwbar-dbus
###########

############################
Sfwbar DBus interface module
############################

:Copyright: GPLv3+
:Manual section: 1

Filename: dbus.so

Requires: none

SYNOPSIS
========

The DBus interface module provides means for invocation of DBus calls and
monitoring DBus names and listening to DBus signals.

Triggers
========

This module does not have emit triggers with pre-defined names, it does emit
triggers defined by the user when calling `DBusSubscribe` and `DBusWatch`
functions.

Functions
=========

DBusCall(Address, Method [, ParamType, Param])
----------------------------------------------

Call a DBus method. `Address` is an array of 4 strings, the first string is
bus, which can be "system" or "user", the second string is the bus name, the
third is the object path and the fourth is the interface name.
`Method` is a string specifying the name of the method to call. Optionally,
`ParamType` and `Param` can be specified, providing a DBus type string of the
method parameters and an array containing the parameter data. This function
returns any data returned by the call. See DBus type conversion for information
on constructing the `Param` parameter and parsing the return value.

DBusSubscribe(Address, Signal, Trigger [, Arg0])
------------------------------------------------

Subscribe to a DBus signal.  `Address` is an array of 4 strings, the first
string is bus, which can be "system" or "user", the second string is the bus
name, the third is the object path and the fourth is the interface name.
`Signal` is the name of a signal to subscribe to. `Trigger` is a name of a
trigger to emit upon receipt of the signal. An optional parameter `Arg0`
allows filtering the first string of the signal parameters. If specified, any
message where the first string parameter doesn't match `Arg0` will be ignored.
Triggers called by `DBusSubscribe` exposer a variable `DBusSignal` containing
the parameters for the signal. See `DBus type conversion` section for
information on parsing this variable. Retuns n/a.

DBusWatch(Bus, Name, Trigger)
-----------------------------

Watch a name on a bus. Parameter `Bus` specifies the type of a bus to watch a
name on. It can be either "system" or "user". `Name` is the name to watch and
`Trigger` is the name of a trigger to emit when the name appears or disappears.
Triggers called in response to `DBusWatch` supply a variable `DBusStatus` which
may contain one of two values, "appeared" and "vanished", corresponding to the 
state of the DBus name being watched. Returns n/a.

DBus type conversion
====================

DBus uses a complex type system for serializing multiple values. Data supplied
to DBus function and received from the comes in this form. Sfwbar uses a much
simpler type system, which needs to be mapped to and from the DBus types.

DBus types map to Sfwbar types as following:
DBus types `String`, `Object path` and `Signature` correspond to sfwbar
`String` type.
DBus types `Boolean`, `Bytes`, `Int16`, `Uint16`, `Int32`, `Uint32`, `Int64`,
`Uint64`, `Handle` and `Double` correspond to sfwbar `Numberic` type.
DBus `Tuples`. `Arrays` and `Dictionary entries` correspond to sfwbar `Array`
type.

This means that DBus type `(ss)` corresponds to an sfwbar variable
`["string", "string"]`

Complex DBus data structures would be parsed into sfwbar arrays, with nested
arrays where necessary. I.e. `(sas)` would become `["String", ["string"... ]]`
When parsing data received from DBus, it may be helpful to use Print on the
received data to check the system of the nested arrays it's mapped to.

When encoding DBus data, there is an important diversion from the standard DBus
type convention. When encoding a variant value `v`, you need to supply a hint
for the underlying type. I.e. you can't just supply `(v)`, you need to tell
sfwbar what type the variant will take. I.e. `(vs)` to supply a variant holding
a string or `(v(ss))` for a variant holding a tuple of two strings.
