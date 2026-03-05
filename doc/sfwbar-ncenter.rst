sfwbar-ncenter
##############

#################################
Sfwbar Notification center module
#################################

:Copyright: GPLv3+
:Manual section: 1

Filename: ncenter.so

Requires: none

SYNOPSIS
========

The notification center module provides a desktop notification host.

Triggers
========

The module defines the following triggers:

"notification-updated"
  this trigger is emitted when a new notification has been received or
  properties of a notification are changed. Functions handling this trigger can
  access the following variables:
  `id` - an id of a notification.
  `icon` -  an icon associated with the notification.
  `app` - an app id of the program sending the notification.
  `summary` - a summary text of the notification.
  `body` - the body of the notification.
  `time` - the time when the notification was generated.
  `category` - a category of the notification.
  `actions` - a list of actions. The first column contains action id's, the
  second column contains action titles.

"notification-removed"
  this trigger is emitted when a notification is no longer available and should
  be removed from the layout. The id of a removed notification is stored in
  variable `id`.

"notification-group"
  this trigger is emitted when grouping state of notifications changes. I.e.
  when any group is expanded or collapsed.

Functions
=========

NotificationGet(Property)
-------------------------

function NotificationGet fetches a property for a currently advertised
notification.  The supported properties are:


"action_title"
  the title of the currently exposed action.

NotificationGroup(id)
---------------------

Query group expansion state for a given notification id. Possible return values
are: <blank> if the notification group is no expanded, 'header' if this is a
header of the expanded notification 'sole' if the group is expanded, but this
is the sole notification in the group.

NotificationActiveGroup()
-------------------------

Query the app_id of the currently expanded notification group.

NotificationCount([id])
-----------------------

Get count of the notifications. If the `id` is specified, this function returns
the number of notification for the app id of the notification referenced by the
`id`, otherwise it will return the total count of notifications.

NotificationClose(<id>)
-----------------------

Close a notification with a specified id 

NotificationExpand(<id>)
------------------------

Expand a notification group to which notification referenced by `id` belongs.

NotificationCollapse()
----------------------

Collapse the currently expanded notification group.

NotificationAction(<id>, <action_id>)
-------------------------------------

Activate an action `action_id` for notification `id`.
