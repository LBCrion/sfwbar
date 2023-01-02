/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <glib.h>
#include "sfwbar.h"
#include "wintree.h"
#include "module.h"

enum {
  G_TOKEN_TIME    = G_TOKEN_LAST + 1,
  G_TOKEN_MIDW    = G_TOKEN_LAST + 2,
  G_TOKEN_EXTRACT = G_TOKEN_LAST + 3,
  G_TOKEN_DISK    = G_TOKEN_LAST + 4,
  G_TOKEN_VAL     = G_TOKEN_LAST + 5,
  G_TOKEN_STRW    = G_TOKEN_LAST + 6,
  G_TOKEN_ACTIVE  = G_TOKEN_LAST + 7,
  G_TOKEN_PAD     = G_TOKEN_LAST + 8,
  G_TOKEN_IF      = G_TOKEN_LAST + 9,
  G_TOKEN_CACHED  = G_TOKEN_LAST + 10
};


gdouble expr_parse_num ( GScanner *scanner );
gchar *expr_parse_str ( GScanner *scanner );

gboolean parser_expect_symbol ( GScanner *scanner, gint symbol, gchar *expr )
{
  if(g_scanner_peek_next_token(scanner)==symbol)
  {
    g_scanner_get_next_token(scanner);
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

gboolean expr_identifier_is_numeric ( gchar *identifier )
{
  if(!identifier || *identifier == '$')
    return FALSE;
  return module_is_numeric(identifier);
}

gboolean expr_is_numeric ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  return ((scanner->next_token == G_TOKEN_FLOAT)||
      (scanner->next_token == '!')||
      (scanner->next_token == (GTokenType)G_TOKEN_DISK)||
      (scanner->next_token == (GTokenType)G_TOKEN_VAL)||
      (scanner->next_token == (GTokenType)G_TOKEN_LEFT_PAREN)||
      ((scanner->next_token == G_TOKEN_IDENTIFIER)&&
        expr_identifier_is_numeric(scanner->next_value.v_identifier)));
}

/* convert a number to a string with specified number of decimals */
gchar *expr_dtostr ( double num, gint dec )
{
  static const gchar *format = "%%0.%df";
  static gchar fbuf[16];
  static gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

  if(dec<0)
    return g_strdup(g_ascii_dtostr(buf,G_ASCII_DTOSTR_BUF_SIZE,num));

  if(dec>99)
    dec = 99;

  g_snprintf(fbuf,16,format,dec);
  return g_strdup(g_ascii_formatd(buf,G_ASCII_DTOSTR_BUF_SIZE,fbuf,num));
}

gchar *expr_module_function ( GScanner *scanner )
{
  gint i;
  gchar *err;

  if(module_is_function(scanner) && !scanner->max_parse_errors)
  {
    *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
    return module_get_string(scanner);
  }

  i=1;
  err = g_strdup_printf("Unknown Function: %s",scanner->value.v_identifier);

  if(g_scanner_peek_next_token(scanner)=='(')
  {
    g_scanner_get_next_token(scanner);
    while(i && !g_scanner_eof(scanner))
      switch((gint)g_scanner_get_next_token(scanner))
      {
        case '(':
          i++;
          break;
        case ')':
          i--;
          break;
      }
  }
  return err;
}

gchar *expr_parse_cached ( GScanner *scanner )
{
  gchar *ret;
  
  parser_expect_symbol(scanner,'(',"Cached(Identifier)");
  if(parser_expect_symbol(scanner,G_TOKEN_IDENTIFIER,"Cached(Identifier)"))
    return g_strdup("");

  *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
  if(*(scanner->value.v_identifier)=='$')
    ret = scanner_get_string(scanner->value.v_identifier,FALSE);
  else
    ret = expr_dtostr(
        scanner_get_numeric(scanner->value.v_identifier,FALSE),-1);

  parser_expect_symbol(scanner,')',"Cached(Identifier)");
  return ret;
}
  
gchar *expr_parse_if ( GScanner *scanner )
{
  gboolean condition;
  gchar *str, *str2;
  guint vstate = scanner->max_parse_errors;

  parser_expect_symbol(scanner,'(',"If(Condition,Expression,Expression)");
  if(expr_is_numeric(scanner))
    condition = (gboolean)expr_parse_num ( scanner );
  else
  {
    str = expr_parse_str(scanner);
    condition = (gboolean)strtod(str,NULL);
    g_free(str);
  }

  if(!condition)
    scanner->max_parse_errors = 1;
  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  if(expr_is_numeric(scanner))
    str = expr_dtostr(expr_parse_num ( scanner ),-1);
  else
    str = expr_parse_str(scanner);

  if(condition)
    scanner->max_parse_errors = 1;
  else
    scanner->max_parse_errors = vstate;
  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  if(expr_is_numeric(scanner))
    str2 = expr_dtostr(expr_parse_num ( scanner ),-1);
  else
    str2 = expr_parse_str(scanner);

  scanner->max_parse_errors = vstate;
  parser_expect_symbol(scanner,')',"If(Condition,Expression,Expression)");

  if(condition)
  {
    g_free(str2);
    return str;
  }
  else
  {
    g_free(str);
    return str2;
  }
}

/* extract a substring */
gchar *expr_parse_str_mid ( GScanner *scanner )
{
  gchar *str, *result;
  gint len, c1, c2;

  parser_expect_symbol(scanner,'(',"Mid(String,Number,Number)");
  str = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c1 = expr_parse_num(scanner);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c2 = expr_parse_num(scanner);
  parser_expect_symbol(scanner,')',"Mid(String,Number,Number)");

  if(str==NULL)
    return g_strdup("");
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

  result = g_strndup( str+c1*sizeof(gchar), (c2-c1+1)*sizeof(gchar));

  g_free(str);

  return result;
}

/* generate disk space utilization for a device */
gdouble expr_parse_disk ( GScanner *scanner )
{
  gchar *fpath,*param;
  struct statvfs fs;
  gdouble result = 0;

  parser_expect_symbol(scanner,'(',"Disk()");
  fpath = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Disk()");
  param = expr_parse_str(scanner);
  parser_expect_symbol(scanner,')',"Disk()");

  if(statvfs(fpath,&fs)==0 || !param)
  {
    if(!g_ascii_strcasecmp(param,"total"))
      result = fs.f_blocks * fs.f_frsize;
    if(!g_ascii_strcasecmp(param,"avail"))
      result = fs.f_bavail * fs.f_bsize;
    if(!g_ascii_strcasecmp(param,"free"))
      result = fs.f_bfree * fs.f_bsize;
    if(!g_ascii_strcasecmp(param,"%avail"))
      result = ((gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;
    if(!g_ascii_strcasecmp(param,"%used"))
      result = (1.0 - (gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;
  }

  g_free(fpath);
  g_free(param);

  return result;
}

gchar *expr_parse_padstr ( GScanner *scanner )
{
  gchar *str, *result;
  gint n;

  parser_expect_symbol(scanner,'(',"Pad(String,Number)");
  str = expr_parse_str(scanner);
  parser_expect_symbol(scanner,',',"Pad(String,Number)");
  n = expr_parse_num(scanner);
  parser_expect_symbol(scanner,')',"Pad(String,Number)");

  result = g_strdup_printf("%*s",n,str);
  g_free(str);
  return result;
}

/* Extract substring using regex */
gchar *expr_parse_extract( GScanner *scanner )
{
  gchar *str, *pattern, *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

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
    sres = g_strdup("");

  g_free(str);
  g_free(pattern);

  return sres;
}

gchar *expr_parse_active ( GScanner *scanner )
{
  parser_expect_symbol(scanner,'(',"ActiveWin()");
  parser_expect_symbol(scanner,')',"ActiveWin()");
  return g_strdup(wintree_get_active());
}

/* Get current time string */
gchar *expr_parse_time ( GScanner *scanner )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str, *tzstr, *format;

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

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_STRING:
      str = g_strdup(scanner->value.v_string);
      break;
    case G_TOKEN_STRW:
      parser_expect_symbol(scanner,'(',"Str(Number,Number)");
      n1 = expr_parse_num(scanner);
      parser_expect_symbol(scanner,',',"Str(Number,Number)");
      n2 = expr_parse_num(scanner);
      parser_expect_symbol(scanner,')',"Str(Number,Number)");
      str = expr_dtostr(n1,(gint)n2);
      break;
    case G_TOKEN_ACTIVE:
      str = expr_parse_active ( scanner );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    case G_TOKEN_MIDW:
      str = expr_parse_str_mid( scanner );
      break;
    case G_TOKEN_EXTRACT:
      str = expr_parse_extract ( scanner );
      break;
    case G_TOKEN_PAD:
      str = expr_parse_padstr ( scanner );
      break;
    case G_TOKEN_TIME:
      str = expr_parse_time ( scanner );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    case G_TOKEN_CACHED:
      str = expr_parse_cached ( scanner );
      break;
    case G_TOKEN_IF:
      str = expr_parse_if ( scanner );
      break;
    case G_TOKEN_IDENTIFIER:
      if(g_scanner_peek_next_token(scanner)=='(')
        str = expr_module_function(scanner);
      else if(scanner->max_parse_errors)
        str = g_strdup("");
      else
      {
        str = scanner_get_string(scanner->value.v_identifier,TRUE);
        *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      }
      break;
    default:
      g_scanner_warn(scanner,
          "Unexpected token at position %u, expected a string",
          g_scanner_cur_position(scanner));
      str = g_strdup("");
  }
  return str;
}

gchar *expr_parse_str ( GScanner *scanner )
{
  gchar *str,*next,*tmp;

  str = expr_parse_str_l1( scanner );

  while(g_scanner_peek_next_token( scanner )=='+' ||
      g_scanner_peek_next_token( scanner )=='=')
  {
    g_scanner_get_next_token( scanner );
    switch((gint)scanner->token)
    {
      case '+':
        next = expr_parse_str_l1( scanner );
        tmp = g_strconcat(str,next,NULL);
        g_free(str);
        g_free(next);
        str=tmp;
        break;
      case '=':
        next = expr_parse_str_l1( scanner );
        tmp = str;
        if(!g_strcmp0(tmp,next))
          str = g_strdup("1");
        else
          str = g_strdup("0");
        g_free(tmp);
        g_free(next);
        break;
    }
  }
  return str;
}

gdouble expr_parse_num_value ( GScanner *scanner )
{
  gdouble val;
  gchar *str;

  switch((gint)g_scanner_get_next_token(scanner) )
  {
    case '+':
      val = expr_parse_num_value ( scanner );
      break;
    case '-':
      val = -expr_parse_num_value ( scanner );
      break;
    case '!':
      val = !expr_parse_num ( scanner );
      break;
    case G_TOKEN_FLOAT: 
      val = scanner->value.v_float;
      break;
    case '(':
      val = expr_parse_num ( scanner );
      parser_expect_symbol(scanner, ')',"(Number)");
      break;
    case G_TOKEN_DISK:
      val = expr_parse_disk ( scanner );
      *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      break;
    case G_TOKEN_VAL:
      parser_expect_symbol(scanner,'(',"Val(String)");
      str = expr_parse_str(scanner);
      val = strtod(str,NULL);
      g_free(str);
      parser_expect_symbol(scanner,')',"Val(String)");
      break;
    case G_TOKEN_CACHED:
      str = expr_parse_cached(scanner);
      val = strtod(str,NULL);
      g_free(str);
      break;
    case G_TOKEN_IF:
      str = expr_parse_if(scanner);
      val = strtod(str,NULL);
      g_free(str);
      break;
    case G_TOKEN_IDENTIFIER:
      if(g_scanner_peek_next_token(scanner)=='(')
      {
        str = expr_module_function(scanner);
        val = g_strtod(str,NULL);
        g_free(str);
      }
      else if(scanner->max_parse_errors)
        val = 0;
      else
      {
        val = scanner_get_numeric( scanner->value.v_identifier,TRUE );
        *((guint *)scanner->user_data) = *((guint *)scanner->user_data) + 1;
      }
      break;
    default:
      g_scanner_warn(scanner,
          "Unexpected token at position %u, expected a number",
          g_scanner_cur_position(scanner));
      val = 0;
  }
  
  return val;
}


gdouble expr_parse_num_factor ( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_value ( scanner );
  while(strchr("*/%",g_scanner_peek_next_token ( scanner )))
  {
    g_scanner_get_next_token ( scanner );
    if(scanner->token == '*')
      val *= expr_parse_num_value( scanner );
    if(scanner->token == '/')
      val /= expr_parse_num_value( scanner );
    if(scanner->token == '%')
      val = (gint)val % (gint)expr_parse_num_value( scanner );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

gdouble expr_parse_num_sum ( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_factor ( scanner );
  while(strchr("+-",g_scanner_peek_next_token( scanner )))
  {
    g_scanner_get_next_token (scanner );
    if(scanner->token == '+')
      val+=expr_parse_num_factor( scanner );
    if(scanner->token == '-')
      val-=expr_parse_num_factor( scanner );
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

gdouble expr_parse_num_compare ( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_sum ( scanner );
  while(strchr("<>=",g_scanner_peek_next_token ( scanner )))
  {
    switch((gint)g_scanner_get_next_token ( scanner ))
    {
      case '>':
        if( g_scanner_peek_next_token( scanner ) == '=' )
        {
          g_scanner_get_next_token( scanner );
          val = (gdouble)(val >= expr_parse_num_sum ( scanner ));
        }
        else
          val = (gdouble)(val > expr_parse_num_sum ( scanner ));
        break;
      case '<':
        if( g_scanner_peek_next_token( scanner ) == '=' )
        {
          g_scanner_get_next_token( scanner );
          val = (gdouble)(val <= expr_parse_num_sum ( scanner ));
        }
        else
          val = (gdouble)(val < expr_parse_num_sum ( scanner ));
        break;
      case '=':
        val = (gdouble)(val == expr_parse_num_sum ( scanner ));
        break;
    }
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

gdouble expr_parse_num( GScanner *scanner )
{
  gdouble val;

  val = expr_parse_num_compare ( scanner );
  while(strchr("&|",g_scanner_peek_next_token ( scanner )))
  {
    switch((gint)g_scanner_get_next_token ( scanner ))
    {
      case '&':
        val = expr_parse_num_compare ( scanner ) && val;
        break;
      case '|':
        val = expr_parse_num_compare ( scanner ) || val;
        break;
    }
    if(g_scanner_eof(scanner))
      break;
  }
  return val;
}

void **expr_module_parameters ( GScanner *scanner, gchar *spec, gchar *name )
{
  void **params;
  gint i;

  if(g_scanner_peek_next_token(scanner)!='(')
  {
    g_scanner_error(scanner,"missing '(' after function call: %s",name);
    return NULL;
  }
  else
    g_scanner_get_next_token(scanner);

  if(!spec)
    params = NULL;
  else
  {
    params = g_malloc0(strlen(spec)*sizeof(gpointer));
    for(i=0;spec[i];i++)
      if(g_scanner_peek_next_token(scanner)!=')')
      {
        if(g_ascii_tolower(spec[i])=='n' && expr_is_numeric(scanner))
        {
          params[i] = g_malloc0(sizeof(gdouble));
          *((gdouble *)params[i]) = expr_parse_num(scanner);
        }
        else if(g_ascii_tolower(spec[i])=='s' && !expr_is_numeric(scanner))
          params[i] = expr_parse_str(scanner);
        else if(!g_ascii_islower(spec[i]))
          g_scanner_error(scanner,"invalid type in parameter %d of %s",i,name);
        if(params[i] && g_scanner_peek_next_token(scanner)==',')
          g_scanner_get_next_token(scanner);
      }
  }

  if(g_scanner_peek_next_token(scanner)!=')')
    g_scanner_error(scanner,"missing ')' after function call: %s",name);
  else
    g_scanner_get_next_token(scanner);

  return params;
}

static GScanner *expr_scanner_new ( void )
{
  GScanner *scanner;

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
  g_scanner_scope_add_symbol(scanner,0, "Disk", (gpointer)G_TOKEN_DISK );
  g_scanner_scope_add_symbol(scanner,0, "ActiveWin", (gpointer)G_TOKEN_ACTIVE);
  g_scanner_scope_add_symbol(scanner,0, "Pad", (gpointer)G_TOKEN_PAD );
  g_scanner_scope_add_symbol(scanner,0, "If", (gpointer)G_TOKEN_IF );
  g_scanner_scope_add_symbol(scanner,0, "Cached", (gpointer)G_TOKEN_CACHED );
  g_scanner_set_scope(scanner,0);

  return scanner;
}

gchar *expr_parse( gchar *expr, guint *vcount )
{
  GScanner *scanner;
  gchar *result;
  guint vholder;

  scanner = expr_scanner_new();

  scanner->input_name = expr;
  scanner->max_parse_errors = 0;

  if(!vcount)
    vcount = &vholder;
  scanner->user_data = vcount;
  *vcount=0;
 
  g_scanner_input_text(scanner, expr, strlen(expr));

  if( expr_is_numeric(scanner) )
    result = expr_dtostr(expr_parse_num(scanner),-1);
  else
    result = expr_parse_str(scanner);

  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy( scanner );

  g_debug("expr: \"%s\" = \"%s\"",expr,result);

  return result;
}
