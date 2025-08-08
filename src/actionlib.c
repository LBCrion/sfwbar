/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "module.h"
#include "client.h"
#include "trigger.h"
#include "appinfo.h"
#include "config/config.h"
#include "gui/bar.h"
#include "gui/basewidget.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/pageritem.h"
#include "gui/popup.h"
#include "gui/taskbaritem.h"
#include "util/string.h"
#include "vm/vm.h"

static value_t action_exec_impl ( vm_t *vm, value_t p[], gint np )
{
  gint argc;
  gchar **argv;

  if(np!=1 || !value_is_string(p[0]) || !p[0].value.string)
    return value_na;

  if(!g_shell_parse_argv(p[0].value.string, &argc, &argv, NULL))
    return value_na;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL |
       G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
  g_strfreev(argv);

  return value_na;
}

static value_t action_function ( vm_t *vm, value_t p[], gint np )
{
  vm_function_t *func;

  vm_param_check_np_range(vm, np, 1, 2, "Function");
  vm_param_check_string(vm, p, 0, "Function");

  if(np==2)
  {
    vm_param_check_string(vm, p, 1, "Function");
    vm_target_push(vm, value_get_string(p[0]), NULL, NULL);
  }

  if( (func = vm_func_lookup(value_get_string(p[np-1]))) &&
      (func->flags & VM_FUNC_USERDEFINED) )
    value_free(vm_function_user(vm, func->ptr.code, 0));

  if(np==2)
    vm_target_pop(vm);

  return value_na;
}

static value_t action_piperead ( vm_t *vm, value_t p[], gint np )
{
  gchar *conf, **argv;
  gint argc;

  vm_param_check_np(vm, np, 1, "Piperead");
  vm_param_check_string(vm, p, 0, "Piperead");

  if(!g_shell_parse_argv(value_get_string(p[0]), &argc, &argv, NULL))
    return value_na;

  if(g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH |
        G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &conf, NULL, NULL, NULL))
    config_parse_data(value_get_string(p[0]), conf, NULL, vm->store, FALSE);

  g_free(conf);
  g_strfreev(argv);
  return value_na;
}

static value_t action_menuclear ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MenuClear");
  vm_param_check_string(vm, p, 0, "MenuClear");

  menu_remove(value_get_string(p[0]));
  return value_na;
}

static value_t action_menuitemclear ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MenuClear");
  vm_param_check_string(vm, p, 0, "MenuClear");

  menu_item_remove(value_get_string(p[0]));
  return value_na;
}

static value_t action_menu ( vm_t *vm, value_t p[], gint np )
{
  guint16 state;

  vm_param_check_np(vm, np, 1, "Menu");
  vm_param_check_string(vm, p, 0, "Menu");

  state = VM_WSTATE(vm);
  menu_popup(base_widget_from_id(vm->store, VM_WIDGET(vm)),
      menu_from_name(value_get_string(p[0])), vm->event,
      VM_WINDOW(vm), &state);

  return value_na;
}

static value_t action_mpd ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MpdCmd");
  vm_param_check_string(vm, p, 0, "MpdCmd");

  client_mpd_command(value_get_string(p[0]));

  return value_na;
}

static value_t action_config ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "Config");
  vm_param_check_string(vm, p, 0, "Config");

  g_debug("parsing config string: %s", value_get_string(p[0]));
  config_parse_data("config string", value_get_string(p[0]), NULL, vm->store,
      ((guint8 *)g_bytes_get_data(vm->bytes, NULL))[0]);

  return value_na;
}

static value_t action_map_icon ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MapIcon");
  vm_param_check_string(vm, p, 0, "MapIcon");
  vm_param_check_string(vm, p, 1, "MapIcon");

  app_icon_map_add(value_get_string(p[0]), value_get_string(p[1]));

  return value_na;
}

static value_t action_setmonitor ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetMonitor is deprecated, please use monitor property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetMonitor");
  vm_param_check_string(vm, p, np-1, "SetMonitor");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_monitor(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_monitor);

  return value_na;
}

static value_t action_setlayer ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetLayer is deprecated, please use layer property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetLayer");
  vm_param_check_string(vm, p, np-1, "SetLayer");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_layer(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_layer);

  return value_na;
}

static value_t action_setmirror ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetMirror is deprecated, please use mirror property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetMirror");
  vm_param_check_string(vm, p, np-1, "SetMirror");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_mirrors_old(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_mirrors_old);

  return value_na;
}

static value_t action_setbarsize ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetBarSize is deprecated, please use size property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetBarSize");
  vm_param_check_string(vm, p, np-1, "SetBarSize");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_size(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_size);

  return value_na;
}

static value_t action_setbarmargin ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;

  g_message("SetBarMargin is deprecated, please use margin property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetBarMargin");
  vm_param_check_string(vm, p, np-1, "SetBarMargin");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    g_object_set(G_OBJECT(bar), "margin",
        (gint)g_ascii_strtoll(value_get_string(p[1]), NULL, 10), NULL);
  else if( (list = bar_get_list()) )
  {
    g_hash_table_iter_init(&iter, list);
    while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&bar))
      g_object_set(G_OBJECT(bar), "margin",
          (gint)g_ascii_strtoll(value_get_string(p[1]), NULL, 10), NULL);
  }

  return value_na;
}

static value_t action_setbarid ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetBarID is deprecated, please use sway_bar_id property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarID");
  vm_param_check_string(vm, p, np-1, "SetBarID");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_id(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_id);

  return value_na;
}

static value_t action_setbarsensor ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;
  gint64 timeout;

  g_message("SetBarSensor is deprecated, please use sensor property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarSensor");
  vm_param_check_string(vm, p, np-1, "SetBarSensor");

  timeout = g_ascii_strtoll(value_get_string(p[np-1]), NULL, 10);

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_sensor(bar, timeout);
  else if( (list = bar_get_list()) )
  {
    g_hash_table_iter_init(&iter, list);
    while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&bar))
      bar_set_sensor(bar, timeout);
  }

  return value_na;
}

static value_t action_setexclusivezone ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_message("SetExclusiveSone is deprecated, please use"
     " exclusive_zone property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetExclusiveZone");
  vm_param_check_string(vm, p, np-1, "SetExclusiveZone");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    g_object_set(G_OBJECT(bar), "exclusive_zone", value_get_string(p[1]),NULL);
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_exclusive_zone);

  return value_na;
}

static value_t action_setbarvisibility ( vm_t *vm, value_t p[], gint np )
{
  g_message("SetBarVisibility is deprecated, please use"
     " exclusive_zone property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarVisibility");
  vm_param_check_string(vm, p, np-1, "SetBarVisibility");

  bar_set_visibility((np==2)? bar_from_name(value_get_string(p[0])) : NULL,
      NULL, *value_get_string(p[np-1]));

  return value_na;
}

static value_t action_setcode ( vm_t *vm, value_t p[], gint np, gchar *func,
    gchar *prop )
{
  GtkWidget *widget;
  GBytes *code;
  guint8 *mark, api2 = TRUE;

  vm_param_check_np_range(vm, np, 1, 2, func);
  vm_param_check_string(vm, p, 0, func);

  if(!vm->pstack->len)
    return value_na;

  mark = vm->pstack->pdata[vm->pstack->len-1];
  widget = base_widget_from_id(vm->store, np==2? value_get_string(p[0]) :
      VM_WIDGET(vm));
  if(!widget)
    return value_na;

  GByteArray *bytes = g_byte_array_sized_new(vm->ip-mark+1);
  g_byte_array_append(bytes, &api2, 1);
  g_byte_array_append(bytes, mark, vm->ip - mark);
  code = g_byte_array_free_to_bytes(bytes);
  g_object_set(G_OBJECT(widget), prop, code, NULL);
  g_bytes_unref(code);

  return value_na;
}

static value_t action_setvalue ( vm_t *vm, value_t p[], gint np )
{
  return action_setcode(vm, p, np, "SetValue", "value");
}

static value_t action_setstyle ( vm_t *vm, value_t p[], gint np )
{
  return action_setcode(vm, p, np, "SetStyle", "style");
}

static value_t action_settooltip ( vm_t *vm, value_t p[], gint np )
{
  return action_setcode(vm, p, np, "SetTooltip", "tooltip");
}

static value_t action_userstate ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  gchar *state, *value;
  guint16 mask;

  vm_param_check_np_range(vm, np, 1, 2, "UserState");
  vm_param_check_string(vm, p, 0, "UserState");
  if(np==2)
    vm_param_check_string(vm, p, 1, "UserState");

  widget = base_widget_from_id(vm->store, np==2? value_get_string(p[0]) :
      VM_WIDGET(vm));

  if(!widget || !(value = value_get_string(p[np-1])) )
    return value_na;

  if( !(state = strchr(value , ':')) )
  {
    state = value;
    mask = WS_USERSTATE;
  }
  else
  {
    state++;
    mask = g_ascii_digit_value(*value)==2? WS_USERSTATE2 : WS_USERSTATE;
  }

  base_widget_set_state(widget, mask, !g_ascii_strcasecmp(state, "on"));

  return value_na;
}

static value_t action_popup ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "Popup");
  vm_param_check_string(vm, p, 0, "Popup");

  popup_trigger(base_widget_from_id(vm->store, VM_WIDGET(vm)),
        value_get_string(p[0]), vm->event);

  return value_na;
}

static value_t action_client_send ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ClientSend");
  vm_param_check_string(vm, p, 0, "ClientSend");
  vm_param_check_string(vm, p, 1, "ClientSend");

  client_send(value_get_string(p[0]), value_get_string(p[1]));

  return value_na;
}

static window_t *action_window_get ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;

  widget = base_widget_from_id(vm->store,
      np==1? value_get_string(p[0]) : VM_WIDGET(vm));

  if(widget && IS_TASKBAR_ITEM(widget))
    return flow_item_get_source(widget);
  else
    return wintree_from_id(VM_WINDOW(vm));
}

static value_t action_focus ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_focus(win->uid);

  return value_na;
}

static value_t action_close ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_close(win->uid);

  return value_na;
}

static value_t action_minimize ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_minimize(win->uid);

  return value_na;
}

static value_t action_maximize ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_maximize(win->uid);

  return value_na;
}

static value_t action_unminimize ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_unminimize(win->uid);

  return value_na;
}

static value_t action_unmaximize ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if( (win = action_window_get(vm, p, np)) )
    wintree_unmaximize(win->uid);

  return value_na;
}

static value_t action_eval ( vm_t *vm, value_t p[], gint np )
{
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE], *ptr;

  vm_param_check_np(vm, np, 2, "Eval");
  vm_param_check_string(vm, p, 0, "Eval");

  if(value_is_string(p[1]))
    ptr = p[1].value.string;
  else
    ptr = g_ascii_dtostr(buf, G_ASCII_DTOSTR_BUF_SIZE,value_get_numeric(p[1]));

  if(!ptr)
    return value_na;

  scanner_var_new(value_get_string(p[0]), NULL,
      (gchar *)parser_string_build(ptr), G_TOKEN_SET, VT_FIRST, vm->store);

  return value_na;
}

static value_t action_switcher ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 0, 1, "SwitcherEvent");

  if(np==0 || !g_strcasecmp(value_get_string(p[0]), "forward"))
    trigger_emit("switcher_forward");
  else if(np==1 && !g_strcasecmp(value_get_string(p[0]), "back"))
    trigger_emit("switcher_back");

  return value_na;
}

static value_t action_clear_widget ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *w;

  vm_param_check_np(vm, np, 1, "ClearWidget");
  vm_param_check_string(vm, p, 0, "ClearWidget");

  if( (w = base_widget_from_id(vm->store, value_get_string(p[0]))) )
    gtk_widget_destroy(w);

  return value_na;
}

static value_t action_taskbar_item ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;
  if( !(win = wintree_from_id(VM_WINDOW(vm))) )
    return value_na;
  if (wintree_is_focused(win->uid) && !(win->state & WS_MINIMIZED))
    wintree_minimize(win->uid);
  else
    wintree_focus(win->uid);

  return value_na;
}

static value_t action_workspace_activate ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;

  widget = base_widget_from_id(vm->store,
      np==1? value_get_string(p[0]) : VM_WIDGET(vm));

  if(widget && IS_PAGER_ITEM(widget))
    workspace_activate(flow_item_get_source(widget));

  return value_na;
}

static value_t action_check_state ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  window_t *win;
  guint16 cond, ncond;

  vm_param_check_np(vm, np, 2, "");
  vm_param_check_numeric(vm, p, 0, "CheckState");
  vm_param_check_numeric(vm, p, 1, "CheckState");

  widget = base_widget_from_id(vm->store, VM_WIDGET(vm));
  win = wintree_from_id(VM_WINDOW(vm));

  cond = (guint16)value_get_numeric(p[0]) & ~WS_CHILDREN;
  ncond = (guint16)value_get_numeric(p[1]) & ~WS_CHILDREN;

  if(((cond & 0x0f) || (ncond & 0x0f)) && !win)
    return value_new_numeric(FALSE);
  if(((cond & 0xf0) || (ncond & 0xf0)) && !widget )
    return value_new_numeric(FALSE);
  if((VM_WSTATE(vm) & cond) != cond)
    return value_new_numeric(FALSE);
  if((~VM_WSTATE(vm) & ncond) != ncond)
    return value_new_numeric(FALSE);

  return value_new_numeric(TRUE);
}

static void action_file_trigger_cb ( GFileMonitor *m, GFile *f1, GFile *f2,
    GFileMonitorEvent ev, gchar *trigger )
{
  g_source_remove_by_user_data(trigger);
  g_debug("FileTrigger: emit '%s'", trigger);
  trigger_emit(trigger);
}

static gboolean action_file_timeout_cb ( gchar *trigger )
{
  g_debug("FileTrigger: timeout emit '%s'", trigger);
  trigger_emit(trigger);

  return TRUE;
}

static value_t action_file_trigger ( vm_t *vm, value_t p[], gint np )
{
  GFile *f;
  GFileMonitor *m;

  vm_param_check_np_range(vm, np, 2, 3, "FileTrigger");
  vm_param_check_string(vm, p, 0, "FileTrigger");
  vm_param_check_string(vm, p, 1, "FileTrigger");
  if(np==3)
    vm_param_check_numeric(vm, p, 2, "FileTrigger");

  f = g_file_new_for_path(value_get_string(p[0]));
  if( !(m = g_file_monitor_file(f, 0, NULL, NULL)) )
  {
    g_debug("FileTrigger: unable to setup a file monitor for %s",
        value_get_string(p[0]));
    g_object_unref(f);
    return value_na;
  }
  if(np==3)
    g_timeout_add(value_get_numeric(p[2]), (GSourceFunc)action_file_timeout_cb,
        (gpointer)trigger_name_intern(value_get_string(p[1])));
  g_object_weak_ref(G_OBJECT(m), (GWeakNotify)g_object_unref, f);
  g_signal_connect(G_OBJECT(m), "changed", G_CALLBACK(action_file_trigger_cb),
      (gpointer)trigger_name_intern(value_get_string(p[1])));
  g_debug("FileTrigger: subscribe to '%s', trigger '%s'",
      value_get_string(p[0]), value_get_string(p[1]));
  return value_na;
}

static value_t action_emit_trigger ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "");
  vm_param_check_string(vm, p, 0, "EmitTrigger");

  g_main_context_invoke(NULL, (GSourceFunc)trigger_emit,
      (gchar *)trigger_name_intern(value_get_string(p[0])));

  return value_na;
}

static value_t action_exit ( vm_t *vm, value_t p[], gint np )
{
  exit(1);
}

static value_t action_update_widget ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;

  vm_param_check_np_range(vm, np, 0, 1, "UpdateWidget");
  if(np==1)
    vm_param_check_string(vm, p, 0, "UpdateWidget");

  if( (widget = base_widget_from_id(vm->store,
          np? value_get_string(p[0]) : VM_WIDGET(vm))) )
    base_widget_update_expressions(widget);

  return value_na;
}

static value_t action_print ( vm_t *vm, value_t p[], gint np )
{
  gchar *str;

  vm_param_check_np(vm, np, 1, "Print");

  str = value_to_string(p[0], -1);
  g_message("%s", str);
  g_free(str);

  return value_na;
}

static value_t action_usleep ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "uSleep");

  g_usleep(value_as_numeric(p[0]));

  return value_na;
}


void action_lib_init ( void )
{
  vm_func_add("exec", action_exec_impl, TRUE, TRUE);
  vm_func_add("function", action_function, TRUE, FALSE);
  vm_func_add("piperead", action_piperead, TRUE, FALSE);
  vm_func_add("menuclear", action_menuclear, TRUE, FALSE);
  vm_func_add("menuitemclear", action_menuitemclear, TRUE, FALSE);
  vm_func_add("menu", action_menu, TRUE, FALSE);
  vm_func_add("mpdcmd", action_mpd, TRUE, TRUE);
  vm_func_add("config", action_config, TRUE, FALSE);
  vm_func_add("mapicon", action_map_icon, TRUE, TRUE);
  vm_func_add("popup", action_popup, TRUE, FALSE);
  vm_func_add("setmonitor", action_setmonitor, TRUE, FALSE);
  vm_func_add("setlayer", action_setlayer, TRUE, FALSE);
  vm_func_add("setmirror", action_setmirror, TRUE, FALSE);
  vm_func_add("setbarsize", action_setbarsize, TRUE, FALSE);
  vm_func_add("setbarmargin", action_setbarmargin, TRUE, FALSE);
  vm_func_add("setbarid", action_setbarid, TRUE, FALSE);
  vm_func_add("setbarsensor", action_setbarsensor, TRUE, FALSE);
  vm_func_add("setbarvisibility", action_setbarvisibility, TRUE, FALSE);
  vm_func_add("setexclusivezone", action_setexclusivezone, TRUE, FALSE);
  vm_func_add("userstate", action_userstate, TRUE, FALSE);
  vm_func_add("setvalue", action_setvalue, TRUE, FALSE);
  vm_func_add("setstyle", action_setstyle, TRUE, FALSE);
  vm_func_add("settooltip", action_settooltip, TRUE, FALSE);
  vm_func_add("focus", action_focus, TRUE, TRUE);
  vm_func_add("close", action_close, TRUE, TRUE);
  vm_func_add("minimize", action_minimize, TRUE, TRUE);
  vm_func_add("maximize", action_maximize, TRUE, TRUE);
  vm_func_add("unminimize", action_unminimize, TRUE, TRUE);
  vm_func_add("unmaximize", action_unmaximize, TRUE, TRUE);
  vm_func_add("clientsend", action_client_send, TRUE, TRUE);
  vm_func_add("eval", action_eval, TRUE, FALSE);
  vm_func_add("switcherevent", action_switcher, TRUE, TRUE);
  vm_func_add("workspaceactivate", action_workspace_activate, TRUE, FALSE);
  vm_func_add("taskbaritemdefault", action_taskbar_item, TRUE, FALSE);
  vm_func_add("clearwidget", action_clear_widget, TRUE, FALSE);
  vm_func_add("checkstate", action_check_state, FALSE, FALSE);
  vm_func_add("filetrigger", action_file_trigger, FALSE, FALSE);
  vm_func_add("emittrigger", action_emit_trigger, FALSE, TRUE);
  vm_func_add("exit", action_exit, TRUE, TRUE);
  vm_func_add("UpdateWidget", action_update_widget, TRUE, FALSE);
  vm_func_add("Print", action_print, TRUE, TRUE);
  vm_func_add("uSleep", action_usleep, TRUE, TRUE);
}
