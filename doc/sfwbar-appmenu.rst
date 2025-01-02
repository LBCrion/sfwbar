sfwbar-appmenu
##############

##############################
Sfwbar Application menu module
##############################

:Copyright: GPLv3+
:Manual section: 1

Filename: appmenu.so

Requires: none

SYNOPSIS
========

The application menu module creates and maintains the applications menu object.
This module has no triggers, actions or expression functions. The application
menu is named `app_menu_system`.

Actions
=======

AppMenuFilter Filename
----------------------

Don't add a program with a .desktop file matching Filename. The Filename should
include the .desktop extension.


AppMenuItemTop Title, Command
-----------------------------

Add a custom item to the top of the application menu. The Title parameter is
the name of the item (it can include a '%' separated icon name). The command
is the command to execute upon the item activation.

AppMenuItemBottom Title, Command
--------------------------------

Add a custom item to the bottom of the application menu. The parameters are
the same as for AppMenuItemTop action.
