/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "expr.h"
#include "wayland.h"
#include "bar.h"
#include "basewidget.h"
#include "taskbaritem.h"
#include "config.h"
#include "action.h"
#include "menu.h"
#include "sway_ipc.h"
#include "module.h"
#include "popup.h"
#include "client.h"

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
    stat_win = g_memdup2(win,sizeof(window_t));
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
  gchar *state;
  guint16 mask;

  if(!widget || !value)
    return;

  state = strchr(value,':');
  if(!state)
  {
    state = value;
    mask = WS_USERSTATE;
  }
  else
  {
    state++;
    if(g_ascii_digit_value(*value)==2)
      mask = WS_USERSTATE2;
    else
      mask = WS_USERSTATE;
  }

  if(!g_ascii_strcasecmp(state,"on"))
    base_widget_set_state(widget,mask,TRUE);
  else
    base_widget_set_state(widget,mask,FALSE);
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

    if(IS_BASE_WIDGET(widget))
      state |= base_widget_get_state(widget);
  }
  return state;
}

void action_handle_exec ( gchar *command )
{
  gint argc;
  gchar **argv;

  if(!command)
    return;
  if(!g_shell_parse_argv(command,&argc,&argv,NULL))
    return;
  g_spawn_async(NULL,argv,NULL,G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL |
       G_SPAWN_STDERR_TO_DEV_NULL,NULL,NULL,NULL,NULL);
  g_strfreev(argv);
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

  if( action->type != G_TOKEN_SETVALUE &&
      action->type != G_TOKEN_SETSTYLE &&
      action->type != G_TOKEN_SETTOOLTIP )
    expr_cache_eval(action->command);

  expr_cache_eval(action->addr);

  if(action->addr->cache && (
        action->type == G_TOKEN_MENU ||
        action->type == G_TOKEN_FUNCTION ||
        action->type == G_TOKEN_SETVALUE ||
        action->type == G_TOKEN_SETSTYLE ||
        action->type == G_TOKEN_SETTOOLTIP ||
        action->type == G_TOKEN_IDLEINHIBIT ||
        action->type == G_TOKEN_USERSTATE ))
  {
    widget = base_widget_from_id(action->addr->cache);
    istate = NULL;
  }

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

  if(action->command->cache)
    g_debug("widget action: (%d) %s (addr %s)",action->type, action->command->cache,
        action->addr->cache?action->addr->cache:"null");
  else if(win)
    g_debug("widget action: (%d) on %d",action->type,
        GPOINTER_TO_INT(win->uid));

  if(action->cond & WS_CHILDREN &&
      GTK_IS_CONTAINER(base_widget_get_child(widget)))
  {
    caction = action_dup(action);
    caction->cond = 0;
    caction->ncond = 0;
    children = gtk_container_get_children(
        GTK_CONTAINER(base_widget_get_child(widget)));
    for(iter=children;iter;iter=g_list_next(iter))
      action_exec(iter->data,caction,event,win,NULL);
    g_list_free(children);
    action_free(caction,NULL);
    return;
  }

  switch(action->type)
  {
    case G_TOKEN_EXEC:
      action_handle_exec(action->command->cache);
      break;
    case G_TOKEN_MENU:
      menu_popup(widget, menu_from_name(action->command->cache), event,
          win?win->uid:NULL, &state);
      break;
    case G_TOKEN_MENUCLEAR:
      menu_remove(action->command->cache);
      break;
    case G_TOKEN_PIPEREAD:
      config_pipe_read(action->command->cache);
      break;
    case G_TOKEN_FUNCTION:
      action_function_exec(action->command->cache,widget,event,win,&state);
      break;
    case G_TOKEN_SWAYCMD:
      sway_ipc_command("%s",action->command->cache);
      break;
    case G_TOKEN_MPDCMD:
      client_mpd_command(action->command->cache);
      break;
    case G_TOKEN_SWAYWIN:
      if(win)
        sway_ipc_command("[con_id=%ld] %s",GPOINTER_TO_INT(win->uid),
            action->command->cache);
      break;
    case G_TOKEN_CONFIG:
      config_string(action->command->cache);
      break;
    case G_TOKEN_SETMONITOR:
      bar_set_monitor(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETLAYER:
      bar_set_layer(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETMIRROR:
      bar_set_mirrors(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_BLOCKMIRROR:
      bar_set_mirror_blocks(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETBARSIZE:
      bar_set_size(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETBARID:
      bar_set_id(NULL,action->command->cache);
      break;
    case G_TOKEN_SETBARSENSOR:
      bar_set_sensor(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETBARVISIBILITY:
      if(action->command->cache)
        bar_set_visibility(bar_from_name(action->addr->cache),NULL,
            *(action->command->cache));
      break;
    case G_TOKEN_SETEXCLUSIVEZONE:
      bar_set_exclusive_zone(bar_from_name(action->addr->cache),
          action->command->cache);
      break;
    case G_TOKEN_SETVALUE:
      if(widget && action->command->cache)
        base_widget_set_value(widget,g_strdup(action->command->cache));
      break;
    case G_TOKEN_SETSTYLE:
      if(widget && action->command->cache)
        base_widget_set_style(widget,g_strdup(action->command->cache));
      break;
    case G_TOKEN_SETTOOLTIP:
      if(widget && action->command->cache)
        base_widget_set_tooltip(widget,g_strdup(action->command->cache));
      break;
    case G_TOKEN_IDLEINHIBIT:
      action_idle_inhibit(widget, action->command->cache);
      break;
    case G_TOKEN_USERSTATE:
      action_set_user_state(widget, action->command->cache);
      break;
    case G_TOKEN_POPUP:
      popup_trigger(widget, action->command->cache);
      break;
    case G_TOKEN_CLIENTSEND:
      client_send(action->addr->cache,action->command->cache);
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
    case G_TOKEN_IDENTIFIER:
      module_action_exec(action->ident,action->command->cache,
          action->addr->cache,widget,event,win,&state);
  }
}

action_t *action_new ( void )
{
  action_t *new;

  new = g_malloc0(sizeof(action_t));
  new->command = expr_cache_new();
  new->addr = expr_cache_new();

  return new;
}

action_t *action_dup ( action_t *src )
{
  action_t *dest;

  if(!src)
    return NULL;

  dest = action_new();
  dest->cond = src->cond;
  dest->ncond = src->ncond;
  dest->type = src->type;
  dest->ident = g_strdup(src->ident);

  dest->command->definition = g_strdup(src->command->definition);
  dest->command->cache = g_strdup(src->command->cache);
  dest->command->eval = src->command->eval;
  dest->addr->definition = g_strdup(src->addr->definition);
  dest->addr->cache = g_strdup(src->addr->cache);
  dest->addr->eval = src->addr->eval;
  return dest;
}

void action_free ( action_t *action, GObject *old )
{
  if(!action)
    return;

  expr_cache_free(action->command);
  expr_cache_free(action->addr);
  g_free(action->ident);

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
