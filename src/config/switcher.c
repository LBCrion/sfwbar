/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "css.h"
#include "config.h"
#include "switcher.h"

void config_switcher ( GScanner *scanner )
{
  GtkWidget *widget;

  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{',"Missing '{' after 'switcher'"))
    return;

  widget = switcher_new();

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(!config_flowgrid_property(scanner, widget))
    {
      if(!g_ascii_strcasecmp(scanner->value.v_identifier, "css"))
        css_widget_apply(widget, config_assign_string(scanner, "css"));
      else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "interval"))
        g_object_set_data(G_OBJECT(widget),"interval",
          GINT_TO_POINTER(config_assign_number(scanner,"interval")/100));
      else
        g_scanner_error(scanner, "Unexpected token in 'switcher'");
    }
  }
}
