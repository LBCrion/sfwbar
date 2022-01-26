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
    struct wt_window *win )
{
  GList *l;
  struct wt_window *stat_win;

  if(!name)
    return;
 
  if(win)
  {
    stat_win = g_malloc(sizeof(struct wt_window));
    memcpy(stat_win,win,sizeof(struct wt_window));
  }
  else
    stat_win = NULL;

  for(l = g_hash_table_lookup(functions, name); l; l = g_list_next(l))
    action_exec(w,l->data,ev,stat_win);

  g_free(stat_win);
}

void action_exec ( GtkWidget *widget, struct layout_action *action,
    GdkEvent *event, struct wt_window *win )
{
  if(action->cond || action->ncond)
  {
    if(!win)
      return;
    if(wintree_is_focused(win->uid))
      win->state |= WS_FOCUSED;
    else
      win->state &= ~WS_FOCUSED;
    if((win->state & action->cond) != action->cond)
      return;
    if(((~win->state) & action->ncond) != action->ncond)
      return;
  }

  if(action->command)
    g_debug("widget action: (%d) %s",action->type, action->command);
  else
    if(win)
      g_debug("widget action: (%d) on %d",action->type,GPOINTER_TO_INT(win->uid));

  switch(action->type)
  {
    case G_TOKEN_EXEC:
      if(action->command)
        g_spawn_command_line_async(action->command,NULL);
      break;
    case G_TOKEN_MENU:
      if(action->command && win)
        layout_menu_popup(widget, layout_menu_get(action->command), event, win->uid);
      break;
    case G_TOKEN_MENUCLEAR:
      if(action->command)
        layout_menu_remove(action->command);
      break;
    case G_TOKEN_PIPEREAD:
      if(action->command)
        config_pipe_read(action->command);
      break;
    case G_TOKEN_FUNCTION:
      if(action->command)
        action_function_exec(action->command,widget,event,win);
      break;
    case G_TOKEN_SWAYCMD:
      if(action->command)
        sway_ipc_command("%s",action->command);
      break;
    case G_TOKEN_SWAYWIN:
      if(action->command && win)
        sway_ipc_command("[con_id=%ld] %s",GPOINTER_TO_INT(win->uid),
            action->command);
      break;
    case G_TOKEN_CONFIG:
      if(action->command)
        config_string(action->command);
      break;
    case G_TOKEN_SETMONITOR:
      if(action->command)
        set_monitor(action->command);
      break;
    case G_TOKEN_SETLAYER:
      if(action->command)
        set_layer(action->command);
      break;
    case G_TOKEN_SETBARSIZE:
      if(action->command)
        bar_set_size(action->command);
      break;
    case G_TOKEN_SETBARID:
      if(action->command)
        sway_ipc_bar_id(action->command);
      break;
    case G_TOKEN_FOCUS:
      if(win)
        wintree_focus(win->uid);
      break;
    case G_TOKEN_CLOSE:
      if(win)
        wintree_close(win->uid);
      break;
    case G_TOKEN_MINIMIZE:
      if(win)
        wintree_minimize(win->uid);
      break;
    case G_TOKEN_MAXIMIZE:
      if(win)
        wintree_maximize(win->uid);
      break;
    case G_TOKEN_UNMINIMIZE:
      if(win)
        wintree_unminimize(win->uid);
      break;
    case G_TOKEN_UNMAXIMIZE:
      if(win)
        wintree_unmaximize(win->uid);
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
