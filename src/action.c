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

void action_function_exec ( gchar *name, GtkWidget *w, GdkEvent *ev,
    gpointer wid )
{
  GList *l;

  if(!name)
    return;

  for(l = g_hash_table_lookup(functions, name); l; l = g_list_next(l))
    action_exec(w,l->data,ev,wid);
}

void action_exec ( GtkWidget *widget, struct layout_action *action,
    GdkEvent *event, gpointer wid )
{
  if(action->command)
    g_debug("widget action: (%d) %s",action->type, action->command);
  else
    g_debug("widget action: (%d) on %d",action->type,GPOINTER_TO_INT(wid));

  switch(action->type)
  {
    case ACT_EXEC:
      if(action->command)
        g_spawn_command_line_async(action->command,NULL);
      break;
    case ACT_MENU:
      if(action->command)
        layout_menu_popup(widget, layout_menu_get(action->command), event, wid);
      break;
    case ACT_CLEAR:
      if(action->command)
        layout_menu_remove(action->command);
      break;
    case ACT_PIPE:
      if(action->command)
        config_pipe_read(action->command);
      break;
    case ACT_FUNC:
      if(action->command)
        action_function_exec(action->command,widget,event,wid);
      break;
    case ACT_SWAY:
      if(action->command)
        sway_ipc_command("%s",action->command);
      break;
    case ACT_SWIN:
      if(action->command)
        sway_ipc_command("[con_id=%ld] %s",GPOINTER_TO_INT(wid),
            action->command);
      break;
    case ACT_CONF:
      if(action->command)
        config_string(action->command);
      break;
    case ACT_FOCUS:
      wintree_focus(wid);
      break;
    case ACT_CLOSE:
      wintree_close(wid);
      break;
    case ACT_MIN:
      wintree_minimize(wid);
      break;
    case ACT_MAX:
      wintree_maximize(wid);
      break;
    case ACT_UNMIN:
      wintree_unminimize(wid);
      break;
    case ACT_UNMAX:
      wintree_unmaximize(wid);
      break;
  }
}

void action_free ( struct layout_action *action, GObject *old )
{
  if(!action)
    return;

  g_free(action->command);
  g_free(action);
}
