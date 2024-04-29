/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "../config.h"
#include "../sfwbar.h"
#include "../bar.h"
#include "../button.h"
#include "../scale.h"
#include "../image.h"
#include "../label.h"
#include "../cchart.h"
#include "../grid.h"
#include "../taskbarshell.h"
#include "../taskbar.h"
#include "../pager.h"
#include "../popup.h"
#include "../tray.h"

void config_widget (GScanner *scanner, GtkWidget *widget);

GdkRectangle config_get_loc ( GScanner *scanner )
{
  GdkRectangle rect;
  rect.x = 0;
  rect.y = 0;
  rect.width = 1;
  rect.height = 1;

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

void config_get_pins ( GScanner *scanner, GtkWidget *widget )
{
  scanner->max_parse_errors = FALSE;

  if(!IS_PAGER(widget))
  {
    g_scanner_error(scanner,"this widget has no property 'pins'");
    return;
  }

  if(!config_expect_token(scanner, '=',"expecting pins = string [,string]"))
    return;

  do
  {
    g_scanner_get_next_token(scanner);
    if(!config_expect_token(scanner, G_TOKEN_STRING,
          "expecting a string in pins = string [,string]"))
      break;
    g_scanner_get_next_token(scanner);
    pager_add_pin(widget,g_strdup(scanner->value.v_string));
  } while ( g_scanner_peek_next_token(scanner)==',');
  config_optional_semicolon(scanner);
}

gboolean config_action_mods ( GScanner *scanner, gint *mods )
{
  gpointer mod;

  while((gint)g_scanner_peek_next_token(scanner) == G_TOKEN_VALUE &&
    (mod = g_hash_table_lookup(config_mods, scanner->next_value.v_identifier)))
  {
    g_scanner_get_next_token(scanner);
    *mods |= GPOINTER_TO_INT(mod);

    if(g_scanner_peek_next_token(scanner) != '+')
      return FALSE;
    g_scanner_get_next_token(scanner);
  }
  return TRUE;
}

gboolean config_action_slot (GScanner *scanner, gint *slot )
{
  gint token;

  g_scanner_get_next_token(scanner);
  if(scanner->token == G_TOKEN_FLOAT)
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
  action_t *action = NULL;

  config_parse_sequence(scanner,
      SEQ_OPT, '[', NULL, NULL, NULL,
      SEQ_CON, -2, config_action_mods, &mod, NULL,
      SEQ_CON, -2, config_action_slot, &slot, "invalid action slot",
      SEQ_CON, ']', NULL, NULL, "missing ']' after action",
      SEQ_REQ, '=', NULL, NULL, "missing '=' after action",
      SEQ_REQ, -2, config_action, &action, "missing action",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(!scanner->max_parse_errors)
    base_widget_set_action(widget, slot, mod, action);
  else if(action)
    action_free(action, NULL);
}

GtkWidget *config_include ( GScanner *scanner, gboolean toplevel )
{
  GtkWidget *widget;
  gchar *fname;

  config_parse_sequence(scanner,
      SEQ_REQ, '(', NULL, NULL, "Missing '(' after include",
      SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing filename in include",
      SEQ_REQ, ')', NULL, NULL, "Missing ')',after include",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  widget = scanner->max_parse_errors?NULL:config_parse(fname, toplevel);
  g_free(fname);

  return widget;
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
//      g_object_set_data(G_OBJECT(widget),"icons",
//        GINT_TO_POINTER(config_assign_boolean(scanner, FALSE, "icons")));
      flow_grid_set_icons(widget,
          config_assign_boolean(scanner, FALSE, "icons"));
      return TRUE;
    case G_TOKEN_LABELS:
//      g_object_set_data(G_OBJECT(widget),"labels",
//        GINT_TO_POINTER(config_assign_boolean(scanner, FALSE, "labels")));
      flow_grid_set_labels(widget,
          config_assign_boolean(scanner, FALSE, "labels"));
      return TRUE;
    case G_TOKEN_SORT:
      flow_grid_set_sort(widget,
          config_assign_boolean(scanner, TRUE, "sort"));
      return TRUE;
    case G_TOKEN_NUMERIC:
      g_object_set_data(G_OBJECT(widget), "sort_numeric",GINT_TO_POINTER(
            config_assign_boolean(scanner, TRUE, "numeric")));
      return TRUE;
  }

  return FALSE;
}

gboolean config_widget_property ( GScanner *scanner, GtkWidget *widget )
{
  GtkWindow *win;
  gchar *trigger;
  gint key;

  if(config_flowgrid_property(scanner, widget))
    return TRUE;

  key = config_lookup_key(scanner, config_prop_keys);

  if(IS_BASE_WIDGET(widget))
    switch(key)
    {
      case G_TOKEN_STYLE:
        base_widget_set_style(widget,
            config_get_value(scanner,"style",TRUE,NULL));
        return TRUE;
      case G_TOKEN_CSS:
        base_widget_set_css(widget, config_assign_string(scanner,"css"));
        return TRUE;
      case G_TOKEN_INTERVAL:
        base_widget_set_interval(widget,
            1000*config_assign_number(scanner, "interval"));
        return TRUE;
      case G_TOKEN_TRIGGER:
        trigger = config_assign_string(scanner, "trigger");
        base_widget_set_trigger(widget, trigger);
        g_free(trigger);
        return TRUE;
      case G_TOKEN_LOC:
        base_widget_set_rect(widget, config_get_loc(scanner));
        return TRUE;
      case G_TOKEN_ACTION:
        config_widget_action(scanner, widget);
        return TRUE;
    }

  if(IS_BASE_WIDGET(widget) && !IS_FLOW_GRID(widget))
    switch(key)
    {
      case G_TOKEN_VALUE:
        base_widget_set_value(widget,
            config_get_value(scanner,"value",TRUE,NULL));
        return TRUE;
      case G_TOKEN_TOOLTIP:
        base_widget_set_tooltip(widget,
            config_get_value(scanner,"tooltip",TRUE,NULL));
        return TRUE;
    }

  if(IS_PAGER(widget))
    switch(key)
    {
      case G_TOKEN_PINS:
        config_get_pins( scanner, widget );
        return TRUE;
      case G_TOKEN_PREVIEW:
        g_object_set_data(G_OBJECT(base_widget_get_child(widget)),"preview",
            GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"preview")));
        return TRUE;
    }

  if(IS_TASKBAR_SHELL(widget))
    switch(key)
    {
      case G_TOKEN_PEROUTPUT:
        if(config_assign_boolean(scanner,FALSE,"filter_output"))
          taskbar_shell_set_filter(widget,G_TOKEN_OUTPUT);
        g_message("'filter_output' is deprecated, please use 'filter = output' instead");
        return TRUE;
      case G_TOKEN_FILTER:
        taskbar_shell_set_filter(widget, GPOINTER_TO_INT(config_assign_tokens(
                scanner, config_filter_keys,
              "Invalid value in 'filter = output|workspace|floating'")));
        return TRUE;
      case G_TOKEN_TITLEWIDTH:
        g_object_set_data(G_OBJECT(widget),"title_width",
            GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
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
                config_assign_string(scanner,"group css"));
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

  return FALSE;
}

GtkWidget *config_widget_new ( gint type, GScanner *scanner )
{
  switch(type)
  {
    case G_TOKEN_GRID:
      return grid_new();
    case G_TOKEN_LABEL:
      return label_new();
    case G_TOKEN_IMAGE:
      return image_new();
    case G_TOKEN_BUTTON:
      return button_new();
    case G_TOKEN_SCALE:
      return scale_new();
    case G_TOKEN_CHART:
      return cchart_new();
    case G_TOKEN_INCLUDE:
      return config_include( scanner, FALSE );
    case G_TOKEN_TASKBAR:
      return taskbar_shell_new();
    case G_TOKEN_PAGER:
      return pager_new();
    case G_TOKEN_TRAY:
      return tray_new();
  }
  return NULL;
}

gboolean config_widget_child ( GScanner *scanner, GtkWidget *container )
{
  GtkWidget *widget, *parent;
  gboolean found = FALSE;
  gint type;

  if(container && !IS_GRID(container))
    return FALSE;

  if( !(type = config_lookup_key(scanner, config_widget_keys)) )
      return FALSE;
  scanner->max_parse_errors = FALSE;

  if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
  {
    g_scanner_get_next_token(scanner);
    widget = base_widget_from_id(scanner->value.v_string);
    parent = widget?gtk_widget_get_parent(widget):NULL;
    parent = parent?gtk_widget_get_parent(parent):NULL;
    if(!widget || (container && parent != container))
    {
      widget = config_widget_new(type, scanner);
      base_widget_set_id(widget, g_strdup(scanner->value.v_string));
    }
    else
      found = TRUE;
  }
  else
    widget = config_widget_new(type, scanner);

  config_widget(scanner, widget);
  if(container && !gtk_widget_get_parent(widget))
    grid_attach(container, widget);
  css_widget_cascade(widget, NULL);
  if(!found && !container)
  {
    g_object_ref_sink(widget);
    g_object_unref(widget);
  }

  return TRUE;
}

void config_widget ( GScanner *scanner, GtkWidget *widget )
{
  if(g_scanner_peek_next_token(scanner) != '{')
    return;
  g_scanner_get_next_token(scanner);

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(!config_widget_property(scanner, widget) &&
        !config_widget_child(scanner, widget))
      g_scanner_error(scanner, "Invalid property in a widget declaration");
  }
}

void config_layout ( GScanner *scanner, GtkWidget **widget, gboolean toplevel )
{
  GtkWidget *layout;
  scanner->max_parse_errors=FALSE;

  if(!toplevel)
  {
    if(!*widget)
      *widget = grid_new();
    layout = *widget;
  }
  else
  {
    if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
    {
      g_scanner_get_next_token(scanner);
      layout = bar_grid_from_name(scanner->value.v_string);
    }
    else
      layout = bar_grid_from_name(NULL);
  }

  config_widget(scanner, layout);
}

void config_popup ( GScanner *scanner )
{
  GtkWidget *win, *grid;

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing argument after 'popup'"))
    return;

  g_scanner_get_next_token(scanner);
  win = popup_new(scanner->value.v_string);
  grid = gtk_bin_get_child(GTK_BIN(win));
  config_widget(scanner, grid);
}
