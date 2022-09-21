/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "wayland.h"
#include "bar.h"
#include "basewidget.h"
#include "taskbaritem.h"
#include "config.h"
#include "action.h"
#include "menu.h"
#include "sway_ipc.h"

static GHashTable *functions;
static GHashTable *trigger_actions;

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
    window_t *win, guint16 *state )
{
  GList *l;
  window_t *stat_win;

  if(!name || !functions)
    return;

  if(win)
  {
    stat_win = g_malloc(sizeof(window_t));
    memcpy(stat_win,win,sizeof(window_t));
  }
  else
    stat_win = NULL;

  for(l = g_hash_table_lookup(functions, name); l; l = g_list_next(l))
    action_exec(w,l->data,ev,stat_win,state);

  g_free(stat_win);
}

void action_idle_inhibit ( GtkWidget *widget, gchar *command )
{
  if(!widget)
    return;

  if(!g_ascii_strcasecmp(command,"on"))
    wayland_set_idle_inhibitor(widget,TRUE);
  if(!g_ascii_strcasecmp(command,"off"))
    wayland_set_idle_inhibitor(widget,FALSE);
}

void action_set_user_state ( GtkWidget *widget, gchar *value )
{
  if(!widget)
    return;

  if(!g_ascii_strcasecmp(value,"on"))
    base_widget_set_state(widget,TRUE);
  else
    base_widget_set_state(widget,FALSE);
}

guint16 action_state_build ( GtkWidget *widget, window_t *win )
{
  guint16 state = 0;

  if(win)
  {
    state = win->state;
    if(wintree_is_focused(win->uid))
      state |= WS_FOCUSED;
  }
  if(widget)
  {
    if(g_object_get_data(G_OBJECT(widget),"inhibitor"))
      state |= WS_INHIBIT;

    if(IS_BASE_WIDGET(widget) && base_widget_get_state(widget))
      state |= WS_USERSTATE;
  }
  return state;
}

void action_client_send ( action_t *action )
{
  ScanFile *file;

  if(!action->addr || !action->command )
    return;

  file = scanner_file_get ( action->addr );

  if(file)
    g_io_channel_write_chars(file->out,action->command,-1,NULL,NULL);
}

void action_exec ( GtkWidget *widget, action_t *action,
    GdkEvent *event, window_t *win, guint16 *istate )
{
  guint16 state;
  GList *children, *iter;
  action_t *caction;

  if(!action)
    return;

  if(IS_TASKBAR_ITEM(widget))
    win = flow_item_get_parent(widget);

  if(action->addr && (
        action->type == G_TOKEN_MENU ||
        action->type == G_TOKEN_FUNCTION ||
        action->type == G_TOKEN_SETVALUE ||
        action->type == G_TOKEN_SETSTYLE ||
        action->type == G_TOKEN_SETTOOLTIP ||
        action->type == G_TOKEN_IDLEINHIBIT ||
        action->type == G_TOKEN_USERSTATE ))
    widget = base_widget_from_id(action->addr);

  if(istate)
    state = *istate;
  else
    state = action_state_build ( widget, win );

  if(action->cond & WS_CHILDREN)
    state |= WS_CHILDREN;

  if(((action->cond & 0x0f) || (action->ncond & 0x0f)) && !win)
      return;
  if(((action->cond & 0xf0) || (action->ncond & 0xf0)) && !widget )
      return;
  if((state & action->cond) != action->cond)
    return;
  if((~state & action->ncond) != action->ncond)
    return;

  if(action->command)
    g_debug("widget action: (%d) %s",action->type, action->command);
  else
    if(win)
      g_debug("widget action: (%d) on %d",action->type,
          GPOINTER_TO_INT(win->uid));

  if(action->cond & WS_CHILDREN)
  {
    caction = action_dup(action);
    caction->cond &= ~WS_CHILDREN;
    state &= ~WS_CHILDREN;
    children = gtk_container_get_children(GTK_CONTAINER(base_widget_get_child(widget)));
    for(iter=children;iter;iter=g_list_next(iter))
      action_exec(iter->data,caction,event,win,&state);
    g_list_free(children);
    action_free(caction,NULL);
    return;
  }

  switch(action->type)
  {
    case G_TOKEN_EXEC:
      if(action->command)
        g_spawn_command_line_async(action->command,NULL);
      break;
    case G_TOKEN_MENU:
      menu_popup(widget, menu_from_name(action->command), event,
          win?win->uid:NULL, &state);
      break;
    case G_TOKEN_MENUCLEAR:
      menu_remove(action->command);
      break;
    case G_TOKEN_PIPEREAD:
      config_pipe_read(action->command);
      break;
    case G_TOKEN_FUNCTION:
      action_function_exec(action->command,widget,event,win,&state);
      break;
    case G_TOKEN_SWAYCMD:
      sway_ipc_command("%s",action->command);
      break;
    case G_TOKEN_MPDCMD:
      mpd_ipc_command(action->command);
      break;
    case G_TOKEN_SWAYWIN:
      if(win)
        sway_ipc_command("[con_id=%ld] %s",GPOINTER_TO_INT(win->uid),
            action->command);
      break;
    case G_TOKEN_CONFIG:
      config_string(action->command);
      break;
    case G_TOKEN_SETMONITOR:
      bar_set_monitor(action->command,action->addr);
      break;
    case G_TOKEN_SETLAYER:
      bar_set_layer(action->command,action->addr);
      break;
    case G_TOKEN_SETBARSIZE:
      bar_set_size(action->command,action->addr);
      break;
    case G_TOKEN_SETBARID:
      sway_ipc_bar_id(action->command);
      break;
    case G_TOKEN_SETEXCLUSIVEZONE:
      if(action->command)
        bar_set_exclusive_zone(action->command,action->addr);
      break;
    case G_TOKEN_SETVALUE:
      if(widget && action->command)
        base_widget_set_value(widget,g_strdup(action->command));
      break;
    case G_TOKEN_SETSTYLE:
      if(widget && action->command)
        base_widget_set_style(widget,g_strdup(action->command));
      break;
    case G_TOKEN_SETTOOLTIP:
      if(widget && action->command)
        base_widget_set_tooltip(widget,g_strdup(action->command));
      break;
    case G_TOKEN_IDLEINHIBIT:
      action_idle_inhibit(widget, action->command);
      break;
    case G_TOKEN_USERSTATE:
      action_set_user_state(widget, action->command);
      break;
    case G_TOKEN_CLIENTSEND:
      action_client_send(action);
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

action_t *action_dup ( action_t *src )
{
  action_t *dest;

  if(!src)
    return NULL;

  dest = g_malloc0(sizeof(action_t));
  dest->cond = src->cond;
  dest->ncond = src->ncond;
  dest->type = src->type;
  dest->command = g_strdup(src->command);
  dest->addr = g_strdup(src->addr);
  return dest;
}

void action_free ( action_t *action, GObject *old )
{
  if(!action)
    return;

  g_free(action->command);
  g_free(action->addr);
  g_free(action);
}

void action_trigger_add ( action_t *action, gchar *trigger )
{
  void *old;

  if(!trigger_actions)
    trigger_actions = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  old = g_hash_table_lookup(trigger_actions,trigger);
  if(old)
  {
    g_message("Action for trigger '%s' is already defined",trigger);
    g_free(trigger);
    action_free(action,NULL);
    return;
  }

  g_hash_table_insert(trigger_actions, trigger, action);
}

action_t *action_trigger_lookup ( gchar *trigger )
{
  if(!trigger_actions)
    return NULL;

  return g_hash_table_lookup(trigger_actions,trigger);
}
