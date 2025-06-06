/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "util/string.h"
#include "vm/vm.h"

gboolean config_expect_token ( GScanner *scanner, gint token, gchar *fmt, ...)
{
  gchar *errmsg;
  va_list args;

  if( g_scanner_peek_next_token(scanner) == token )
  {
    g_scanner_get_next_token(scanner);
    return TRUE;
  }
 
  va_start(args, fmt);
  errmsg = g_strdup_vprintf(fmt, args);
  va_end(args);
  g_scanner_error(scanner, "%s", errmsg);
  g_free(errmsg);

  return FALSE;
}

gboolean config_check_and_consume ( GScanner *scanner, gint token )
{
  if(g_scanner_peek_next_token(scanner) != token)
    return FALSE;

  g_scanner_get_next_token(scanner);
  return TRUE;
}

gboolean config_action ( GScanner *scanner, GBytes **action_dst )
{
  GByteArray *code;

  *action_dst = NULL;
  code = g_byte_array_new();
  if(parser_block_parse(scanner, code))
    *action_dst = g_byte_array_free_to_bytes(code);
  else
    g_byte_array_unref(code);

  return !!*action_dst;
}

gboolean config_expr ( GScanner *scanner, GBytes **expr_dst )
{
  GByteArray *code;

  *expr_dst = NULL;
  code = g_byte_array_new();
  if(parser_expr_parse(scanner, code))
    *expr_dst = g_byte_array_free_to_bytes(code);
  else
    g_byte_array_unref(code);
  return !!*expr_dst;
}

void config_parse_sequence ( GScanner *scanner, ... )
{
  va_list args;
  void *dest;
  parse_func func;
  gchar *err;
  gint type, req;
  gboolean matched = TRUE;

  scanner->max_parse_errors = FALSE;
  va_start(args,scanner);
  req = va_arg(args, gint);
  while(req!=SEQ_END)
  {
    type = va_arg(args, gint);
    func = va_arg(args, parse_func);
    dest  = va_arg(args, void *);
    err = va_arg(args, char *);
    
    if(dest)
    {
      if(type == G_TOKEN_STRING || type == G_TOKEN_IDENTIFIER)
        *((gchar **)dest) = NULL;
      else if (type > 0 && type != G_TOKEN_INT && type != G_TOKEN_FLOAT)
        *((gboolean *)dest) = FALSE;
    }

    if( (matched || req != SEQ_CON) && (type < 0 || type == G_TOKEN_VALUE ||
          type == g_scanner_peek_next_token(scanner) ||
          (scanner->next_token == G_TOKEN_FLOAT && type == G_TOKEN_INT) ))
    {
      if(type != G_TOKEN_VALUE && type != -2)
        g_scanner_get_next_token(scanner);

      if(type == -2 && func && !func(scanner, dest) && err)
          g_scanner_error(scanner, "%s", err);

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

gboolean config_assign_boolean (GScanner *scanner, gboolean def, const gchar *expr)
{
  gboolean result = def;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <boolean>", expr))
    return FALSE;

  g_scanner_get_next_token(scanner);

  if(!g_ascii_strcasecmp(scanner->value.v_identifier, "true"))
    result = TRUE;
  else if(!g_ascii_strcasecmp(scanner->value.v_identifier, "false"))
    result = FALSE;
  else
    g_scanner_error(scanner, "Missing value in %s = <boolean>", expr);

  config_check_and_consume(scanner, ';');

  return result;
}

gchar *config_assign_enum ( GScanner *scanner, const gchar *expr )
{
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <enum>", expr))
    return NULL;

  if(config_check_and_consume(scanner, G_TOKEN_STRING))
    return scanner->value.v_string;
  if(config_check_and_consume(scanner, G_TOKEN_IDENTIFIER))
    return scanner->value.v_identifier;

  g_scanner_error(scanner, "Missing <string> in %s = <string>", expr);
  return NULL;
}

gchar *config_assign_string ( GScanner *scanner, const gchar *expr )
{
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <string>", expr))
    return NULL;

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing <string> in %s = <string>", expr))
    return NULL;

  return scanner->value.v_string;
}

gdouble config_assign_number ( GScanner *scanner, const gchar *expr )
{
  gdouble result;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <number>", expr))
    return 0;

  if(!config_expect_token(scanner, G_TOKEN_FLOAT,
        "Missing <number> in %s = <number>", expr))
    return 0;

  result = scanner->value.v_float;
  config_check_and_consume(scanner, ';');

  return result;
}

GPtrArray *config_assign_string_list ( GScanner *scanner )
{
  GPtrArray *array;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' after '%s'",
        scanner->value.v_identifier ))
    return NULL;

  array = g_ptr_array_new_full(1, g_free);
  do
  {
    if(!config_expect_token(scanner, G_TOKEN_STRING,
          "invalid token in string list"))
      break;
    g_ptr_array_add(array, g_strdup(scanner->value.v_string));
  } while (config_check_and_consume(scanner, ','));
  config_check_and_consume(scanner, ';');

  return array;
}

GBytes *config_assign_action ( GScanner *scanner, gchar *err )
{
  GBytes *code;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <code>", err))
    return NULL;

  if(!config_action(scanner, &code))
    return NULL;

  config_check_and_consume(scanner, ';');

  return code;
}

GBytes *config_assign_expr ( GScanner *scanner, const gchar *err )
{
  GBytes *code;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <code>", err))
    return NULL;

  if(!config_expr(scanner, &code))
    return NULL;

  config_check_and_consume(scanner, ';');

  return code;
}

gboolean config_is_section_end ( GScanner *scanner )
{
  config_check_and_consume(scanner, ';');

  if(g_scanner_peek_next_token(scanner) == G_TOKEN_EOF)
    return TRUE;

  if(!config_check_and_consume(scanner, '}'))
    return FALSE;

  config_check_and_consume(scanner, ';');
  return TRUE;
}

gpointer config_lookup_ptr ( GScanner *scanner, GHashTable *table )
{
  if(scanner->token != G_TOKEN_IDENTIFIER)
    return NULL;

  return g_hash_table_lookup(table, scanner->value.v_identifier);
}

gpointer config_lookup_next_ptr ( GScanner *scanner, GHashTable *table )
{
  if(g_scanner_peek_next_token(scanner) != G_TOKEN_IDENTIFIER)
    return NULL;

  return g_hash_table_lookup(table, scanner->next_value.v_identifier);
}

void config_skip_statement ( GScanner *scanner )
{
  gint c = 0;

  scanner->max_parse_errors = FALSE;
  while(g_scanner_get_next_token(scanner) != ';' || c)
  {
    if(scanner->token == '{')
      c++;
    else if(scanner->token == '}')
    {
      c--;
      if(c<1)
        return;
    }
  }
}
