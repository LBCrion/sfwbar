/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"
#include "config.h"
#include <gtk/gtk.h>

void action_exec ( GtkWidget *widget, struct layout_action *action,
    GdkEvent *event )
{
  if(action->command)
  {
    g_debug("widget action: (%d) %s",action->type, action->command);
    switch(action->type)
    {
      case ACT_EXEC:
        g_spawn_command_line_async(action->command,NULL);
        break;
      case ACT_MENU:
        layout_menu_popup(widget, layout_menu_get(action->command), event);
        break;
      case ACT_SWAY:
        sway_ipc_command(action->command);
        break;
    }
  }
}

void action_free ( struct layout_action *action, GObject *old )
{
  if(!action)
    return;

  g_free(action->command);
  g_free(action);
}
