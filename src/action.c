/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"
#include "config.h"
#include <gtk/gtk.h>

static GHashTable *functions;

void action_function_add ( gchar *name, GList *actions )
{
  GList *list;

  if(!functions)
    functions = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  list = g_hash_table_lookup(functions,name);
  if(list)
  {
    list = g_list_concat(list,actions);
    g_hash_table_replace(functions,name,list);
    g_free(name);
  }
  else
    g_hash_table_insert(functions, name, actions);
}

void action_function_exec ( gchar *name, GtkWidget *w, GdkEvent *ev )
{
  GList *l;

  if(!name)
    return;

  for(l = g_hash_table_lookup(functions, name); l; l = g_list_next(l))
    action_exec(w,l->data,ev);
}

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
      case ACT_CLEAR:
        layout_menu_remove(action->command);
        break;
      case ACT_PIPE:
        config_pipe_read(action->command);
        break;
      case ACT_FUNC:
        action_function_exec(action->command,widget,event);
        break;
      case ACT_SWAY:
        sway_ipc_command(action->command);
        break;
      case ACT_CONF:
        config_string(action->command);
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
