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

The notification center module provides desktop notification host.

Expression Functions
====================

NotificationGet(Property)
-------------------------

function NotificationGet fetches a property for a currently advertised
notification.  The supported properties are:

"id"
  an id of a notification, this is used as a unique id to track changes to or
  destruction of the notification.

"removed-id"
  an id of the most recently removed notification. This is populated
  upon emission of the "notification-removed" trigger.

"icon"
  an icon associated with the notification.

"app"
  an app id of the program sending the notification.

"summary"
  a summary text of the notification.

"body"
  the body of the notification.

"time"
  the time when the notification was generated.

"category"
  a category of the notification.

"action_id"
  an id of the currently exposed action.

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

Actions
=======

NotificationAck
---------------

Notify the module that all information for the currently advertised
notification has been processed. The module may then emit another
`notification-updated` event if further notification updates are available.

NotificationAckRemoved
----------------------

Notify the module that all information for the currently advertised
notification removal has been processed. The module may then emit another
`notification-removed` event if further notification removals are queued.

NotificationClose <id>
----------------------

Close a notification with a specified id 

NotificationExpand <id>
-----------------------

Expand a notification group to which notification referenced by `id` belongs.

NotificationCollapse
--------------------

Collapse the currently expanded notification group.

NotificationAction <id>, <action_id>
------------------------------------

Activate an action `action_id` for notification `id`.

Triggers
========

The module defines the following triggers:

"notification-updated"
  this trigger is emitted when a new notification has been received or
  properties of a notification are changed. For a notification with
  multiple actions, this trigger is emitted once for every action.
  Once the trigger is emitted, the information for the relevant notification
  will be available via NotificationGet expression function. Once the config
  finished handling the notification update, it should call action
  NotificationAck. Upon receipt of this action, the module may emit
  "notification-updated" again if further notification updates are queued.

"notification-removed"
  this trigger is emitted when a notification is no longer available and should
  be removed from the layout. The id of the removed notification is available
  via NotificationGet("removed-id"). Once the config finished removing the
  notification, it should call action NotificationAckRemoved. Upon receipt of
  this action, the module may emit another "notification-removed" trigger if
  further notifications have been removed.

"notification-group"
  this trigger is emitted when grouping state of notifications changes. I.e.
  when any group is expanded or collapsed.
