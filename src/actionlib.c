/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "sfwbar.h"
#include "config.h"
#include "action.h"
#include "module.h"
#include "client.h"
#include "appinfo.h"
#include "gui/bar.h"
#include "gui/basewidget.h"
#include "gui/menu.h"
#include "gui/pageritem.h"
#include "gui/popup.h"
#include "gui/switcher.h"

static void exec_action ( gchar *cmd, gchar *name, void *widget,
    void *event, void *win, void *state )
{
  gint argc;
  gchar **argv;

  if(!cmd || !g_shell_parse_argv(cmd,&argc,&argv,NULL))
    return;
  g_spawn_async(NULL,argv,NULL,G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL |
       G_SPAWN_STDERR_TO_DEV_NULL,NULL,NULL,NULL,NULL);
  g_strfreev(argv);
}

static ModuleActionHandlerV1 exec_handler = {
  .name = "Exec",
  .function = (ModuleActionFunc)exec_action
};

static void function_action ( gchar *cmd, gchar *name, void *widget,
    void *event, void *win, void *state )
{
  action_function_exec(cmd,widget,event,win,state);
}

static ModuleActionHandlerV1 function_handler = {
  .name = "Function",
  .flags = MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)function_action
};

static void piperead_action ( gchar *cmd, gchar *name, void *widget,
    void *event, void *win, void *state )
{
  config_pipe_read(cmd);
}
  
static ModuleActionHandlerV1 piperead_handler = {
  .name = "PipeRead",
  .function = piperead_action
};

static void menuclear_action ( gchar *cmd, gchar *name, void *widget,
    void *event, void *win, void *state )
{
  menu_remove(cmd);
}

static ModuleActionHandlerV1 menuclear_handler = {
  .name = "MenuClear",
  .function = menuclear_action
};

static void menuitemclear_action ( gchar *cmd, gchar *name, void *widget,
    void *event, void *win, void *state )
{
  menu_item_remove(cmd);
}

static ModuleActionHandlerV1 menuitemclear_handler = {
  .name = "MenuItemClear",
  .function = menuitemclear_action
};

static void menu_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  menu_popup(widget, menu_from_name(cmd), event,
      win?win->uid:NULL, state);
}

static ModuleActionHandlerV1 menu_handler = {
  .name = "Menu",
  .flags = MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)menu_action
};

static void mpdcmd_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  client_mpd_command(cmd);
}

static ModuleActionHandlerV1 mpdcmd_handler = {
  .name = "MpdCmd",
  .function = (ModuleActionFunc)mpdcmd_action
};

static void config_func_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  config_string(cmd);
}

static ModuleActionHandlerV1 config_handler = {
  .name = "Config",
  .function = (ModuleActionFunc)config_func_action
};

static void map_icon_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(cmd && name)
    app_icon_map_add(name, cmd);
}

static ModuleActionHandlerV1 map_icon_handler = {
  .name = "MapIcon",
  .function = (ModuleActionFunc)map_icon_action
};

static void setmonitor_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetMonitor is deprecated, please use monitor property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_monitor(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_monitor);
}

static ModuleActionHandlerV1 setmonitor_handler = {
  .name = "SetMonitor",
  .function = (ModuleActionFunc)setmonitor_action
};

static void setlayer_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetLayer is deprecated, please use layer property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_layer(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_layer);
}

static ModuleActionHandlerV1 setlayer_handler = {
  .name = "SetLayer",
  .function = (ModuleActionFunc)setlayer_action
};

static void setmirror_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetMirror is deprecated, please use mirror property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_mirrors_old(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_mirrors_old);
}

static ModuleActionHandlerV1 setmirror_handler = {
  .name = "SetMirror",
  .function = (ModuleActionFunc)setmirror_action
};

static void setbarsize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetBarSize is deprecated, please use size property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_size(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_size);
}

static ModuleActionHandlerV1 setbarsize_handler = {
  .name = "SetBarSize",
  .function = (ModuleActionFunc)setbarsize_action
};

static void setbarmargin_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;

  g_message("SetBarMargin is deprecated, please use margin property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_margin(bar, g_ascii_strtoll(cmd, NULL, 10));
  else if( (list = bar_get_list()) )
  {
    g_hash_table_iter_init(&iter, list);
    while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&bar))
      bar_set_margin(bar, g_ascii_strtoll(cmd, NULL, 10));
  }
}

static ModuleActionHandlerV1 setbarmargin_handler = {
  .name = "SetBarMargin",
  .function = (ModuleActionFunc)setbarmargin_action
};

static void setbarid_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetBarID is deprecated, please use sway_bar_id property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_id(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_id);
}

static ModuleActionHandlerV1 setbarid_handler = {
  .name = "SetBarID",
  .function = (ModuleActionFunc)setbarid_action
};

static void setbarsensor_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;
  gint64 timeout;

  g_message("SetBarSensor is deprecated, please use sensor property instead");

  timeout = g_ascii_strtoll(cmd, NULL, 10);

  if( (bar = bar_from_name(name)) )
    bar_set_sensor(bar, timeout);
  else if( (list = bar_get_list()) )
  {
    g_hash_table_iter_init(&iter, list);
    while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&bar))
      bar_set_sensor(bar, timeout);
  }
}

static ModuleActionHandlerV1 setbarsensor_handler = {
  .name = "SetBarSensor",
  .function = (ModuleActionFunc)setbarsensor_action
};

static void setexclusivezone_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *bar;

  g_message("SetExclusiveSone is deprecated, please use"
     " exclusive_zone property instead");

  if( (bar = bar_from_name(name)) )
    bar_set_exclusive_zone(bar, cmd);
  else
    bar_address_all(NULL, cmd, bar_set_exclusive_zone);
}

static ModuleActionHandlerV1 setexclusivezone_handler = {
  .name = "SetExclusiveZone",
  .function = (ModuleActionFunc)setexclusivezone_action
};

static void setbarvisibility_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(cmd)
    bar_set_visibility(bar_from_name(name), NULL, *cmd);
}

static ModuleActionHandlerV1 setbarvisibility_handler = {
  .name = "SetBarVisibility",
  .function = (ModuleActionFunc)setbarvisibility_action
};

static void setvalue_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(widget && cmd)
    base_widget_set_value(widget,g_strdup(cmd));
}

static ModuleActionHandlerV1 setvalue_handler = {
  .name = "SetValue",
  .flags = MODULE_ACT_CMD_BY_DEF | MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)setvalue_action
};

static void setstyle_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(widget && cmd)
    base_widget_set_style(widget,g_strdup(cmd));
}

static ModuleActionHandlerV1 setstyle_handler = {
  .name = "SetStyle",
  .flags = MODULE_ACT_CMD_BY_DEF | MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)setstyle_action
};

static void settooltip_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(widget && cmd)
    base_widget_set_tooltip(widget,g_strdup(cmd));
}

static ModuleActionHandlerV1 settooltip_handler = {
  .name = "SetTooltip",
  .flags = MODULE_ACT_CMD_BY_DEF | MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)settooltip_action
};

static void userstate_action ( gchar *value, gchar *name, void *widget,
    void *event, window_t *win, guint16 *st )
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

static ModuleActionHandlerV1 userstate_handler = {
  .name = "UserState",
  .flags = MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)userstate_action
};

static void popup_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  popup_trigger(widget, cmd, event);
}

static ModuleActionHandlerV1 popup_handler = {
  .name = "PopUp",
  .flags = MODULE_ACT_WIDGET_ADDRESS,
  .function = (ModuleActionFunc)popup_action
};

static void clientsend_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  client_send(name, cmd);
}

static ModuleActionHandlerV1 clientsend_handler = {
  .name = "ClientSend",
  .function = (ModuleActionFunc)clientsend_action
};

static void focus_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_focus(win->uid);
}

static ModuleActionHandlerV1 focus_handler = {
  .name = "Focus",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)focus_action
};

static void close_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_close(win->uid);
}

static ModuleActionHandlerV1 close_handler = {
  .name = "Close",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)close_action
};

static void minimize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_minimize(win->uid);
}

static ModuleActionHandlerV1 minimize_handler = {
  .name = "Minimize",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)minimize_action
};

static void maximize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_maximize(win->uid);
}

static ModuleActionHandlerV1 maximize_handler = {
  .name = "Maximize",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)maximize_action
};

static void unminimize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_unminimize(win->uid);
}

static ModuleActionHandlerV1 unminimize_handler = {
  .name = "UnMinimize",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)unminimize_action
};

static void unmaximize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    wintree_unmaximize(win->uid);
}

static ModuleActionHandlerV1 unmaximize_handler = {
  .name = "UnMaximize",
  .flags = MODULE_ACT_WIDGET_ADDRESS | MODULE_ACT_ADDRESS_ONLY,
  .function = (ModuleActionFunc)unmaximize_action
};

static void eval_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  gchar *val;

  if(!cmd || !name)
    return;

  val = config_value_string(g_strdup(""), cmd);
  scanner_var_new(name, NULL, val, G_TOKEN_SET, VT_FIRST);
  g_free(val);
}

static ModuleActionHandlerV1 eval_handler = {
  .name = "Eval",
  .function = (ModuleActionFunc)eval_action
};

static void switcher_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(!cmd || !g_strcasecmp(cmd,"forward"))
    switcher_event(NULL);
  if(cmd && !g_strcasecmp(cmd,"back"))
    switcher_event((void *)1);
}

static ModuleActionHandlerV1 switcher_handler = {
  .name = "SwitcherEvent",
  .function = (ModuleActionFunc)switcher_action
};

static void clear_widget_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  GtkWidget *w;

  if(!cmd || !(w = base_widget_from_id(cmd)) )
    return;

  gtk_widget_destroy(w);
}

static ModuleActionHandlerV1 clear_widget_handler = {
  .name = "ClearWidget",
  .function = (ModuleActionFunc)clear_widget_action
};

static void taskbar_item_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, void *state )
{
  if(!win)
    return;
  if (wintree_is_focused(win->uid) && !(win->state & WS_MINIMIZED))
    wintree_minimize(win->uid);
  else
    wintree_focus(win->uid);
}

static ModuleActionHandlerV1 taskbar_item_handler = {
  .name = "TaskbarItemDefault",
  .function = (ModuleActionFunc)taskbar_item_action
};

static void workspace_activate_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, void *state )
{
  if(cmd)
    widget = base_widget_from_id(cmd);

  if(widget && IS_PAGER_ITEM(widget))
    workspace_activate(flow_item_get_source(widget));
}

static ModuleActionHandlerV1 workspace_activate_handler = {
  .name = "WorkspaceActivate",
  .function = (ModuleActionFunc)workspace_activate_action
};

static ModuleActionHandlerV1 *action_handlers[] = {
  &exec_handler,
  &function_handler,
  &piperead_handler,
  &menuclear_handler,
  &menuitemclear_handler,
  &menu_handler,
  &mpdcmd_handler,
  &config_handler,
  &map_icon_handler,
  &setmonitor_handler,
  &setlayer_handler,
  &setmirror_handler,
  &setbarsize_handler,
  &setbarmargin_handler,
  &setbarid_handler,
  &setexclusivezone_handler,
  &setbarsensor_handler,
  &setbarvisibility_handler,
  &setvalue_handler,
  &setstyle_handler,
  &settooltip_handler,
  &userstate_handler,
  &popup_handler,
  &clientsend_handler,
  &focus_handler,
  &close_handler,
  &minimize_handler,
  &maximize_handler,
  &unminimize_handler,
  &unmaximize_handler,
  &eval_handler,
  &switcher_handler,
  &clear_widget_handler,
  &taskbar_item_handler,
  &workspace_activate_handler,
  NULL
};

void action_lib_init ( void )
{
  module_actions_add(action_handlers,"action library");
}
