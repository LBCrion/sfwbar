/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "../config.h"

gboolean config_expect_token ( GScanner *scanner, gint token, gchar *fmt, ...)
{
  gchar *errmsg;
  va_list args;

  if( g_scanner_peek_next_token(scanner) == token )
    return TRUE;
 
  va_start(args,fmt);
  errmsg = g_strdup_vprintf(fmt,args);
  va_end(args);
  g_scanner_error(scanner,"%s",errmsg);
  g_free(errmsg);

  return FALSE;
}

void config_optional_semicolon ( GScanner *scanner )
{
  if(g_scanner_peek_next_token(scanner) == ';')
    g_scanner_get_next_token(scanner);
}

void config_parse_sequence ( GScanner *scanner, ... )
{
  va_list args;
  void *dest;
  parse_func func;
  gchar *err;
  gint type;
  gint req;
  gboolean matched = TRUE;

  scanner->max_parse_errors = FALSE;
  va_start(args,scanner);
  req = va_arg(args, gint );
  while(req!=SEQ_END)
  {
    type = va_arg(args, gint );
    func = va_arg(args, parse_func );
    dest  = va_arg(args, void * );
    err = va_arg(args, char * );
    if( (type < 0 && (matched || req != SEQ_CON)) || 
        g_scanner_peek_next_token(scanner) == type || 
        ( scanner->next_token == G_TOKEN_FLOAT && type == G_TOKEN_INT) )
    {
      if(type != -2)
        g_scanner_get_next_token(scanner);
      else
        if(func && !func(scanner,dest))
          if(err)
            g_scanner_error(scanner,"%s",err);

      matched = TRUE;
      if(dest)
        switch(type)
        {
          case G_TOKEN_STRING:
            *((gchar **)dest) = g_strdup(scanner->value.v_string);
            break;
          case G_TOKEN_IDENTIFIER:
            *((gchar **)dest) = g_strdup(scanner->value.v_identifier);
            break;
          case G_TOKEN_FLOAT:
            *((gdouble *)dest) = scanner->value.v_float;
            break;
          case G_TOKEN_INT:
            *((gint *)dest) = (gint)scanner->value.v_float;
            break;
          case -1:
            *((gint *)dest) = scanner->token;
            break;
          case -2:
            break;
          default:
            *((gboolean *)dest) = TRUE;
            break;
        }
    }
    else
      if(req == SEQ_OPT || (req == SEQ_CON && !matched))
        matched = FALSE;
      else
        g_scanner_error(scanner,"%s",err);
    req = va_arg(args, gint );
  }
  va_end(args);
}

gboolean config_assign_boolean (GScanner *scanner, gboolean def, gchar *expr)
{
  gboolean result = def;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <boolean>",expr))
    return FALSE;
  g_scanner_get_next_token(scanner);

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_TRUE:
      result = TRUE;
      break;
    case G_TOKEN_FALSE:
      result = FALSE;
      break;
    default:
      g_scanner_error(scanner, "Missing <boolean> value in %s = <boolean>",
          expr);
      break;
  }

  config_optional_semicolon(scanner);

  return result;
}

gchar *config_assign_string ( GScanner *scanner, gchar *expr )
{
  gchar *result;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <string>",expr))
    return NULL;

  g_scanner_get_next_token(scanner);

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing <string> in %s = <string>",expr))
    return NULL;

  g_scanner_get_next_token(scanner);
  result = g_strdup(scanner->value.v_string);
  config_optional_semicolon(scanner);

  return result;
}

gdouble config_assign_number ( GScanner *scanner, gchar *expr )
{
  gdouble result;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <number>",expr))
    return 0;

  g_scanner_get_next_token(scanner);

  if(!config_expect_token(scanner, G_TOKEN_FLOAT,
        "Missing <number> in %s = <number>",expr))
    return 0;

  g_scanner_get_next_token(scanner);
  result = scanner->value.v_float;
  config_optional_semicolon(scanner);

  return result;
}

gint config_assign_tokens ( GScanner *scanner, gchar *name, gchar *type, ... )
{
  va_list args;
  gint token, res = 0;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = %s",name,type))
    return 0;

  g_scanner_get_next_token(scanner);
  g_scanner_peek_next_token(scanner);

  va_start(args,type);
  token = va_arg(args, gint );

  while(token!=0)
  {
    if(token == scanner->next_token)
      res = g_scanner_get_next_token(scanner);

    token = va_arg(args, gint );
  }
  va_end(args);

  if(!res)
    g_scanner_error(scanner,"missing %s in %s = %s",name,type,name);
  config_optional_semicolon(scanner);

  return res;
}
