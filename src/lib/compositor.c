/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "input.h"
#include "vm/vm.h"
#include "gui/taskbaritem.h"
#include "gui/pageritem.h"

static gpointer lib_compositor_window_get ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget = vm_widget_get(vm, np==1? value_get_string(p[0]) : NULL);
  window_t *win;

  if(widget && IS_TASKBAR_ITEM(widget))
    win = flow_item_get_source(widget);
  else
    win = wintree_from_id(VM_WINDOW(vm));

  return win? win->uid : NULL;
}

static value_t lib_compositor_focus ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_focus(wid);

  return value_na;
}

static value_t lib_compositor_close ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_close(wid);

  return value_na;
}

static value_t lib_compositor_minimize ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_minimize(wid);

  return value_na;
}

static value_t lib_compositor_maximize ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_maximize(wid);

  return value_na;
}

static value_t lib_compositor_unminimize ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_unminimize(wid);

  return value_na;
}

static value_t lib_compositor_unmaximize ( vm_t *vm, value_t p[], gint np )
{
  gpointer wid;

  if( (wid = lib_compositor_window_get(vm, p, np)) )
    wintree_unmaximize(wid);

  return value_na;
}

static value_t lib_compositor_lib_active ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(wintree_get_active());
}

static value_t lib_compositor_ws_activate ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;

  widget = vm_widget_get(vm, np==1? value_get_string(p[0]) : NULL);

  if(widget && IS_PAGER_ITEM(widget))
    workspace_activate(flow_item_get_source(widget));

  return value_na;
}

static value_t lib_compositor_lib_window_info ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  window_t *win;

  vm_param_check_np_range(vm, np, 1, 2, "WindowInfo");
  vm_param_check_string(vm, p, 0, "WindowInfo");
  if(np==2)
    vm_param_check_string(vm, p, 1, "WindowInfo");

  widget = vm_widget_get(vm, np==2? value_get_string(p[0]) : NULL);

  if( (win = flow_item_get_source(widget)) )
  {
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "appid"))
      return value_new_string(win->appid);
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "title"))
      return value_new_string(win->title);
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "minimized"))
      return value_new_numeric(!!(win->state & WS_MINIMIZED));
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "maximized"))
      return value_new_numeric(!!(win->state & WS_MAXIMIZED));
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "fullscreen"))
      return value_new_numeric(!!(win->state & WS_FULLSCREEN));
    if(!g_ascii_strcasecmp(value_get_string(p[np-1]), "focused"))
      return value_new_numeric(wintree_is_focused(win->uid));
  }

  return value_na;
}

static value_t lib_compositor_custom_ipc ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(wintree_get_custom_ipc());
}

static value_t lib_compositor_layout ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(input_layout_get());
}

static value_t lib_compositor_layout_list ( vm_t *vm, value_t p[], gint np )
{
  return value_array_from_strv(input_layout_list_get());
}

static value_t lib_compositor_set_layout ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "SetLayout");
  vm_param_check_string(vm, p, 0, "SetLayout");

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "prev"))
    input_layout_prev();
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "next"))
    input_layout_next();
  else
    input_layout_change(value_get_string(p[0]));

  return value_na;
}

static value_t lib_compositor_set_disown ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "SetDisownMinimzied");
  vm_param_check_string(vm, p, 0, "SetDisownMinimized");

  wintree_set_disown(!!value_get_numeric(p[0]));

  return value_na;
}

void lib_compositor_init ( void )
{
  vm_func_add("focus", lib_compositor_focus, TRUE, FALSE);
  vm_func_add("close", lib_compositor_close, TRUE, FALSE);
  vm_func_add("minimize", lib_compositor_minimize, TRUE, FALSE);
  vm_func_add("maximize", lib_compositor_maximize, TRUE, FALSE);
  vm_func_add("unminimize", lib_compositor_unminimize, TRUE, FALSE);
  vm_func_add("unmaximize", lib_compositor_unmaximize, TRUE, FALSE);
  vm_func_add("activewin", lib_compositor_lib_active, FALSE, FALSE);
  vm_func_add("workspaceactivate", lib_compositor_ws_activate, TRUE, FALSE);
  vm_func_add("windowinfo", lib_compositor_lib_window_info, FALSE, FALSE);
  vm_func_add("customipc", lib_compositor_custom_ipc, FALSE, TRUE);
  vm_func_add("layout", lib_compositor_layout, FALSE, TRUE);
  vm_func_add("layoutlist", lib_compositor_layout_list, FALSE, TRUE);
  vm_func_add("setlayout", lib_compositor_set_layout, TRUE, TRUE);
  vm_func_add("setdisownminized", lib_compositor_set_disown, TRUE, TRUE);
}
