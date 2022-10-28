
#include "../config.h"
#include "../switcher.h"

void config_switcher ( GScanner *scanner )
{
  GtkWidget *widget;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{',"Missing '{' after 'switcher'"))
    return;
  g_scanner_get_next_token(scanner);

  widget = switcher_new();

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token ( scanner ) )
    {
      case G_TOKEN_INTERVAL: 
        g_object_set_data(G_OBJECT(widget),"interval",
          GINT_TO_POINTER(config_assign_number(scanner,"interval")/100));
        break;
      case G_TOKEN_COLS: 
        flow_grid_set_cols(base_widget_get_child(widget),
          config_assign_number(scanner, "cols"));
        break;
      case G_TOKEN_ROWS:
        flow_grid_set_rows(base_widget_get_child(widget),
          config_assign_number(scanner, "rows"));
        break;
      case G_TOKEN_CSS:
        css_widget_apply(widget,config_assign_string(scanner,"css"));
        break;
      case G_TOKEN_ICONS:
        g_object_set_data(G_OBJECT(widget),"icons",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"icons")));
        break;
      case G_TOKEN_LABELS:
        g_object_set_data(G_OBJECT(widget),"labels",
          GINT_TO_POINTER(config_assign_boolean(scanner,FALSE,"labels")));
        break;
      case G_TOKEN_TITLEWIDTH:
        g_object_set_data(G_OBJECT(widget),"title_width",
          GINT_TO_POINTER(config_assign_number(scanner,"title_width")));
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'switcher'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);
}

