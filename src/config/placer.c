/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "../config.h"

void config_placer ( GScanner *scanner )
{
  gint wp_x= 10;
  gint wp_y= 10;
  gint wo_x= 0;
  gint wo_y= 0;
  gboolean pid = FALSE;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner,'{',"Missing '{' after 'placer'"))
    return;
  g_scanner_get_next_token(scanner);

  while (( (gint)g_scanner_peek_next_token ( scanner ) != '}' )&&
      ( (gint)g_scanner_peek_next_token ( scanner ) != G_TOKEN_EOF ))
  {
    switch ((gint)g_scanner_get_next_token(scanner) )
    {
      case G_TOKEN_XSTEP: 
        wp_x = config_assign_number ( scanner, "xstep" );
        break;
      case G_TOKEN_YSTEP: 
        wp_y = config_assign_number ( scanner, "ystep" );
        break;
      case G_TOKEN_XORIGIN: 
        wo_x = config_assign_number ( scanner, "xorigin" );
        break;
      case G_TOKEN_YORIGIN: 
        wo_y = config_assign_number ( scanner, "yorigin" );
        break;
      case G_TOKEN_CHILDREN:
        pid = config_assign_boolean(scanner,FALSE,"children");
        break;
      default:
        g_scanner_error(scanner,"Unexpected token in 'placer'");
        break;
    }
  }
  if((gint)scanner->next_token == '}')
    g_scanner_get_next_token(scanner);

  config_optional_semicolon(scanner);

  wintree_placer_conf(wp_x,wp_y,wo_x,wo_y,pid);
}

