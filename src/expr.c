/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
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
  G_TOKEN_MIDW,
  G_TOKEN_EXTRACT,
  G_TOKEN_DISK,
  G_TOKEN_VAL,
  G_TOKEN_STRW,
  G_TOKEN_ACTIVE,
  G_TOKEN_PAD,
  G_TOKEN_IF,
  G_TOKEN_CACHED,
  G_TOKEN_LOOKUP,
  G_TOKEN_MAP,
  G_TOKEN_HAVEFUNCTION,
  G_TOKEN_HAVEVAR

};

enum {
  EXPR_STRING,
  EXPR_NUMERIC,
  EXPR_VARIANT
};

typedef struct expr_state {
  gint type;
  gint vcount;
  gboolean error;
  gboolean ignore;
} ExprState;

#define E_STATE(x) ((ExprState *)x->user_data)

static gdouble expr_parse_num ( GScanner *scanner, gdouble * );
static gchar *expr_parse_str ( GScanner *scanner, gchar * );
static gchar *expr_parse_variant ( GScanner *scanner );
static gchar *expr_parse_root ( GScanner *scanner );

static gdouble expr_str_to_num ( gchar *str )
{
  gdouble val;
 
  if(!str)
    return 0;

  val = strtod(str,NULL);
  g_free(str);
  return val;
}

static gboolean expr_is_numeric ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if (scanner->next_token == G_TOKEN_FLOAT ||
      scanner->next_token == '!' ||
      scanner->next_token == '(' ||
      scanner->next_token == (GTokenType)G_TOKEN_DISK ||
      scanner->next_token == (GTokenType)G_TOKEN_HAVEFUNCTION ||
      scanner->next_token == (GTokenType)G_TOKEN_HAVEVAR ||
      scanner->next_token == (GTokenType)G_TOKEN_VAL ||
      scanner->next_token == (GTokenType)G_TOKEN_LEFT_PAREN)
    return TRUE;

  if(scanner->next_token != G_TOKEN_IDENTIFIER)
    return FALSE;

  if(scanner_is_variable(scanner->next_value.v_identifier))
    return (*(scanner->next_value.v_identifier) != '$');
  if(module_is_function(scanner->next_value.v_identifier))
    return module_is_numeric(scanner->next_value.v_identifier);
  return FALSE;
}

static gboolean expr_is_string ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if(scanner->next_token == G_TOKEN_STRING ||
      scanner->next_token == (GTokenType)G_TOKEN_TIME ||
      scanner->next_token == (GTokenType)G_TOKEN_MIDW ||
      scanner->next_token == (GTokenType)G_TOKEN_EXTRACT ||
      scanner->next_token == (GTokenType)G_TOKEN_STRW ||
      scanner->next_token == (GTokenType)G_TOKEN_ACTIVE ||
      scanner->next_token == (GTokenType)G_TOKEN_MAP ||
      scanner->next_token == (GTokenType)G_TOKEN_LOOKUP ||
      scanner->next_token == (GTokenType)G_TOKEN_PAD)
    return TRUE;

  if(scanner->next_token != G_TOKEN_IDENTIFIER)
    return FALSE;

  if(scanner_is_variable(scanner->next_value.v_identifier))
    return (*(scanner->next_value.v_identifier) == '$');
  if(module_is_function(scanner->next_value.v_identifier))
    return !module_is_numeric(scanner->next_value.v_identifier);
  return FALSE;
}

static gboolean expr_is_variant ( GScanner *scanner )
{
  g_scanner_peek_next_token(scanner);
  if(scanner->next_token == (GTokenType)G_TOKEN_IF ||
      scanner->next_token == (GTokenType)G_TOKEN_CACHED)
    return TRUE;

  if(scanner->next_token != G_TOKEN_IDENTIFIER)
    return FALSE;
  if(scanner_is_variable(scanner->next_value.v_identifier))
    return FALSE;
  if(module_is_function(scanner->next_value.v_identifier))
    return FALSE;

  return TRUE;
}

static gboolean parser_expect_symbol ( GScanner *scanner, gint symbol,
    gchar *expr )
{
  if(g_scanner_peek_next_token(scanner)==symbol)
  {
    g_scanner_get_next_token(scanner);
    return FALSE;
  }
  if(!expr)
    g_scanner_error(scanner,
        "Unexpected token '%c'(%d) at position %u, expected '%c'",
        scanner->next_token,scanner->next_token,
        g_scanner_cur_position(scanner), symbol );
  else
    g_scanner_error(scanner,
        "%u: Unexpected token '%c'(%d) in expression %s, expected '%c'",
        scanner->next_token,scanner->next_token,
        g_scanner_cur_position(scanner), expr, symbol );

  return TRUE;
}

/* convert a number to a string with specified number of decimals */
static gchar *expr_dtostr ( double num, gint dec )
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

static gchar *expr_module_function ( GScanner *scanner )
{
  gint i;
  gchar *err;

  if(module_is_function(scanner->value.v_identifier) &&
      !E_STATE(scanner)->ignore)
  {
    E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + 1;
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

static gchar *expr_parse_map ( GScanner *scanner )
{
  gchar *match, *result, *comp;
  guint vstate = E_STATE(scanner)->ignore;

  parser_expect_symbol(scanner,'(',"Map(...)");
  match = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Map(value,...)");
  result = NULL;

  while(scanner->token==',' && g_scanner_peek_next_token(scanner)!=')')
  {
    comp = expr_parse_str(scanner,NULL);
    if(g_scanner_peek_next_token(scanner)==')')
    {
      if(!result)
        result = comp;
      break;
    }
    parser_expect_symbol(scanner,',',"Map(... match , string ...)");
    if(!g_strcmp0(comp,match))
      result = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = vstate;
    }
    g_free(comp);
    if(g_scanner_peek_next_token(scanner)==',')
      g_scanner_get_next_token(scanner);
  }
  g_free(match);
  return result;
}

static gchar *expr_parse_lookup ( GScanner *scanner )
{
  gchar *str;
  gdouble value, comp;
  guint vstate = E_STATE(scanner)->ignore;

  parser_expect_symbol(scanner,'(',"Lookup(...)");
  value = expr_parse_num ( scanner, NULL );
  parser_expect_symbol(scanner,',',"Lookup(value,...)");
  str = NULL;

  while (scanner->token==',' && expr_is_numeric(scanner))
  {
    comp = expr_parse_num(scanner, NULL);
    parser_expect_symbol(scanner,',',"Lookup(... threshold, value ...)");
    if(comp<value && !str)
      str = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = vstate;
    }
    if(g_scanner_peek_next_token(scanner)==',')
      g_scanner_get_next_token(scanner);
  }
  if(scanner->token==',')
  {
    if(!str)
      str = expr_parse_str(scanner,NULL);
    else
    {
      E_STATE(scanner)->ignore = 1;
      g_free(expr_parse_str(scanner,NULL));
      E_STATE(scanner)->ignore = vstate;
    }
  }

  parser_expect_symbol(scanner,')',"Lookup(...)");

  return str?str:g_strdup("");
}

static gchar *expr_parse_cached ( GScanner *scanner )
{
  gchar *ret;
  guint vcount = 0;
  
  parser_expect_symbol(scanner,'(',"Cached(Identifier)");
  if(parser_expect_symbol(scanner,G_TOKEN_IDENTIFIER,"Cached(Identifier)"))
    return g_strdup("");

  if(!scanner_is_variable(scanner->value.v_identifier))
  {
    E_STATE(scanner)->type = EXPR_VARIANT;
    ret = g_strdup("");
  }
  else if(*(scanner->value.v_identifier)=='$')
  {
    E_STATE(scanner)->type = EXPR_STRING;
    ret = scanner_get_string(scanner->value.v_identifier,FALSE,&vcount);
  }
  else
  {
    E_STATE(scanner)->type = EXPR_NUMERIC;
    ret = expr_dtostr(
        scanner_get_numeric(scanner->value.v_identifier,FALSE,&vcount),-1);
  }
  E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + vcount;

  parser_expect_symbol(scanner,')',"Cached(Identifier)");
  return ret;
}
  
static gchar *expr_parse_if ( GScanner *scanner )
{
  gboolean condition;
  gchar *str, *str2;
  guint vstate = E_STATE(scanner)->ignore;
  gint stype = E_STATE(scanner)->type;

  parser_expect_symbol(scanner,'(',"If(Condition,Expression,Expression)");
  condition = expr_parse_num(scanner,NULL);
  E_STATE(scanner)->type = stype;

  if(!condition)
    E_STATE(scanner)->ignore = 1;
  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  g_scanner_peek_next_token(scanner);
  str = expr_parse_root(scanner);

  if(condition)
    E_STATE(scanner)->ignore = 1;
  else
    E_STATE(scanner)->ignore = vstate;
  parser_expect_symbol(scanner,',',"If(Condition,Expression,Expression)");
  str2 = expr_parse_root(scanner);

  E_STATE(scanner)->ignore = vstate;
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

static gdouble expr_have_function ( GScanner *scanner )
{
  gchar *tmp;
  gdouble res;

  parser_expect_symbol(scanner,'(',"HaveModule(String)");
  tmp = expr_parse_str(scanner,NULL);
  res = module_is_function(tmp);
  g_free(tmp);
  parser_expect_symbol(scanner,')',"HaveModuke(String)");

  return res;
}

static gdouble expr_have_var ( GScanner *scanner )
{
  gchar *tmp;
  gdouble res;

  parser_expect_symbol(scanner,'(',"HaveModule(String)");
  tmp = expr_parse_str(scanner,NULL);
  res = scanner_is_variable(tmp);
  g_free(tmp);
  parser_expect_symbol(scanner,')',"HaveModuke(String)");

  return res;
}


/* extract a substring */
static gchar *expr_parse_str_mid ( GScanner *scanner )
{
  gchar *str, *result;
  gint len, c1, c2;

  parser_expect_symbol(scanner,'(',"Mid(String,Number,Number)");
  str = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c1 = expr_parse_num(scanner,NULL);
  parser_expect_symbol(scanner,',',"Mid(String,Number,Number)");
  c2 = expr_parse_num(scanner,NULL);
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
static gdouble expr_parse_disk ( GScanner *scanner )
{
  gchar *fpath,*param;
  struct statvfs fs;
  gdouble result = 0;

  parser_expect_symbol(scanner,'(',"Disk()");
  fpath = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Disk()");
  param = expr_parse_str(scanner,NULL);
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

static gchar *expr_parse_padstr ( GScanner *scanner )
{
  gchar *str, *result;
  gint n;

  parser_expect_symbol(scanner,'(',"Pad(String,Number)");
  str = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Pad(String,Number)");
  n = expr_parse_num(scanner,NULL);
  parser_expect_symbol(scanner,')',"Pad(String,Number)");

  result = g_strdup_printf("%*s",n,str);
  g_free(str);
  return result;
}

/* Extract substring using regex */
static gchar *expr_parse_extract( GScanner *scanner )
{
  gchar *str, *pattern, *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  parser_expect_symbol(scanner,'(',"Extract(String,String)");
  str = expr_parse_str(scanner,NULL);
  parser_expect_symbol(scanner,',',"Extract(String,String)");
  pattern = expr_parse_str(scanner,NULL);
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

static gchar *expr_parse_active ( GScanner *scanner )
{
  parser_expect_symbol(scanner,'(',"ActiveWin()");
  parser_expect_symbol(scanner,')',"ActiveWin()");
  return g_strdup(wintree_get_active());
}

/* Get current time string */
static gchar *expr_parse_time ( GScanner *scanner )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str, *tzstr, *format;

  parser_expect_symbol(scanner,'(',"Time([String][,String])");

  if(g_scanner_peek_next_token(scanner)==')')
    format = NULL;
  else
    format = expr_parse_str( scanner, NULL );

  if(g_scanner_peek_next_token(scanner)==')')
    tz = NULL;
  else
  {
    parser_expect_symbol(scanner,',',"Time([String][,String])");
    tzstr = expr_parse_str( scanner, NULL );
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

/* a string has been encountered in a numeric sequence
 * is this a comparison between strings? */
static gdouble expr_parse_num_str ( GScanner *scanner, gchar *prev )
{
  gchar *str1, *str2;
  gint res, pos;

  pos = scanner->position;

  E_STATE(scanner)->type = EXPR_STRING;
  if(!prev)
    str1 = expr_parse_str(scanner,NULL);
  else
    str1 = prev;

  if(g_scanner_peek_next_token(scanner)=='=')
    g_scanner_get_next_token(scanner);
  else
    g_info("expr: %s:%d: Unexpected string where a numer is expected",
        scanner->input_name,pos);
  str2 = expr_parse_str(scanner,NULL);
  res = !g_strcmp0(str1,str2);
  g_free(str1);
  g_free(str2);

  return (gdouble)res;
}

static gchar *expr_parse_str_l1 ( GScanner *scanner )
{
  gchar *str;
  gdouble n1, n2;
  guint vcount;
  gint spos;

  if(expr_is_variant(scanner))
  {
    spos = scanner->position;
    E_STATE(scanner)->type = EXPR_STRING;
    str = expr_parse_variant(scanner);
    if(E_STATE(scanner)->type == EXPR_STRING)
      return str;
    else if(E_STATE(scanner)->type == EXPR_VARIANT)
    {
      g_free(str);
      return g_strdup("");
    }
    else
    {
      g_scanner_warn(scanner,
          "Unexpected variant at position %d, expected a string",spos);
      return g_strdup("");
    }
  }

  switch((gint)g_scanner_get_next_token(scanner))
  {
    case G_TOKEN_STRING:
      str = g_strdup(scanner->value.v_string);
      break;
    case G_TOKEN_STRW:
      parser_expect_symbol(scanner,'(',"Str(Number,Number)");
      n1 = expr_parse_num(scanner,NULL);
      parser_expect_symbol(scanner,',',"Str(Number,Number)");
      n2 = expr_parse_num(scanner,NULL);
      parser_expect_symbol(scanner,')',"Str(Number,Number)");
      str = expr_dtostr(n1,(gint)n2);
      break;
    case G_TOKEN_ACTIVE:
      str = expr_parse_active ( scanner );
      E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + 1;
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
      E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + 1;
      break;
    case G_TOKEN_LOOKUP:
      str = expr_parse_lookup ( scanner );
      break;
    case G_TOKEN_MAP:
      str = expr_parse_map ( scanner );
      break;
    case G_TOKEN_IDENTIFIER:
      if(g_scanner_peek_next_token(scanner)=='(')
        str = expr_module_function(scanner);
      else if(E_STATE(scanner)->ignore)
        str = g_strdup("");
      else
      {
        vcount = 0;
        str = scanner_get_string(scanner->value.v_identifier,TRUE,&vcount);
        E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + vcount;
      }
      break;
    default:
      g_scanner_warn(scanner,
          "Unexpected token %c at position %u, expected a string",scanner->token,
          g_scanner_cur_position(scanner));
      str = g_strdup("");
  }
  return str;
}

static gchar *expr_parse_str ( GScanner *scanner, gchar *prev )
{
  gchar *str,*next,*tmp;

  E_STATE(scanner)->type = EXPR_STRING;
  if(prev)
    str = prev;
  else
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
  E_STATE(scanner)->type = EXPR_STRING;
  return str;
}

static gdouble expr_parse_num_value ( GScanner *scanner )
{
  gdouble val;
  gchar *str;
  guint vcount;

  if(expr_is_string(scanner))
    return expr_parse_num_str(scanner,NULL);

  if(expr_is_variant(scanner))
  {
    E_STATE(scanner)->type = EXPR_NUMERIC;
    str = expr_parse_variant(scanner);
    if(E_STATE(scanner)->type == EXPR_NUMERIC)
      return expr_str_to_num(str);
    /* if type is string, assume it's there for comparison */
    else if(E_STATE(scanner)->type == EXPR_STRING) 
      return expr_parse_num_str(scanner,str);
    else /* if the type is unresolved, cast to numeric zero */
    {
      E_STATE(scanner)->type = EXPR_NUMERIC;
      return 0;
    }
  }

  switch((gint)g_scanner_get_next_token(scanner) )
  {
    case '+':
      return expr_parse_num_value ( scanner );
    case '-':
      return -expr_parse_num_value ( scanner );
    case '!':
      return !expr_parse_num ( scanner, NULL );
    case G_TOKEN_FLOAT: 
      return scanner->value.v_float;
    case '(':
      val = expr_parse_num ( scanner, NULL );
      parser_expect_symbol(scanner, ')',"(Number)");
      return val;
    case G_TOKEN_DISK:
      val = expr_parse_disk ( scanner );
      E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + 1;
      return val;
    case G_TOKEN_HAVEFUNCTION:
      return expr_have_function( scanner );
    case G_TOKEN_HAVEVAR:
      return expr_have_var( scanner );
    case G_TOKEN_VAL:
      parser_expect_symbol(scanner,'(',"Val(String)");
      val = expr_str_to_num(expr_parse_str(scanner, NULL));
      parser_expect_symbol(scanner,')',"Val(String)");
      return val;
    case G_TOKEN_IDENTIFIER:
      if(g_scanner_peek_next_token(scanner)=='(')
        return expr_str_to_num(expr_module_function(scanner));
      else if(E_STATE(scanner)->ignore)
        return 0;
      else
      {
        vcount = 0;
        val = scanner_get_numeric( scanner->value.v_identifier,TRUE, &vcount );
        E_STATE(scanner)->vcount = E_STATE(scanner)->vcount + vcount;
        return val;
      }
    default:
      g_scanner_warn(scanner,
          "Unexpected token at position %u, expected a number",
          g_scanner_cur_position(scanner));
      return 0;
  }
}

static gdouble expr_parse_num_factor ( GScanner *scanner )
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

static gdouble expr_parse_num_sum ( GScanner *scanner )
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

static gdouble expr_parse_num_compare ( GScanner *scanner )
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

static gdouble expr_parse_num( GScanner *scanner, gdouble *prev )
{
  gdouble val;

  E_STATE(scanner)->type = EXPR_NUMERIC;
  if(prev)
    val = *prev;
  else
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
  E_STATE(scanner)->type = EXPR_NUMERIC;
  return val;
}

static gchar *expr_parse_variant_token ( GScanner *scanner )
{
  gchar *str;

  E_STATE(scanner)->type = EXPR_VARIANT;
  switch((gint)g_scanner_peek_next_token(scanner))
  {
    case G_TOKEN_IF:
      g_scanner_get_next_token(scanner);
      str = expr_parse_if(scanner);
      break;
    case G_TOKEN_CACHED:
      g_scanner_get_next_token(scanner);
      str = expr_parse_cached(scanner);
      break;
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token(scanner);
      if(g_scanner_peek_next_token(scanner)=='(')
        str = expr_module_function(scanner);
      else
        str = g_strdup("");
      break;
    default:
      str = g_strdup("");
  }

  return str;
}

static gchar *expr_parse_variant ( GScanner *scanner )
{
  gdouble val;
  gint entry_type;
  gchar *str;

  entry_type = E_STATE(scanner)->type;
  str = expr_parse_variant_token(scanner);

  g_scanner_peek_next_token(scanner);

  while(E_STATE(scanner)->type==EXPR_VARIANT && (scanner->next_token=='=' ||
      (entry_type==EXPR_VARIANT && scanner->next_token == '+')))
  {
    g_scanner_get_next_token(scanner);
    if(scanner->token == '=')
    {
      g_free(expr_parse_root(scanner));
      /* comparing a variant to anything produces a variant result */
      E_STATE(scanner)->type = EXPR_VARIANT;
    }
    else
    {
      g_free(str);
      str = expr_parse_root(scanner);
    }
    g_scanner_peek_next_token(scanner);
  }

  if(!strchr("+-*/%<>|&",scanner->next_token))
    return str;

  if(entry_type != EXPR_VARIANT)
    return str;
  if(E_STATE(scanner)->type == EXPR_STRING && scanner->next_token == '+')
    return expr_parse_str(scanner, str);
  if(E_STATE(scanner)->type == EXPR_NUMERIC ||
      (scanner->next_token && strchr("+-*/%<>|&",scanner->next_token)))
  {
    val = expr_str_to_num(str);
    return expr_dtostr(expr_parse_num(scanner, &val),-1);
  }

  return str;
}

static gchar *expr_parse_root ( GScanner *scanner )
{
  if(expr_is_numeric(scanner) || E_STATE(scanner)->type == EXPR_NUMERIC)
    return expr_dtostr(expr_parse_num(scanner,NULL),1);
  else if(expr_is_string(scanner) || E_STATE(scanner)->type == EXPR_STRING)
    return expr_parse_str(scanner,NULL);
  else if(expr_is_variant(scanner))
    return expr_parse_variant(scanner);
  else
    return g_strdup("");
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
          *((gdouble *)params[i]) = expr_parse_num(scanner,NULL);
        }
        else if(g_ascii_tolower(spec[i])=='s' && !expr_is_numeric(scanner))
          params[i] = expr_parse_str(scanner,NULL);
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
  g_scanner_scope_add_symbol(scanner,0, "Lookup", (gpointer)G_TOKEN_LOOKUP );
  g_scanner_scope_add_symbol(scanner,0, "Map", (gpointer)G_TOKEN_MAP );
  g_scanner_scope_add_symbol(scanner,0, "HaveFunction",
      (gpointer)G_TOKEN_HAVEFUNCTION );
  g_scanner_scope_add_symbol(scanner,0, "HaveVar", (gpointer)G_TOKEN_HAVEVAR );
  g_scanner_set_scope(scanner,0);

  return scanner;
}

gchar *expr_parse( gchar *expr, guint *vcount )
{
  GScanner *scanner;
  gchar *result;
  ExprState state;

  scanner = expr_scanner_new();

  scanner->input_name = expr;
  scanner->user_data = &state;
  E_STATE(scanner)->type = EXPR_VARIANT;
  E_STATE(scanner)->error = FALSE;
  E_STATE(scanner)->ignore = FALSE;
  E_STATE(scanner)->vcount = vcount?*vcount:0;

  g_scanner_input_text(scanner, expr, strlen(expr));

  result = expr_parse_root(scanner);

  if(vcount)
    *vcount = E_STATE(scanner)->vcount;

  g_debug("expr: \"%s\" = \"%s\" (vars: %d)",expr,result,
      E_STATE(scanner)->vcount);

  g_free(scanner->config->cset_identifier_nth);
  g_free(scanner->config->cset_identifier_first);
  g_scanner_destroy( scanner );

  return result;
}

gboolean expr_cache ( gchar **expr, gchar **cache, guint *vc )
{
  gchar *eval;
  guint vcount = 0;

  if(!expr || !*expr)
    return FALSE;

  eval = expr_parse(*expr, &vcount);
  if(!vcount)
  {
    g_free(*expr);
    *expr = NULL;
  }
  if(vc)
    *vc = vcount;
  if(g_strcmp0(eval,*cache))
  {
    g_free(*cache);
    *cache = eval;
    return TRUE;
  }
  g_free(eval);

  return FALSE;
}
