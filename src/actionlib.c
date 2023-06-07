/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "bar.h"
#include "basewidget.h"
#include "config.h"
#include "action.h"
#include "menu.h"
#include "sway_ipc.h"
#include "module.h"
#include "popup.h"
#include "client.h"

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

static void swaycmd_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  sway_ipc_command("%s",cmd);
}

static ModuleActionHandlerV1 swaycmd_handler = {
  .name = "SwayCmd",
  .function = (ModuleActionFunc)swaycmd_action
};

static void swaywincmd_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  if(win)
    sway_ipc_command("[con_id=%ld] %s",GPOINTER_TO_INT(win->uid), cmd);
}

static ModuleActionHandlerV1 swaywincmd_handler = {
  .name = "SwayWinCmd",
  .function = (ModuleActionFunc)swaywincmd_action
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

static void config_action_handler ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  config_string(cmd);
}

static ModuleActionHandlerV1 config_handler = {
  .name = "Config",
  .function = (ModuleActionFunc)config_action_handler
};

static void setmonitor_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_monitor(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setmonitor_handler = {
  .name = "SetMonitor",
  .function = (ModuleActionFunc)setmonitor_action
};

static void setlayer_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_layer(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setlayer_handler = {
  .name = "SetLayer",
  .function = (ModuleActionFunc)setlayer_action
};

static void setmirror_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_mirrors(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setmirror_handler = {
  .name = "SetMirror",
  .function = (ModuleActionFunc)setmirror_action
};

static void blockmirror_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_mirror_blocks(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 blockmirror_handler = {
  .name = "BlockMirror",
  .function = (ModuleActionFunc)blockmirror_action
};

static void setbarsize_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_size(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setbarsize_handler = {
  .name = "SetBarSize",
  .function = (ModuleActionFunc)setbarsize_action
};

static void setbarid_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_id(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setbarid_handler = {
  .name = "SetBarID",
  .function = (ModuleActionFunc)setbarid_action
};

static void setbarsensor_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_sensor(bar_from_name(name), cmd);
}

static ModuleActionHandlerV1 setbarsensor_handler = {
  .name = "SetBarSensor",
  .function = (ModuleActionFunc)setbarsensor_action
};

static void setexclusivezone_action ( gchar *cmd, gchar *name, void *widget,
    void *event, window_t *win, guint16 *state )
{
  bar_set_exclusive_zone(bar_from_name(name), cmd);
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
  popup_trigger(widget, cmd);
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
  .name = "ClientSent",
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
  .function = (ModuleActionFunc)unmaximize_action
};

ModuleActionHandlerV1 *action_handlers[] = {
  &exec_handler,
  &function_handler,
  &piperead_handler,
  &menuclear_handler,
  &menu_handler,
  &swaycmd_handler,
  &swaywincmd_handler,
  &mpdcmd_handler,
  &config_handler,
  &setmonitor_handler,
  &setlayer_handler,
  &setmirror_handler,
  &blockmirror_handler,
  &setbarsize_handler,
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
  NULL
};

void action_lib_init ( void )
{
  module_actions_add(action_handlers,"action library");
}
