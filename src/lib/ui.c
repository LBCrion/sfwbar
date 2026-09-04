/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- sfwbar maintainers
 */

#include "config/config.h"
#include "trigger.h"
#include "vm/vm.h"
#include "gui/bar.h"
#include "gui/basewidget.h"
#include "gui/menu.h"
#include "gui/menuitem.h"
#include "gui/popup.h"
#include "gui/toplevel.h"
#include "util/string.h"

static value_t lib_ui_menuclear ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MenuClear");
  vm_param_check_string(vm, p, 0, "MenuClear");

  menu_remove(value_get_string(p[0]));
  return value_na;
}

static value_t lib_ui_menuitemclear ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "MenuItemClear");
  vm_param_check_string(vm, p, 0, "MenuItemClear");

  menu_item_remove(value_get_string(p[0]));
  return value_na;
}

static value_t lib_ui_menu ( vm_t *vm, value_t p[], gint np )
{
  guint16 state;

  vm_param_check_np(vm, np, 1, "Menu");
  vm_param_check_string(vm, p, 0, "Menu");

  g_debug("menu: popup '%s' at '%s' (%p)", value_get_string(p[0]),
      VM_WIDGET(vm), menu_from_name(value_get_string(p[0])));
  state = VM_WSTATE(vm);
  menu_popup(vm_widget_get(vm, NULL), menu_from_name(value_get_string(p[0])),
      vm->event, VM_WINDOW(vm), &state);

  return value_na;
}

static value_t lib_ui_clear_widget ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *w;

  if( (w = vm_widget_get(vm, np? value_get_string(p[0]) : NULL)) )
    gtk_widget_destroy(w);

  return value_na;
}

static value_t lib_ui_popup ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "Popup");
  vm_param_check_string(vm, p, 0, "Popup");

  popup_trigger(vm_widget_get(vm, NULL), value_get_string(p[0]), vm->event);

  return value_na;
}

static value_t lib_ui_window_open ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WindowOpen");
  vm_param_check_string(vm, p, 0, "WindowOpen");

  toplevel_show(value_get_string(p[0]));

  return value_na;
}

static value_t lib_ui_window_close ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WindowClose");
  vm_param_check_string(vm, p, 0, "WindowClose");

  toplevel_hide(value_get_string(p[0]));

  return value_na;
}

static value_t lib_ui_setmonitor ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetMonitor is deprecated, please use monitor property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetMonitor");
  vm_param_check_string(vm, p, np-1, "SetMonitor");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_monitor(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_monitor);

  return value_na;
}

static value_t lib_ui_setlayer ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetLayer is deprecated, please use layer property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetLayer");
  vm_param_check_string(vm, p, np-1, "SetLayer");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_layer(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_layer);

  return value_na;
}

static value_t lib_ui_setmirror ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetMirror is deprecated, please use mirror property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetMirror");
  vm_param_check_string(vm, p, np-1, "SetMirror");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_mirrors_old(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_mirrors_old);

  return value_na;
}

static value_t lib_ui_setbarsize ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetBarSize is deprecated, please use size property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetBarSize");
  vm_param_check_string(vm, p, np-1, "SetBarSize");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_size(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_size);

  return value_na;
}

static value_t lib_ui_setbarmargin ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;

  g_warning("SetBarMargin is deprecated, please use margin property instead");

  vm_param_check_np_range(vm, np, 1, 2, "SetBarMargin");
  vm_param_check_string(vm, p, np-1, "SetBarMargin");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    g_object_set(G_OBJECT(bar), "margin",
        (gint)str_ascii_toll(value_get_string(p[1]), NULL, 10), NULL);
  else if( (list = bar_get_list()) )
  {
    g_hash_table_iter_init(&iter, list);
    while(g_hash_table_iter_next(&iter, NULL, (gpointer *)&bar))
      g_object_set(G_OBJECT(bar), "margin",
          (gint)str_ascii_toll(value_get_string(p[1]), NULL, 10), NULL);
  }

  return value_na;
}

static value_t lib_ui_setbarid ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetBarID is deprecated, please use sway_bar_id property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarID");
  vm_param_check_string(vm, p, np-1, "SetBarID");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    bar_set_id(bar, value_get_string(p[1]));
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_id);

  return value_na;
}

static value_t lib_ui_setbarsensor ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;
  GHashTable *list;
  GHashTableIter iter;
  gint64 timeout;

  g_warning("SetBarSensor is deprecated, please use sensor property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarSensor");
  vm_param_check_string(vm, p, np-1, "SetBarSensor");

  timeout = str_ascii_toll(value_get_string(p[np-1]), NULL, 10);

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

static value_t lib_ui_setexclusivezone ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *bar;

  g_warning("SetExclusiveSone is deprecated, please use"
     " exclusive_zone property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetExclusiveZone");
  vm_param_check_string(vm, p, np-1, "SetExclusiveZone");

  if( np==2 && (bar = bar_from_name(value_get_string(p[0]))) )
    g_object_set(G_OBJECT(bar), "exclusive_zone", value_get_string(p[1]),NULL);
  else
    bar_address_all(NULL, value_get_string(p[np-1]), bar_set_exclusive_zone);

  return value_na;
}

static value_t lib_ui_setbarvisibility ( vm_t *vm, value_t p[], gint np )
{
  g_warning("SetBarVisibility is deprecated, please use"
     " exclusive_zone property instead");
  vm_param_check_np_range(vm, np, 1, 2, "SetBarVisibility");
  vm_param_check_string(vm, p, np-1, "SetBarVisibility");

  bar_set_visibility((np==2)? bar_from_name(value_get_string(p[0])) : NULL,
      NULL, *value_get_string(p[np-1]));

  return value_na;
}

static value_t lib_ui_bardir ( vm_t *vm, value_t p[], gint np )
{
  switch(bar_get_toplevel_dir(vm_widget_get(vm, NULL)))
  {
    case GTK_POS_RIGHT:
      return value_new_string("right");
    case GTK_POS_LEFT:
      return value_new_string("left");
    case GTK_POS_TOP:
      return value_new_string("top");
    case GTK_POS_BOTTOM:
      return value_new_string("bottom");
    default:
      return value_new_string("unknown");
  }
}

static value_t lib_ui_gtkevent ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *base, *widget;
  GtkAllocation alloc;
  GtkStyleContext *style;
  GtkBorder margin, padding, border;
  gint x, y, w, h, dir;
  gdouble result, rawx, rawy;

  vm_param_check_np(vm, np, 1, "GtkEvent");
  vm_param_check_string(vm, p, 0, "GtkEvent");

  if( !(base = vm_widget_get(vm, NULL)) )
    return value_na;

  gdk_event_get_coords(vm->event, &rawx, &rawy);

  if(GTK_IS_BIN(base))
  {
    widget = gtk_bin_get_child(GTK_BIN(base));
    gtk_widget_translate_coordinates(base, widget, rawx, rawy, &x, &y);
  }
  else
  {
    widget = base;
    x = rawx;
    y = rawy;
  }

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "x"))
    dir = GTK_POS_RIGHT;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "y"))
    dir = GTK_POS_BOTTOM;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "dir"))
    gtk_widget_style_get(widget, "direction", &dir, NULL);
  else
    return value_na;

  gtk_widget_get_allocation(widget, &alloc);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
      &margin);
  gtk_style_context_get_padding(style, gtk_style_context_get_state(style),
      &padding);
  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
      &border);
  w = alloc.width - margin.left - margin.right - padding.left -
    padding.right - border.left - border.right;
  h = alloc.height - margin.top - margin.bottom - padding.top -
    padding.bottom - border.top - border.bottom;

  x = x - margin.left - padding.left - border.left;
  y = y - margin.top - padding.top - border.top;

  if(dir==GTK_POS_RIGHT || dir==GTK_POS_LEFT)
    result = CLAMP((gdouble)x / w, 0, 1);
  else
    result = CLAMP((gdouble)y / h, 0, 1);
  if(dir==GTK_POS_LEFT || dir==GTK_POS_TOP)
    result = 1.0 - result;

  return value_new_numeric(result);
}

static value_t lib_ui_widget_id ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(VM_WIDGET(vm));
}

static value_t lib_ui_get_widget_state ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  gint state;

  vm_param_check_np_range(vm, np, 1, 2, "GetWidgetState");
  if(np==2)
    vm_param_check_string(vm, p, 0, "GetWidgetState");

  widget = vm_widget_get(vm, np==2? value_get_string(p[0]) : NULL);
  if(!IS_BASE_WIDGET(widget))
    return value_na;

  state = base_widget_state_build(widget, NULL);

  if(value_as_numeric(p[np-1])==1)
    return value_new_numeric(state & WS_USERSTATE);
  if(value_as_numeric(p[np-1])==2)
    return value_new_numeric(state & WS_USERSTATE2);

  return value_na;
}

static value_t lib_ui_set_widget_state ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  gchar *state, *value;
  guint16 mask;

  vm_param_check_np_range(vm, np, 1, 2, "SetWidgetState");
  vm_param_check_string(vm, p, 0, "SetWidgetState");
  if(np==2)
    vm_param_check_string(vm, p, 1, "SetWidgetState");

  if( !(widget = vm_widget_get(vm, np==2? value_get_string(p[0]) : NULL)) )
    return value_na;

  if(!(value = value_get_string(p[np-1])) )
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

static value_t lib_ui_check_state ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  window_t *win;
  guint16 cond, ncond;

  vm_param_check_np(vm, np, 2, "");
  vm_param_check_numeric(vm, p, 0, "CheckState");
  vm_param_check_numeric(vm, p, 1, "CheckState");

  widget = vm_widget_get(vm, NULL);
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

static value_t lib_ui_widget_children ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  GList *children, *iter;
  value_t array;

  vm_param_check_np_range(vm, np, 0, 1, "WidgetChildren");

  widget = vm_widget_get(vm, np? value_get_string(p[0]) : NULL);

  if(!IS_BASE_WIDGET(widget))
    return value_array_create(0);

  children = gtk_container_get_children(GTK_CONTAINER(base_widget_get_child(
          widget)));

  array = value_array_create(g_list_length(children)); 
  for(iter=children; iter; iter=g_list_next(iter))
    if(IS_BASE_WIDGET(iter->data))
      value_array_append(array,
          value_new_string(base_widget_get_id(iter->data)));
  g_list_free(children);

  return array;
}

static value_t lib_ui_widget_get_data ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  value_t *vptr;

  vm_param_check_np_range(vm, np, 1, 2, "WidgetrGetData");
  vm_param_check_string(vm, p, 0, "WidgetGetData");
  if(np==2)
    vm_param_check_string(vm, p, 1, "WidgetGetData");

  if( !(widget = vm_widget_get(vm, np==2? value_get_string(p[0]) : NULL)) )
    return value_na;

  if( !(vptr = g_object_get_data(G_OBJECT(widget),
          value_get_string(p[np==2? 1: 0]))) )
    return value_na;

  return value_dup(*vptr);
}

static value_t lib_ui_widget_set_data ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  value_t v1;

  vm_param_check_np_range(vm, np, 2, 3, "WidgetSetData");
  vm_param_check_string(vm, p, 0, "WidgetSetData");
  if(np==3)
    vm_param_check_string(vm, p, 1, "WidgetSetData");

  if( !(widget = vm_widget_get(vm, np==3? value_get_string(p[0]) : NULL)) )
    return value_na;

  v1 = value_dup(p[np==2? 1 : 2]);
  g_object_set_data_full(G_OBJECT(widget), value_get_string(p[np==2? 0 : 1]),
      g_memdup2(&v1, sizeof(value_t)), (GDestroyNotify)value_free_ptr_full);

  return value_na;
}

static value_t lib_ui_update_widget ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;

  vm_param_check_np_range(vm, np, 0, 1, "UpdateWidget");
  if(np==1)
    vm_param_check_string(vm, p, 0, "UpdateWidget");

  if( (widget = vm_widget_get(vm, np? value_get_string(p[0]) : NULL)) )
    base_widget_update_expressions(widget);

  return value_na;
}

static value_t lib_ui_widget_push ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "WidgetPush");
  vm_param_check_string(vm, p, 0, "WidgetPush");

  vm_target_push(vm, value_get_string(p[0]), NULL, NULL, NULL);
  return value_na;
}

static value_t lib_ui_widget_pop ( vm_t *vm, value_t p[], gint np )
{
  vm_target_pop(vm);
  return value_na;
}

static value_t lib_ui_setcode ( vm_t *vm, value_t p[], gint np, gchar *func,
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
  if( !(widget = vm_widget_get(vm, np==2? value_get_string(p[0]) : NULL)) )
    return value_na;

  GByteArray *bytes = g_byte_array_sized_new(vm->ip-mark+1);
  g_byte_array_append(bytes, &api2, 1);
  g_byte_array_append(bytes, mark, vm->ip - mark);
  code = g_byte_array_free_to_bytes(bytes);
  g_object_set(G_OBJECT(widget), prop, code, NULL);
  g_bytes_unref(code);

  return value_na;
}

static value_t lib_ui_setvalue ( vm_t *vm, value_t p[], gint np )
{
  return lib_ui_setcode(vm, p, np, "SetValue", "value");
}

static value_t lib_ui_setstyle ( vm_t *vm, value_t p[], gint np )
{
  return lib_ui_setcode(vm, p, np, "SetStyle", "style");
}

static value_t lib_ui_settooltip ( vm_t *vm, value_t p[], gint np )
{
  return lib_ui_setcode(vm, p, np, "SetTooltip", "tooltip");
}

void lib_ui_init ( void )
{
  vm_func_add("menuclear", lib_ui_menuclear, TRUE, FALSE);
  vm_func_add("menuitemclear", lib_ui_menuitemclear, TRUE, FALSE);
  vm_func_add("menu", lib_ui_menu, TRUE, FALSE);
  vm_func_add("clearwidget", lib_ui_clear_widget, TRUE, FALSE);
  vm_func_add("popup", lib_ui_popup, TRUE, FALSE);
  vm_func_add("windowopen", lib_ui_window_open, TRUE, FALSE);
  vm_func_add("windowclose", lib_ui_window_close, TRUE, FALSE);
  vm_func_add("setmonitor", lib_ui_setmonitor, TRUE, FALSE);
  vm_func_add("setlayer", lib_ui_setlayer, TRUE, FALSE);
  vm_func_add("setmirror", lib_ui_setmirror, TRUE, FALSE);
  vm_func_add("setbarsize", lib_ui_setbarsize, TRUE, FALSE);
  vm_func_add("setbarmargin", lib_ui_setbarmargin, TRUE, FALSE);
  vm_func_add("setbarid", lib_ui_setbarid, TRUE, FALSE);
  vm_func_add("setbarsensor", lib_ui_setbarsensor, TRUE, FALSE);
  vm_func_add("setbarvisibility", lib_ui_setbarvisibility, TRUE, FALSE);
  vm_func_add("setexclusivezone", lib_ui_setexclusivezone, TRUE, FALSE);
  vm_func_add("bardir", lib_ui_bardir, FALSE, FALSE);
  vm_func_add("gtkevent", lib_ui_gtkevent, FALSE, FALSE);
  vm_func_add("widgetid", lib_ui_widget_id, FALSE, FALSE);
  vm_func_add("checkstate", lib_ui_check_state, FALSE, FALSE);
  vm_func_add("getwidgetstate", lib_ui_get_widget_state, FALSE, FALSE);
  vm_func_add("setwidgetstate", lib_ui_set_widget_state, TRUE, FALSE);
  vm_func_add("widgetchildren", lib_ui_widget_children, FALSE, FALSE);
  vm_func_add("widgetgetdata", lib_ui_widget_get_data, FALSE, FALSE);
  vm_func_add("widgetsetdata", lib_ui_widget_set_data, TRUE, FALSE);
  vm_func_add("updatewidget", lib_ui_update_widget, TRUE, FALSE);
  vm_func_add("widgetpush", lib_ui_widget_push, TRUE, FALSE);
  vm_func_add("widgetpop", lib_ui_widget_pop, TRUE, FALSE);
  vm_func_add("setvalue", lib_ui_setvalue, TRUE, FALSE);
  vm_func_add("setstyle", lib_ui_setstyle, TRUE, FALSE);
  vm_func_add("settooltip", lib_ui_settooltip, TRUE, FALSE);

  /* deprecated aliases */
  vm_func_add("widgetstate", lib_ui_get_widget_state, FALSE, FALSE);
  vm_func_add("userstate", lib_ui_set_widget_state, TRUE, FALSE);
}
