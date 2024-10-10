/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include <glib.h>
#include "config.h"
#include "scanner.h"
#include "ipc/sway.h"
#include "sfwbar.h"
#include "client.h"

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

static void config_var ( GScanner *scanner, ScanFile *file )
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
    scanner_var_new(vname, file, pattern, type, flag);

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

static ScanFile *config_source ( GScanner *scanner, gint source )
{
  ScanFile *file;
  gchar *fname = NULL, *trigger = NULL;
  gint flags = 0;

  switch(source)
  {
    case SO_FILE:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_OPT, -2, (parse_func)config_source_flags, &flags, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      break;
    case SO_CLIENT:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_OPT, ',', NULL, NULL, NULL,
          SEQ_CON, G_TOKEN_STRING, NULL, &trigger, NULL,
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      break;
    default:
      config_parse_sequence(scanner,
          SEQ_REQ, '(', NULL, NULL, "Missing '(' after source",
          SEQ_REQ, G_TOKEN_STRING, NULL, &fname, "Missing file in a source",
          SEQ_REQ, ')', NULL, NULL, "Missing ')' after source",
          SEQ_REQ, '{', NULL, NULL, "Missing '{' after source",
          SEQ_END);
      break;
  }

  if(scanner->max_parse_errors)
  {
    g_free(fname);
    g_free(trigger);
    return NULL;
  }

  file = scanner_file_new(source, fname, trigger, flags);
  while(!config_is_section_end(scanner))
    config_var(scanner, file);

  return file;
}

gboolean config_scanner_source ( GScanner *scanner )
{
  switch(config_lookup_key(scanner, config_scanner_keys))
  {
    case G_TOKEN_FILE:
      config_source(scanner, SO_FILE);
      return TRUE;
    case G_TOKEN_EXEC:
      config_source(scanner, SO_EXEC);
      return TRUE;
    case G_TOKEN_MPDCLIENT:
      client_mpd(config_source(scanner, SO_CLIENT));
      return TRUE;
    case G_TOKEN_SWAYCLIENT:
      sway_ipc_client_init(config_source(scanner, SO_CLIENT));
      return TRUE;
    case G_TOKEN_EXECCLIENT:
      client_exec(config_source(scanner, SO_CLIENT));
      return TRUE;
    case G_TOKEN_SOCKETCLIENT:
      client_socket(config_source(scanner, SO_CLIENT));
      return TRUE;
  }
  return FALSE;
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
