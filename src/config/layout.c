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
      SEQ_REQ,'(',NULL,NULL,"missing '(' afer loc",
      SEQ_REQ,G_TOKEN_INT,NULL,&rect.x,"missing x value in loc",
      SEQ_REQ,',',NULL,NULL,"missing comma afer x value in loc",
      SEQ_REQ,G_TOKEN_INT,NULL,&rect.y,"missing y value in loc",
      SEQ_OPT,',',NULL,NULL,NULL,
      SEQ_CON,G_TOKEN_INT,NULL,&rect.width,"missing w value in loc",
      SEQ_OPT,',',NULL,NULL,NULL,
      SEQ_CON,G_TOKEN_INT,NULL,&rect.height,"missing h value in loc",
      SEQ_REQ,')',NULL,NULL,"missing ')' in loc statement",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END );

  return rect;
}

void config_get_pins ( GScanner *scanner, GtkWidget *widget )
{
  scanner->max_parse_errors = FALSE;

  if(!IS_PAGER(widget))
    return g_scanner_error(scanner,"this widget has no property 'pins'");

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

void config_widget_action_index ( GScanner *scanner, gint *button, gint *mod )
{
  *mod = 0;

  g_scanner_get_next_token(scanner);
  while(g_scanner_peek_next_token(scanner)=='+')
  {
    switch((gint)(scanner->token))
    {
      case G_TOKEN_SHIFT:
        *mod |= GDK_SHIFT_MASK;
        break;
      case G_TOKEN_CTRL:
        *mod |= GDK_CONTROL_MASK;
        break;
      case G_TOKEN_MOD1:
        *mod |= GDK_MOD1_MASK;
        break;
      case G_TOKEN_MOD2:
        *mod |= GDK_MOD2_MASK;
        break;
      case G_TOKEN_MOD3:
        *mod |= GDK_MOD3_MASK;
        break;
      case G_TOKEN_MOD4:
        *mod |= GDK_MOD4_MASK;
        break;
      case G_TOKEN_MOD5:
        *mod |= GDK_MOD5_MASK;
        break;
      case G_TOKEN_SUPER:
        *mod |= GDK_SUPER_MASK;
        break;
      case G_TOKEN_HYPER:
        *mod |= GDK_HYPER_MASK;
        break;
      case G_TOKEN_META:
        *mod |= GDK_META_MASK;
        break;
      default:
        g_scanner_error(scanner, "Invalid action key modifier");
    }
    g_scanner_get_next_token(scanner); // get '+'
    g_scanner_get_next_token(scanner);
  }
  switch((gint)(scanner->token))
  {
    case G_TOKEN_FLOAT:
      *button = scanner->value.v_float;
      break;
    case G_TOKEN_INIT:
      *button = 0;
      break;
    case G_TOKEN_LEFT:
      *button = 1;
      break;
    case G_TOKEN_MIDDLE:
      *button = 2;
      break;
    case G_TOKEN_RIGHT:
      *button = 3;
      break;
    case G_TOKEN_SCROLL_UP:
      *button = 4;
      break;
    case G_TOKEN_SCROLL_DOWN:
      *button = 5;
      break;
    case G_TOKEN_SCROLL_LEFT:
      *button = 6;
      break;
    case G_TOKEN_SCROLL_RIGHT:
      *button = 7;
      break;
    default:
      g_scanner_error(scanner,"invalid action index");
      break;
  }
}

void config_widget_action ( GScanner *scanner, GtkWidget *widget )
{
  gint button = 1, mod = 0;

  if(g_scanner_peek_next_token(scanner)=='[')
  {
    g_scanner_get_next_token(scanner);
    config_widget_action_index(scanner,&button,&mod);
    if(config_expect_token(scanner, ']', "missing ']' after action"))
      g_scanner_get_next_token(scanner);
  }

  if(config_expect_token(scanner, '=', "missing '=' after action"))
    g_scanner_get_next_token(scanner);

  if(scanner->max_parse_errors)
    return;

  if( button<0 || button >=8 )
    return g_scanner_error(scanner,"invalid action index %d",button);

  base_widget_set_action(widget,button,mod,config_action(scanner));
  if(!base_widget_get_action(widget,button,mod))
    return g_scanner_error(scanner,"invalid action");

  config_optional_semicolon(scanner);
}

GtkWidget *config_include ( GScanner *scanner, gboolean toplevel )
{
  GtkWidget *widget;
  gchar *fname = NULL;

  config_parse_sequence(scanner,
      SEQ_REQ,'(',NULL,NULL,"Missing '(' after include",
      SEQ_REQ,G_TOKEN_STRING,NULL,&fname,"Missing filename in include",
      SEQ_REQ,')',NULL,NULL,"Missing ')',after include",
      SEQ_OPT,';',NULL,NULL,NULL,
      SEQ_END);

  if(!scanner->max_parse_errors) 
    widget = config_parse(fname, toplevel);
  else
    widget = NULL;

  g_free(fname);

  return widget;
}

gboolean config_widget_property ( GScanner *scanner, GtkWidget *widget )
{
  GtkWindow *win;

  if(IS_BASE_WIDGET(widget))
    switch ( (gint)scanner->token )
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
        base_widget_set_interval(widget,0);
        base_widget_set_trigger(widget,
            config_assign_string(scanner, "trigger"));
        return TRUE;
      case G_TOKEN_LOC:
        base_widget_set_rect(widget,config_get_loc(scanner));
        return TRUE;
      case G_TOKEN_ACTION:
        config_widget_action(scanner, widget);
        return TRUE;
    }

  if(IS_BASE_WIDGET(widget) && !IS_FLOW_GRID(base_widget_get_child(widget)))
    switch ( (gint)scanner->token )
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
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_PINS:
        config_get_pins( scanner, widget );
        return TRUE;
      case G_TOKEN_PREVIEW:
        g_object_set_data(G_OBJECT(base_widget_get_child(widget)),"preview",
            GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"preview")));
        return TRUE;
      case G_TOKEN_NUMERIC:
        g_object_set_data(G_OBJECT(base_widget_get_child(widget)),
            "sort_numeric",GINT_TO_POINTER(
              config_assign_boolean(scanner,TRUE,"numeric")));
        return TRUE;
    }

  if(IS_TASKBAR(widget))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_PEROUTPUT:
        if(config_assign_boolean(scanner,FALSE,"filter_output"))
          taskbar_set_filter(widget,G_TOKEN_OUTPUT);
        g_message("'filter_output' is deprecated, please use 'filter = output' instead");
        return TRUE;
      case G_TOKEN_FILTER:
        taskbar_set_filter(widget,config_assign_tokens(scanner,"filter",
              "output|workspace",
              G_TOKEN_OUTPUT,G_TOKEN_WORKSPACE,G_TOKEN_FLOATING,0));
        return TRUE;
      case G_TOKEN_TITLEWIDTH:
        g_object_set_data(G_OBJECT(widget),"title_width",
            GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
        return TRUE;
      case G_TOKEN_GROUP:
        if(g_scanner_peek_next_token(scanner) == '=')
        {
          g_object_set_data(G_OBJECT(widget),"group",GINT_TO_POINTER(
            config_assign_boolean(scanner,FALSE,"group")));
          return TRUE;
        }
        switch((gint)g_scanner_get_next_token(scanner))
        {
          case G_TOKEN_COLS:
            g_object_set_data(G_OBJECT(widget),"g_cols",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group cols")));
            return TRUE;
          case G_TOKEN_ROWS:
            g_object_set_data(G_OBJECT(widget),"g_rows",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group rows")));
            return TRUE;
          case G_TOKEN_ICONS:
            g_object_set_data(G_OBJECT(widget),"g_icons",
              GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"group icons")));
            return TRUE;
          case G_TOKEN_LABELS:
            g_object_set_data(G_OBJECT(widget),"g_labels",
              GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"group labels")));
            return TRUE;
          case G_TOKEN_CSS:
            g_object_set_data_full(G_OBJECT(widget),"g_css",
              config_assign_string(scanner,"group css"),g_free);
            return TRUE;
          case G_TOKEN_STYLE:
            g_object_set_data_full(G_OBJECT(widget),"g_style",
              config_assign_string(scanner,"group style"),g_free);
            return TRUE;
          case G_TOKEN_TITLEWIDTH:
            g_object_set_data(G_OBJECT(widget),"g_title_width",GINT_TO_POINTER(
              (gint)config_assign_number(scanner,"group title_width")));
            return TRUE;
          case G_TOKEN_SORT:
            g_object_set_data(G_OBJECT(widget),"g_sort",GINT_TO_POINTER(
              config_assign_boolean(scanner,TRUE,"group sort")));
            return TRUE;
        }
    }

  if(IS_FLOW_GRID(base_widget_get_child(widget)))
    switch ( (gint)scanner->token )
    {
      case G_TOKEN_COLS:
        flow_grid_set_cols(base_widget_get_child(widget),
          config_assign_number(scanner, "cols"));
        return TRUE;
      case G_TOKEN_ROWS:
        flow_grid_set_rows(base_widget_get_child(widget),
          config_assign_number(scanner, "rows"));
        return TRUE;
      case G_TOKEN_PRIMARY:
        flow_grid_set_primary(base_widget_get_child(widget),
            config_assign_tokens(scanner,"primary","rows|cols",
              G_TOKEN_ROWS,G_TOKEN_COLS,NULL));
        return TRUE;
      case G_TOKEN_ICONS:
        g_object_set_data(G_OBJECT(widget),"icons",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"icons")));
        return TRUE;
      case G_TOKEN_LABELS:
        g_object_set_data(G_OBJECT(widget),"labels",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"labels")));
        return TRUE;
      case G_TOKEN_SORT:
        flow_grid_set_sort(base_widget_get_child(widget),
              config_assign_boolean(scanner,TRUE,"sort"));
        return TRUE;
    }

  win = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));
  if(win && gtk_bin_get_child(GTK_BIN(win)) == widget &&
      gtk_window_get_window_type(win) == GTK_WINDOW_POPUP)
    switch ( (gint)scanner->token )
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
      return taskbar_new(TRUE);
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
  gint type;

  if(!IS_GRID(container))
    return FALSE;

  type = scanner->token;
  switch (type)
  {
    case G_TOKEN_GRID:
    case G_TOKEN_LABEL:
    case G_TOKEN_IMAGE:
    case G_TOKEN_BUTTON:
    case G_TOKEN_SCALE:
    case G_TOKEN_CHART:
    case G_TOKEN_INCLUDE:
    case G_TOKEN_TASKBAR:
    case G_TOKEN_PAGER:
    case G_TOKEN_TRAY:
      break;
    default:
      return FALSE;
  }
  scanner->max_parse_errors = FALSE;

  if(g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
  {
    g_scanner_get_next_token(scanner);
    widget = base_widget_from_id(scanner->value.v_string);
    parent = widget?gtk_widget_get_parent(widget):NULL;
    parent = parent?gtk_widget_get_parent(parent):NULL;
    if(!widget || parent != container)
    {
      widget = config_widget_new(type, scanner);
      base_widget_set_id(widget, g_strdup(scanner->value.v_string));
    }
  }
  else
    widget = config_widget_new(type, scanner);

  config_widget(scanner, widget);
  if(!gtk_widget_get_parent(widget))
    grid_attach(container, widget);
  css_widget_cascade(widget, NULL);

  return TRUE;
}

void config_widget ( GScanner *scanner, GtkWidget *widget )
{
  if(g_scanner_peek_next_token(scanner) == '{')
  {
    g_scanner_get_next_token(scanner);
    while ( (gint)g_scanner_get_next_token ( scanner ) != '}' &&
        (gint)scanner->token != G_TOKEN_EOF )
    {
      if(!config_widget_property(scanner,widget) &&
          !config_widget_child(scanner,widget))
        g_scanner_error(scanner,"Invalid property in a widget declaration");
    }
    if(scanner->token != '}')
      g_scanner_error(scanner,"Missing '}' at the end of widget properties");
  }
  config_optional_semicolon(scanner);
}

void config_layout ( GScanner *scanner, GtkWidget **widget, gboolean toplevel )
{
  GtkWidget *layout;
  scanner->max_parse_errors=FALSE;
 
  if(!toplevel)
  {
    if(!*widget)
    {
      *widget = grid_new();
      gtk_widget_set_name(*widget,"layout");
    }
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
  if(g_scanner_peek_next_token(scanner)!=G_TOKEN_STRING)
  {
    g_scanner_error(scanner,"missing identifier after 'popup'");
    return;
  }

  g_scanner_get_next_token(scanner);
  win = popup_new(scanner->value.v_string);
  grid = gtk_bin_get_child(GTK_BIN(win));
  config_widget(scanner, grid);
}
