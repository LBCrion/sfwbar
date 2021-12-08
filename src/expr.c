/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <glob.h>
#include <glib.h>
#include "sfwbar.h"

gchar *expr_token[] = {"Time","Mid","Extract","Disk","Val","Str"};

gdouble expr_parse_num ( GScanner *scanner );
gchar *expr_parse_str ( GScanner *scanner );

gboolean parser_expect_symbol ( GScanner *scanner, gchar symbol, gchar *expr )
{
  if(g_scanner_peek_next_token(scanner)==symbol)
  {
    g_scanner_get_next_token(scanner);
    scanner->max_parse_errors = FALSE;
    return FALSE;
  }
  if(!expr)
    g_scanner_error(scanner,
        "Unexpected token at position %u, expected '%c'",
          g_scanner_cur_position(scanner), symbol );
  else
    g_scanner_error(scanner,
        "%u: Unexpected token in expression %s, expected '%c'",
        g_scanner_cur_position(scanner), expr, symbol );

  return TRUE;
}

/* convert a number to a string with specified number of decimals */
char *expr_dtostr ( double num, gint dec )
  {
  static gchar *format = "%%0.%df";
  static gchar fbuf[16];

  if(dec<0)
    return g_strdup_printf("%f",num);

  if(dec>99)
    dec = 99;

  snprintf(fbuf,15,format,dec);
  return g_strdup_printf(fbuf,num);
  }

/* extract a substring */
char *expr_parse_str_mid ( GScanner *scanner )
{
  gchar *str, *result;
  gint len, c1, c2;

  g_scanner_get_next_token( scanner );
  parser_expect_symbol(scanner,'(',"Mid(String,Number,Number)");
  str = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c1 = expr_parse_num(scanner);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c2 = expr_parse_num(scanner);
  parser_expect_symbol(scanner,')',"Mid(String,Number,Number)");

  if(str==NULL)
    return strdup("");
  len = strlen(str);
  if(c1<0)	/* negative offsets are relative to the end of the string */
    c1+=len;
  if(c2<0)
    c2+=len;
  if(c1<0)	/* ... but very negative offsets must be floored */
    c1=0;
  if(c2<0)
    c2=0;
  if(c1>=len) /* and if offsets are too long, they must be capped */
    c1=len-1;
  if(c2>=len)
    c2=len-1;
  if(c1>c2)
    {
    c2^=c1;	/* swap the ofsets */
    c1^=c2;
    c2^=c1;
    }

  result = strndup( str+c1*sizeof(gchar), (c2-c1+1)*sizeof(gchar));

  g_free(str);

  return result;
  }

/* generate disk space utilization for a device */
gdouble expr_parse_disk ( GScanner *scanner )
{
  gchar *fpath,*param;
  struct statvfs fs;
  gdouble result = 0;

  g_scanner_get_next_token( scanner );
  parser_expect_symbol(scanner,'(',"Disk()");
  fpath = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Disk()");
  param = expr_parse_str(scanner);
  parser_expect_symbol(scanner,')',"Disk()");

  if(statvfs(fpath,&fs)==0 || !param)
  {
    if(!g_ascii_strcasecmp(param,"total"))
      result = fs.f_blocks * fs.f_bsize;
    if(!g_ascii_strcasecmp(param,"free"))
      result = fs.f_bavail * fs.f_bsize;
    if(!g_ascii_strcasecmp(param,"%free"))
      result = (1.0 - (gdouble)fs.f_bavail / (gdouble)fs.f_blocks)*100;
    if(!g_ascii_strcasecmp(param,"%used"))
      result = ((gdouble)fs.f_bavail / (gdouble)fs.f_blocks)*100;
  }

  g_free(fpath);
  g_free(param);

  return result;
}

/* Extract substring using regex */
char *expr_parse_extract( GScanner *scanner )
  {
  gchar *str, *pattern, *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  g_scanner_get_next_token( scanner );
  parser_expect_symbol(scanner,'(',"Extract(String,String)");
  str = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Extract(String,String)");
  pattern = expr_parse_str(scanner);
  parser_expect_symbol(scanner,')',"Extract(String,String)");

  if((str!=NULL)||(pattern!=NULL))
  {
    regex = g_regex_new(pattern,0,0,NULL);
    g_regex_match (regex, str, 0, &match);
    if(g_match_info_matches (match))
      sres = g_match_info_fetch (match, 0);
    g_match_info_free (match);
    g_regex_unref (regex);
  }
  if(sres==NULL)
    sres = strdup("");

  g_free(str);
  g_free(pattern);

  return sres;
  }

/* Get current time string */
char *expr_parse_time ( GScanner *scanner )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str, *tzstr, *format;

  g_scanner_get_next_token( scanner );
  parser_expect_symbol(scanner,'(',"Time([String][,String])");

  if(g_scanner_peek_next_token(scanner)==')')
    format = NULL;
  else
    format = expr_parse_str( scanner );

  if(g_scanner_peek_next_token(scanner)==')')
    tz = NULL;
  else
  {
    parser_expect_symbol(scanner,',',"Time([String][,String])");
    tzstr = expr_parse_str( scanner );
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    tz = g_time_zone_new_identifier( tzstr );
#else
    tz = g_time_zone_new( tzstr );
#endif
    g_free(tzstr);
  }

  parser_expect_symbol(scanner,')',"Time([String][,String])");

  if(tz==NULL)
    time = g_date_time_new_now_local();
  else
  {
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  if(!format)
    str = g_date_time_format ( time, "%a %b %d %H:%M:%S %Y" );
  else
    str = g_date_time_format ( time, format );

  g_free(format);
  g_date_time_unref(time);

  return str;
  }

gchar *expr_parse_str_l1 ( GScanner *scanner )
{
  gchar *str;
  gdouble n1, n2;

  switch((gint)g_scanner_peek_next_token(scanner))
  {
    case G_TOKEN_STRING:
      g_scanner_get_next_token( scanner );
      str = strdup(scanner->value.v_string);
      break;
    case G_TOKEN_STRW:
      g_scanner_get_next_token( scanner );
      parser_expect_symbol(scanner,'(',"Str(Number,Number)");
      n1 = expr_parse_num(scanner);
      parser_expect_symbol(scanner,',',"Str(Number,Number)");
      n2 = expr_parse_num(scanner);
      parser_expect_symbol(scanner,')',"Str(Number,Number)");
      str = expr_dtostr(n1,n2);
      break;
    case G_TOKEN_MIDW:
      str = expr_parse_str_mid( scanner );
      break;
    case G_TOKEN_EXTRACT:
      str = expr_parse_extract ( scanner );
      break;
    case G_TOKEN_TIME:
      str = expr_parse_time ( scanner );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token( scanner );
      str = string_from_name(scanner->value.v_identifier);
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    default:
      g_scanner_get_next_token ( scanner );
      g_scanner_warn(scanner,
          "Unexpected token at position %u, expected a string",
          g_scanner_cur_position(scanner));
      str = strdup("");
  }
  return str;
}

gchar *expr_parse_str ( GScanner *scanner )
{
  gchar *str,*next,*tmp;

  str = expr_parse_str_l1( scanner );
  while(g_scanner_peek_next_token( scanner )=='+')
  {
    g_scanner_get_next_token( scanner );
    next = expr_parse_str_l1( scanner );
    tmp = g_strconcat(str,next,NULL);
    g_free(str);
    g_free(next);
    str=tmp;
  }
  return str;
}

gdouble expr_parse_num_l2 ( GScanner *scanner )
{
  gdouble val;
  gchar *str;

  switch((gint)g_scanner_peek_next_token(scanner) )
  {
    case '+':
      g_scanner_get_next_token(scanner);
      val = expr_parse_num_l2 ( scanner );
      break;
    case '-':
      g_scanner_get_next_token(scanner);
      val = -expr_parse_num_l2 ( scanner );
      break;
    case G_TOKEN_FLOAT: 
      g_scanner_get_next_token ( scanner );
      val = scanner->value.v_float;
      break;
    case '(':
      g_scanner_get_next_token ( scanner );
      val = expr_parse_num ( scanner );
      parser_expect_symbol(scanner, ')',"(Number)");
      break;
    case G_TOKEN_DISK:
      val = expr_parse_disk ( scanner );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    case G_TOKEN_VAL:
      g_scanner_get_next_token ( scanner );
      parser_expect_symbol(scanner,'(',"Val(String)");
      str = expr_parse_str(scanner);
      val = strtod(str,NULL);
      g_free(str);
      parser_expect_symbol(scanner,')',"Val(String)");
      break;
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token( scanner );
      val = numeric_from_name( scanner->value.v_identifier );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    default:
      g_scanner_get_next_token ( scanner );
      g_scanner_warn(scanner,
          "Unexpected token at position %u, expected a number",
          g_scanner_cur_position(scanner));
      val = 0;
  }
  
  return val;
}

gdouble expr_parse_num_l1 ( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_l2 ( scanner );
  while(strchr("*/%",g_scanner_peek_next_token ( scanner )))
  {
    g_scanner_get_next_token ( scanner );
    if(scanner->token == '*')
      val *= expr_parse_num_l2( scanner );
    if(scanner->token == '/')
      val /= expr_parse_num_l2( scanner );
    if(scanner->token == '%')
      val = (gint)val % (gint)expr_parse_num_l2( scanner );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

gdouble expr_parse_num ( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_l1 ( scanner );
  while(strchr("+-",g_scanner_peek_next_token( scanner )))
  {
    g_scanner_get_next_token (scanner );
    if(scanner->token == '+')
      val+=expr_parse_num_l1( scanner );
    if(scanner->token == '-')
      val-=expr_parse_num_l1( scanner );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

gchar *expr_parse( gchar *expr, guint *vcount )
{
  GScanner *scanner;
  gchar *result;

  scanner = g_scanner_new(NULL);
  scanner->config->scan_octal = 0;
  scanner->config->symbol_2_token = 1;
  scanner->config->case_sensitive = 0;
  scanner->config->numbers_2_int = 1;
  scanner->config->int_2_float = 1;

  scanner->config->cset_identifier_nth = g_strconcat(".",
      scanner->config->cset_identifier_nth,NULL);
  scanner->config->cset_identifier_first = g_strconcat("$",
      scanner->config->cset_identifier_first,NULL);

  g_scanner_scope_add_symbol(scanner,0, "Mid", (gpointer)G_TOKEN_MIDW );
  g_scanner_scope_add_symbol(scanner,0, "Extract", (gpointer)G_TOKEN_EXTRACT );
  g_scanner_scope_add_symbol(scanner,0, "Str", (gpointer)G_TOKEN_STRW);
  g_scanner_scope_add_symbol(scanner,0, "Val", (gpointer)G_TOKEN_VAL );
  g_scanner_scope_add_symbol(scanner,0, "Time", (gpointer)G_TOKEN_TIME );
  g_scanner_scope_add_symbol(scanner,0, "Df", (gpointer)G_TOKEN_DISK );
  g_scanner_set_scope(scanner,0);

  scanner->input_name = expr;
  scanner->user_data = vcount;
  *vcount=0;
  g_scanner_input_text(scanner, expr, strlen(expr));

  if((g_scanner_peek_next_token(scanner)==G_TOKEN_FLOAT)||
      (g_scanner_peek_next_token(scanner)==(GTokenType)G_TOKEN_VAL)||
      (g_scanner_peek_next_token(scanner)==(GTokenType)G_TOKEN_LEFT_PAREN)||
      ((g_scanner_peek_next_token(scanner)==G_TOKEN_IDENTIFIER)&&
       (*(scanner->next_value.v_identifier)!='$')))
    result = expr_dtostr(expr_parse_num(scanner),-1);
  else
    result = expr_parse_str(scanner);

  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy( scanner );

  return result;
}
