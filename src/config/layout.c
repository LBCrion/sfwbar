/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "gui/css.h"
#include "gui/bar.h"
#include "gui/button.h"
#include "gui/scale.h"
#include "gui/image.h"
#include "gui/label.h"
#include "gui/cchart.h"
#include "gui/grid.h"
#include "gui/taskbarshell.h"
#include "gui/taskbar.h"
#include "gui/pager.h"
#include "gui/popup.h"
#include "gui/tray.h"
#include "vm/vm.h"

void config_widget (GScanner *scanner, GtkWidget *widget);

GdkRectangle config_get_loc ( GScanner *scanner )
{
  GdkRectangle rect = { .x=0, .y=0, .width=1, .height=1 };

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "missing '(' after loc",
      SEQ_REQ, G_TOKEN_INT, NULL, &rect.x, "missing x value in loc",
      SEQ_REQ, ',', NULL, NULL, "missing comma after x value in loc",
      SEQ_REQ, G_TOKEN_INT, NULL, &rect.y, "missing y value in loc",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_INT, NULL, &rect.width, "missing w value in loc",
      SEQ_OPT, ',', NULL, NULL, NULL,
      SEQ_CON, G_TOKEN_INT, NULL, &rect.height, "missing h value in loc",
      SEQ_REQ, ')', NULL, NULL, "missing ')' in loc statement",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END );

  return rect;
}

gboolean config_action_mods ( GScanner *scanner, gint *mods )
{
  gpointer mod;

  while( (mod = config_lookup_next_ptr(scanner, config_mods)) )
  {
    g_scanner_get_next_token(scanner);
    *mods |= GPOINTER_TO_INT(mod);

    if(!config_check_and_consume(scanner, '+'))
      return FALSE;
  }
  return TRUE;
}

gboolean config_action_slot ( GScanner *scanner, gint *slot )
{
  gint token;

  g_scanner_get_next_token(scanner);
  if(scanner->token == G_TOKEN_FLOAT && scanner->value.v_float >= 0 &&
      scanner->value.v_float <= BASE_WIDGET_MAX_ACTION)
    *slot = scanner->value.v_float;
  else if( (token = config_lookup_key(scanner, config_events)) )
    *slot = token;
  else
    return FALSE;

  return !(*slot<0 || *slot>BASE_WIDGET_MAX_ACTION );
}

void config_widget_action ( GScanner *scanner, GtkWidget *widget )
{
  gint slot = 1, mod = 0;
  GBytes *action = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT, '[', NULL, NULL, NULL,
      SEQ_CON, -2, config_action_mods, &mod, NULL,
      SEQ_CON, -2, config_action_slot, &slot, "invalid action slot",
      SEQ_CON, ']', NULL, NULL, "missing ']' after action",
      SEQ_REQ, '=', NULL, NULL, "missing '=' after action",
      SEQ_REQ, -2, config_action, &action, "missing action",
      SEQ_END);

  if(!scanner->max_parse_errors)
    base_widget_set_action(widget, slot, mod, action);
  else if(action)
    g_bytes_unref(action);
}

gboolean config_include ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  gchar *fname;

  if(scanner->token != G_TOKEN_IDENTIFIER ||
      (g_ascii_strcasecmp(scanner->value.v_identifier, "include") &&
      (!container || g_ascii_strcasecmp(scanner->value.v_identifier, "widget"))) )
    return FALSE;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "Missing '(' after include",
      SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing filename in include",
      SEQ_REQ, ')', NULL, NULL, "Missing ')',after include",
      SEQ_END);

  if(scanner->max_parse_errors)
  {
    g_free(fname);
    return TRUE;
  }

  widget = config_parse(fname, container, SCANNER_STORE(scanner));
  if(container)
  {
    config_widget(scanner, widget);
    grid_attach(container, widget);
    grid_mirror_child(container, widget);
    css_widget_cascade(widget, NULL);
  }
  g_free(fname);

  return TRUE;
}

gboolean config_flowgrid_property ( GScanner *scanner, GtkWidget *widget )
{
  gint key;

  if(!IS_FLOW_GRID(widget))
    return FALSE;
  if( !(key = config_lookup_key(scanner, config_flowgrid_props)) )
    return FALSE;

  switch(key)
  {
    case G_TOKEN_COLS:
      flow_grid_set_cols(widget, config_assign_number(scanner, "cols"));
      return TRUE;
    case G_TOKEN_ROWS:
      flow_grid_set_rows(widget, config_assign_number(scanner, "rows"));
      return TRUE;
    case G_TOKEN_PRIMARY:
      flow_grid_set_primary(widget, GPOINTER_TO_INT(config_assign_tokens(
              scanner, config_axis_keys,
              "Invalid value in 'primary = rows|cols'")));
      return TRUE;
    case G_TOKEN_ICONS:
      flow_grid_set_icons(widget,
          config_assign_boolean(scanner, FALSE, "icons"));
      return TRUE;
    case G_TOKEN_LABELS:
      flow_grid_set_labels(widget,
          config_assign_boolean(scanner, FALSE, "labels"));
      return TRUE;
    case G_TOKEN_TITLEWIDTH:
      flow_grid_set_title_width(widget,
          config_assign_number(scanner, "title_width"));
      return TRUE;
    case G_TOKEN_SORT:
      flow_grid_set_sort(widget,
          config_assign_boolean(scanner, TRUE, "sort"));
      return TRUE;
    case G_TOKEN_NUMERIC:
      g_message("property 'numeric' has been deprecated");
      return TRUE;
  }

  return FALSE;
}

static gboolean config_widget_variable ( GScanner *scanner, GtkWidget *widget )
{
  vm_var_t *var;
  GBytes *code;

  if( !(var = vm_store_lookup_string(base_widget_get_store(widget),
          scanner->value.v_identifier)) )
    return FALSE;
  if( !(code = config_assign_expr(scanner,
          (gchar *)g_quark_to_string(var->quark))) )
    return FALSE;

  var->value = vm_code_eval(code, widget);
  g_bytes_unref(code);

  return TRUE;
}

gboolean config_widget_property ( GScanner *scanner, GtkWidget *widget )
{
  GtkWindow *win;
  GdkRectangle rect;
  GBytes *code;
  gint key;

  if(config_widget_variable(scanner, widget))
    return TRUE;

  if(config_flowgrid_property(scanner, widget))
    return TRUE;

  key = config_lookup_key(scanner, config_prop_keys);

  if(IS_BASE_WIDGET(widget))
    switch(key)
    {
      case G_TOKEN_DISABLE:
        g_object_set_data(G_OBJECT(widget), "disable",
            GINT_TO_POINTER(config_assign_boolean(scanner, FALSE, "disable")));
        return !scanner->max_parse_errors;
      case G_TOKEN_STYLE:
        code = config_assign_expr(scanner, "style");
        g_object_set(G_OBJECT(widget), "style", code, NULL);
        g_bytes_unref(code);
        return !scanner->max_parse_errors;
      case G_TOKEN_CSS:
        base_widget_set_css(widget, config_assign_string(scanner, "css"));
        return TRUE;
      case G_TOKEN_INTERVAL:
        g_object_set(G_OBJECT(widget), "interval",
            (gint64)(config_assign_number(scanner, "interval")*1000), NULL);
        return TRUE;
      case G_TOKEN_LOCAL:
        g_object_set(G_OBJECT(widget), "local-state",
            config_assign_boolean(scanner, FALSE, "local"), NULL);
        return TRUE;
      case G_TOKEN_TRIGGER:
        g_object_set(G_OBJECT(widget), "trigger",
            config_assign_string(scanner, "trigger"), NULL);
        return TRUE;
      case G_TOKEN_LOC:
        rect = config_get_loc(scanner);
        g_object_set(G_OBJECT(widget), "rectangle", &rect, NULL);
        return TRUE;
      case G_TOKEN_ACTION:
        config_widget_action(scanner, widget);
        return TRUE;
    }

  if(IS_BASE_WIDGET(widget) && !IS_FLOW_GRID(widget))
    switch(key)
    {
      case G_TOKEN_VALUE:
        code = config_assign_expr(scanner, "value");
        g_object_set(G_OBJECT(widget), "value", code, NULL);
        g_bytes_unref(code);
        return !scanner->max_parse_errors;
      case G_TOKEN_TOOLTIP:
        code = config_assign_expr(scanner, "tooltip");
        g_object_set(G_OBJECT(widget), "tooltip", code, NULL);
        g_bytes_unref(code);
        return !scanner->max_parse_errors;
    }

  if(IS_PAGER(widget))
    switch(key)
    {
      case G_TOKEN_PINS:
        pager_add_pins(widget, config_assign_string_list(scanner));
        return TRUE;
      case G_TOKEN_PREVIEW:
        g_object_set_data(G_OBJECT(widget), "preview",
            GINT_TO_POINTER(config_assign_boolean(scanner, FALSE, "preview")));
        return TRUE;
    }

  if(IS_TASKBAR_SHELL(widget))
    switch(key)
    {
      case G_TOKEN_TOOLTIPS:
        taskbar_shell_set_tooltips(widget,
            config_assign_boolean(scanner, FALSE, "tooltips"));
        return TRUE;
      case G_TOKEN_PEROUTPUT:
        if(config_assign_boolean(scanner,FALSE,"filter_output"))
          taskbar_shell_set_filter(widget, G_TOKEN_OUTPUT);
        g_message("'filter_output' is deprecated, please use 'filter = output' instead");
        return TRUE;
      case G_TOKEN_FILTER:
        taskbar_shell_set_filter(widget, GPOINTER_TO_INT(config_assign_tokens(
                scanner, config_filter_keys,
              "Invalid value in 'filter = output|workspace|floating'")));
        return TRUE;
      case G_TOKEN_GROUP:
        if(g_scanner_peek_next_token(scanner) == '=')
        {
          taskbar_shell_set_api(widget, config_assign_tokens(scanner,
                config_taskbar_types,
                "Invalid value in taskbar = false|popup|pager"));
          return TRUE;
        }
        g_scanner_get_next_token(scanner);
        switch(config_lookup_key(scanner, config_flowgrid_props))
        {
          case G_TOKEN_COLS:
            taskbar_shell_set_group_cols(widget,
                (gint)config_assign_number(scanner, "group cols"));
            return TRUE;
          case G_TOKEN_ROWS:
            taskbar_shell_set_group_rows(widget,
                (gint)config_assign_number(scanner, "group rows"));
            return TRUE;
          case G_TOKEN_ICONS:
            taskbar_shell_set_group_icons(widget,
                config_assign_boolean(scanner, FALSE, "group icons"));
            return TRUE;
          case G_TOKEN_LABELS:
            taskbar_shell_set_group_labels(widget,
                config_assign_boolean(scanner, FALSE, "group labels"));
            return TRUE;
          case G_TOKEN_SORT:
            taskbar_shell_set_group_sort(widget,
                config_assign_boolean(scanner, TRUE, "group sort"));
            return TRUE;
          case G_TOKEN_TITLEWIDTH:
            taskbar_shell_set_group_title_width(widget,
                (gint)config_assign_number(scanner, "group title_width"));
            return TRUE;
        }
        switch(config_lookup_key(scanner, config_prop_keys))
        {
          case G_TOKEN_CSS:
            taskbar_shell_set_group_css(widget,
                config_assign_string(scanner, "group css"));
            return TRUE;
          case G_TOKEN_STYLE:
            taskbar_shell_set_group_style(widget,
                config_assign_string(scanner, "group style"));
            return TRUE;
        }
    }

  win = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));
  if(win && gtk_bin_get_child(GTK_BIN(win)) == widget &&
      gtk_window_get_window_type(win) == GTK_WINDOW_POPUP)
    switch(key)
    {
      case G_TOKEN_AUTOCLOSE:
        popup_set_autoclose(GTK_WIDGET(win),
            config_assign_boolean(scanner, FALSE, "autoclose"));
        return TRUE;
    }

  if(GTK_IS_BOX(gtk_widget_get_parent(widget)))
    switch(key)
    {
      case G_TOKEN_LAYER:
        bar_set_layer(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string(scanner, "layer"));
        return TRUE;
      case G_TOKEN_SIZE:
        bar_set_size(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string(scanner, "size"));
        return TRUE;
      case G_TOKEN_EXCLUSIVEZONE:
        bar_set_exclusive_zone(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string(scanner, "exclusive zone"));
        return TRUE;
      case G_TOKEN_SENSOR:
        bar_set_sensor(gtk_widget_get_ancestor(widget, BAR_TYPE),
            (gint64)config_assign_number(scanner, "sensor timeout"));
        return TRUE;
      case G_TOKEN_SENSORDELAY:
        bar_set_show_sensor(gtk_widget_get_ancestor(widget, BAR_TYPE),
            (gint64)config_assign_number(scanner, "sensor popup delay"));
        return TRUE;
      case G_TOKEN_TRANSITION:
        bar_set_transition(gtk_widget_get_ancestor(widget, BAR_TYPE),
            (gint64)config_assign_number(scanner, "transition timeout"));
        return TRUE;
      case G_TOKEN_BAR_ID:
        bar_set_id(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string(scanner, "bar id"));
        return TRUE;
      case G_TOKEN_MONITOR:
        bar_set_monitor(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string(scanner, "monitor"));
        return TRUE;
      case G_TOKEN_MARGIN:
        bar_set_margin(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_number(scanner, "bar margin"));
        return TRUE;
      case G_TOKEN_MIRROR:
        bar_set_mirrors(gtk_widget_get_ancestor(widget, BAR_TYPE),
            config_assign_string_list(scanner));
        return TRUE;
    }

  return FALSE;
}

GtkWidget *config_widget_find_existing ( GScanner *scanner,
    GtkWidget *container, GType (*get_type)(void) )
{
  GtkWidget *widget, *parent;

  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
    return NULL;

  if( !(widget = base_widget_from_id(SCANNER_STORE(scanner),
          scanner->next_value.v_string)) )
    return NULL;

  if(!G_TYPE_CHECK_INSTANCE_TYPE((widget), get_type()))
    return NULL;

  parent = gtk_widget_get_parent(widget);
  parent = parent? gtk_widget_get_parent(parent) : NULL;

  if(container && parent != container)
    return NULL;

  g_scanner_get_next_token(scanner);
  return widget;
}

gboolean config_widget_child ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget;
  GType (*type_get)(void);

  if(container && !IS_GRID(container))
    return FALSE;

  if(config_include(scanner, container))
    return TRUE;

  if( !(type_get = config_lookup_ptr(scanner, config_widget_keys)) )
    return FALSE;

  scanner->max_parse_errors = FALSE;
  if( (widget = config_widget_find_existing(scanner, container, type_get)) )
    container = gtk_widget_get_ancestor(widget, GRID_TYPE);
  else
  {
    widget = GTK_WIDGET(g_object_new(type_get(), NULL));
    if(config_check_and_consume(scanner, G_TOKEN_STRING))
      base_widget_set_id(widget, g_strdup(scanner->value.v_string));
    if(container)
    {
      grid_attach(container, widget);
      grid_mirror_child(container, widget);
    }
    css_widget_cascade(widget, NULL);
  }

  config_widget(scanner, widget);

  if(g_object_get_data(G_OBJECT(widget), "disable"))
    gtk_widget_destroy(widget);
  else if(!container)
  {
    g_scanner_error(scanner, "orphan widget without a valid address: %s",
        base_widget_get_id(widget));
    gtk_widget_destroy(widget);
    return TRUE; /* we parsed successfully, but address was wrong */
  }

  return TRUE;
}

void config_widget ( GScanner *scanner, GtkWidget *widget )
{
  if(!base_widget_get_store(widget))
    g_object_set(G_OBJECT(widget), "store", SCANNER_STORE(scanner), NULL);
  if(!config_check_and_consume(scanner, '{'))
    return;

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(scanner->token == ';')
      continue;
    if(config_widget_property(scanner, widget))
      continue;
    if(config_widget_child(scanner, widget))
      continue;

    g_scanner_error(scanner, "Invalid property in a widget declaration");
  }
}

GtkWidget *config_layout ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *layout;

  scanner->max_parse_errors = FALSE;
  if(container)
    layout = grid_new();
  else
    layout = bar_grid_from_name(
        config_check_and_consume(scanner, G_TOKEN_STRING)?
        scanner->value.v_string : NULL);

  config_widget(scanner, layout);

  return layout;
}

void config_popup ( GScanner *scanner )
{
  gchar *id;

  config_parse_sequence(scanner,
      SEQ_OPT, '(', NULL, NULL, NULL,
      SEQ_REQ, G_TOKEN_STRING, NULL, &id, "Missing PopUp id",
      SEQ_OPT, ')', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors && id)
    config_widget(scanner, gtk_bin_get_child(GTK_BIN(popup_new(id))));

  g_free(id);
}
