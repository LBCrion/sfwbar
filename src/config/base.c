/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "config.h"
#include "util/string.h"

static GHashTable *defines;

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
    
    if(dest)
    {
      if(type == G_TOKEN_STRING || type == G_TOKEN_VALUE ||
          type == G_TOKEN_IDENTIFIER)
        *((gchar **)dest) = NULL;
      else if (type > 0 && type != G_TOKEN_INT && type != G_TOKEN_FLOAT)
        *((gboolean *)dest) = FALSE;
    }

    if( (matched || req != SEQ_CON) &&
        ( type < 0 || type==G_TOKEN_VALUE ||
          g_scanner_peek_next_token(scanner) == type ||
          (scanner->next_token == G_TOKEN_FLOAT && type == G_TOKEN_INT) ))
    {
      if(type != G_TOKEN_VALUE && type != -2)
        g_scanner_get_next_token(scanner);

      if(type == -2)
      {
        if(func && !func(scanner, dest))
          if(err)
            g_scanner_error(scanner, "%s", err);
      }

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
          case G_TOKEN_VALUE:
            *((gchar **)dest) = config_get_value(scanner, err, FALSE, NULL);
            if(!**((gchar **)dest))
            {
              g_free(*((gchar **)dest));
              *((char **)dest) = NULL;
            }
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

gchar *config_assign_string ( GScanner *scanner, gchar *expr )
{
  gchar *result;
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '=', "Missing '=' in %s = <string>", expr))
    return NULL;

  if(!config_expect_token(scanner, G_TOKEN_STRING,
        "Missing <string> in %s = <string>", expr))
    return NULL;

  result = g_strdup(scanner->value.v_string);
  config_check_and_consume(scanner, ';');

  return result;
}

gdouble config_assign_number ( GScanner *scanner, gchar *expr )
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

void *config_assign_tokens ( GScanner *scanner, GHashTable *keys, gchar *err )
{
  void *res = NULL;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' after '%s'",
        scanner->value.v_identifier ))
    return NULL;

  g_scanner_get_next_token(scanner);

  if( scanner->token != G_TOKEN_IDENTIFIER ||
      !(res = g_hash_table_lookup(keys, scanner->value.v_identifier)) )
    g_scanner_error(scanner, "%s", err);
  config_check_and_consume(scanner, ';');

  return res;
}

GList *config_assign_string_list ( GScanner *scanner )
{
  GList *list = NULL;

  scanner->max_parse_errors = FALSE;
  if(!config_expect_token(scanner, '=', "Missing '=' after '%s'",
        scanner->value.v_identifier ))
    return NULL;

  do
  {
    if(!config_expect_token(scanner, G_TOKEN_STRING,
          "invalid token in string list"))
      break;
    list = g_list_prepend(list, g_strdup(scanner->value.v_string));
  } while (config_check_and_consume(scanner, ','));
  config_check_and_consume(scanner, ';');

  return g_list_reverse(list);
}

gboolean config_is_section_end ( GScanner *scanner )
{
  if(g_scanner_peek_next_token(scanner) == G_TOKEN_EOF)
    return TRUE;

  if(!config_check_and_consume(scanner, '}'))
    return FALSE;

  config_check_and_consume(scanner, ';');
  return TRUE;
}

gchar *config_get_value ( GScanner *scanner, gchar *prop, gboolean assign,
    gchar **id )
{
  gchar *value, *temp, *temp2;
  gint pcount = 0;
  static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  scanner->max_parse_errors = FALSE;
  if(assign)
    if(!config_expect_token(scanner, '=', "expecting %s = expression", prop))
      return NULL;

  if(id && g_scanner_peek_next_token(scanner)==G_TOKEN_STRING)
  {
    g_scanner_get_next_token(scanner);
    temp = g_strdup(scanner->value.v_string);
    if(g_scanner_peek_next_token(scanner)==',')
    {
      g_scanner_get_next_token(scanner);
      value = g_strdup("");;
      *id = temp;
    }
    else
    {
      value = str_escape(temp);
      g_free(temp);
    }
  }
  else
    value = g_strdup("");;
  g_scanner_peek_next_token(scanner);
  scanner->token = '+';
  while(((gint)scanner->next_token<G_TOKEN_SCANNER)&&
      (scanner->next_token!='}')&&
      (scanner->next_token!=';')&&
      (scanner->next_token!='[')&&
      (scanner->next_token!=',' || pcount)&&
      (scanner->next_token!=')' || pcount)&&
      (scanner->next_token!=G_TOKEN_IDENTIFIER ||
        strchr(",(+-*/%=<>!|&", scanner->token))&&
      (scanner->next_token!=G_TOKEN_EOF))
  {
    switch((gint)g_scanner_get_next_token(scanner))
    {
      case G_TOKEN_STRING:
        temp = str_escape(scanner->value.v_string);
        temp2 = value;
        value = g_strconcat(temp2, temp, NULL);
        g_free(temp);
        g_free(temp2);
        break;
      case G_TOKEN_IDENTIFIER:
        temp = value;
        if(defines && g_hash_table_contains(defines, scanner->value.v_identifier))
          value = g_strconcat(value, 
              g_hash_table_lookup(defines, scanner->value.v_identifier), NULL);
        else
          value = g_strconcat(value, scanner->value.v_identifier, NULL);
        g_free(temp);
        break;
      case G_TOKEN_FLOAT:
        temp = value;
        value = g_strconcat(temp,g_ascii_dtostr(buf,G_ASCII_DTOSTR_BUF_SIZE,
              scanner->value.v_float),NULL);
        g_free(temp);
        break;
      default:
        temp = value;
        buf[0] = scanner->token;
        buf[1] = 0;
        value = g_strconcat(temp, buf, NULL);
        g_free(temp);
        break;
    }
    if(scanner->token == '(')
      pcount++;
    else if(scanner->token == ')')
      pcount--;
    g_scanner_peek_next_token(scanner);
  }
  config_check_and_consume(scanner, ';');
  return value;
}

void config_define ( GScanner *scanner )
{
  gchar *ident, *value;

  scanner->max_parse_errors = FALSE;
  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &ident,
        "missing identifier after 'define'",
      SEQ_REQ, '=', NULL, NULL, "missing '=' after 'define'",
      SEQ_REQ, G_TOKEN_VALUE, NULL, &value, "missing value in 'define'",
      SEQ_OPT, ';', NULL, NULL, NULL,
      SEQ_END);

  if(scanner->max_parse_errors || !ident || !value)
  {
    g_free(ident);
    g_free(value);
    return;
  }

  if(!defines)
    defines = g_hash_table_new_full((GHashFunc)str_nhash,
        (GEqualFunc)str_nequal, g_free, g_free);

  g_hash_table_insert(defines, ident, value);
}

gpointer config_lookup_ptr ( GScanner *scanner, GHashTable *table )
{
  if(scanner->token != G_TOKEN_IDENTIFIER)
    return NULL;

  return g_hash_table_lookup(table,
        scanner->value.v_identifier);
}

gpointer config_lookup_next_ptr ( GScanner *scanner, GHashTable *table )
{
  if(g_scanner_peek_next_token(scanner) != G_TOKEN_IDENTIFIER)
    return NULL;

  return g_hash_table_lookup(table,
        scanner->next_value.v_identifier);
}
