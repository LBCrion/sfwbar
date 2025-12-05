/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "client.h"
#include "config.h"
#include "scanner.h"
#include "ipc/sway.h"
#include "util/string.h"

static gboolean config_var_flag ( GScanner *scanner, gint *flag )
{
  gint fval;

  if( !(fval = config_lookup_next_key(scanner, config_var_types)) )
    return FALSE;

  g_scanner_get_next_token(scanner);
  *flag = fval;
  return TRUE;
}

static gboolean config_var_type (GScanner *scanner, gint *type )
{
  g_scanner_get_next_token(scanner);
  if( !(*type = config_lookup_key(scanner, config_scanner_types)) )
    g_scanner_error(scanner,"invalid parser");
  return !scanner->max_parse_errors;
}

static void config_var ( GScanner *scanner, source_t *file )
{
  gchar *vname, *pattern = NULL;
  guint type;
  gint flag = VT_LAST;

  config_parse_sequence(scanner,
      SEQ_REQ, G_TOKEN_IDENTIFIER, NULL, &vname,
        "invalid variable identifier in scanner",
      SEQ_REQ, '=', NULL, NULL, "Missing '=' in variable declaration",
      SEQ_REQ, -2, (parse_func)config_var_type, &type, NULL,
      SEQ_REQ, '(', NULL, NULL, "Missing '(' after parser",
      SEQ_END);

  if(scanner->max_parse_errors)
  {
    g_free(vname);
    return;
  }

  switch(type)
  {
    case G_TOKEN_REGEX:
    case G_TOKEN_JSON:
      config_parse_sequence(scanner,
          SEQ_REQ, G_TOKEN_STRING, NULL, &pattern, "Missing pattern in parser",
          SEQ_OPT, ',', NULL, NULL, NULL,
          SEQ_CON, -2, (parse_func)config_var_flag, &flag, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after parser",
          SEQ_OPT, ';', NULL, NULL, NULL,
          SEQ_END);
      break;
    case G_TOKEN_GRAB:
      config_parse_sequence(scanner,
          SEQ_OPT, -2, (parse_func)config_var_flag, &flag, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after parser",
          SEQ_OPT, ';', NULL, NULL, NULL,
          SEQ_END);
      break;
    default:
      g_scanner_error(scanner, "invalid parser for variable %s", vname);
  }

  if(!scanner->max_parse_errors)
    scanner_var_new(vname, file, pattern, type, flag, SCANNER_STORE(scanner));

  g_free(vname);
  g_free(pattern);
}

static gboolean config_source_flags ( GScanner *scanner, gint *flags )
{
  gint flag;

  while (config_check_and_consume(scanner, ','))
  {
    g_scanner_get_next_token(scanner);

    if( (flag = config_lookup_key(scanner, config_scanner_flags)) )
      *flags |= flag;
    else
        g_scanner_error(scanner, "invalid flag in source");
  }
  return !scanner->max_parse_errors;
}

gboolean config_scanner_source ( GScanner *scanner )
{
  source_t *src;
  gchar *fname = NULL, *trigger = NULL;
  gint type, flags = 0;

  if( !(type = config_lookup_key(scanner, config_scanner_keys)) )
    return FALSE;

  if(!config_expect_token(scanner, '(', "Missing '(' in source declaration"))
    return FALSE;

  if(!config_expect_token(scanner, G_TOKEN_STRING, "Missing file in source declaration"))
    return FALSE;

  fname = g_strdup(scanner->value.v_string);

  while(config_check_and_consume(scanner, ','))
  {
    if(config_check_and_consume(scanner, G_TOKEN_STRING))
      str_assign(&trigger, g_strdup(scanner->value.v_string));
    else
      config_source_flags(scanner, &flags);
  }

  if(config_expect_token(scanner, ')', "Missing ')' in source declaration") &&
      config_expect_token(scanner, '{', "Missing '{' in source declaration"))
  {
    if(type == G_TOKEN_FILE)
      src = scanner_file_new(fname, flags);
    else if(type == G_TOKEN_EXEC)
      src = scanner_exec_new(fname);
    else if(type == G_TOKEN_EXECCLIENT)
      src = client_exec(fname, trigger);
    else if(type == G_TOKEN_SOCKETCLIENT)
      src = client_socket(fname, trigger);
    else if(type == G_TOKEN_SWAYCLIENT)
      src = sway_ipc_client_init();
    else if(type == G_TOKEN_MPDCLIENT)
      src = client_mpd(fname);
    else
      src = NULL;

    if(src)
      while(!config_is_section_end(scanner))
        config_var(scanner, src);
  }

/*  switch(type)
  {
    case G_TOKEN_FILE:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_OPT, -2, (parse_func)config_source_flags, &flags, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      if(!scanner->max_parse_errors)
        file = scanner_file_new(SO_FILE, fname, flags);
      break;
    case G_TOKEN_EXEC:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      if(!scanner->max_parse_errors)
        file = scanner_file_new(SO_EXEC, fname, 0);
      break;
    default:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_OPT, ',', NULL, NULL, NULL,
          SEQ_CON, G_TOKEN_STRING, NULL, &trigger, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      if(!scanner->max_parse_errors)
      {
        if(type == G_TOKEN_EXECCLIENT)
          file = client_exec(fname, trigger);
        else if(type == G_TOKEN_SOCKETCLIENT)
          file = client_socket(fname, trigger);
        else if(type == G_TOKEN_SWAYCLIENT)
          file = sway_ipc_client_init();
        else if(type == G_TOKEN_MPDCLIENT)
          file = client_mpd(fname);
      }
      break;
  }*/

  g_free(fname);
  g_free(trigger);

  return !!type;
}

void config_scanner ( GScanner *scanner )
{
  scanner->max_parse_errors = FALSE;

  if(!config_expect_token(scanner, '{', "Missing '{' after 'scanner'"))
    return;

  while(!config_is_section_end(scanner))
  {
    g_scanner_get_next_token(scanner);
    if(!config_scanner_source(scanner))
      g_message("Invalid source in scanner");
  }
}
