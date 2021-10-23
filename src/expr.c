/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <glob.h>
#include <glib.h>
#include "sfwbar.h"

enum {
  G_TOKEN_TIME    = G_TOKEN_LAST + 1,
  G_TOKEN_MIDW    = G_TOKEN_LAST + 2,
  G_TOKEN_EXTRACT = G_TOKEN_LAST + 3,
  G_TOKEN_DF      = G_TOKEN_LAST + 4,
  G_TOKEN_VAL     = G_TOKEN_LAST + 5,
  G_TOKEN_STRW    = G_TOKEN_LAST + 6
};

gdouble expr_parse_num_l1 ( struct context *context );
gchar *expr_parse_str_l1 ( struct context *context );

gboolean parser_expect_symbol ( struct context *context, gchar symbol )
{
  if(g_scanner_peek_next_token(context->escan)==symbol)
  {
    g_scanner_get_next_token(context->escan);
    return TRUE;
  }
  g_scanner_warn(context->escan, "Unexpected token at position %u, expected '%c'",
      g_scanner_cur_position(context->escan), symbol );
  return FALSE;
}

/* extract a substring */
char *expr_parse_str_mid ( struct context *context )
{
  gchar *str, *res;
  gint len, c1, c2;

  g_scanner_get_next_token( context->escan );
  parser_expect_symbol(context,'(');
  str = expr_parse_str_l1(context);
  parser_expect_symbol(context,',');
  c1 = expr_parse_num_l1(context);
  parser_expect_symbol(context,',');
  c2 = expr_parse_num_l1(context);
  parser_expect_symbol(context,')');

  if(str==NULL)
    return NULL;
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

  res=g_malloc(c2-c1+2);
  strncpy(res,str+c1*sizeof(char),(c2-c1+1)*sizeof(char));
  *(res+(c2-c1+1)*sizeof(char))='\0';

  g_free(str);

  return res;
  }

/* generate disk space utilization for a device */
char *expr_parse_df ( struct context *context )
{
  gchar *fpath;
  struct statvfs fs;

  g_scanner_get_next_token( context->escan );
  parser_expect_symbol(context,'(');
  fpath = expr_parse_str_l1(context);
  parser_expect_symbol(context,')');

  if(statvfs(fpath,&fs)!=0)
    return g_strdup("");

  g_free(fpath);

  return g_strdup_printf("%ld %ld %ld %02Lf%%",fs.f_blocks*fs.f_bsize,
      fs.f_bavail*fs.f_bsize, (fs.f_blocks-fs.f_bavail)*fs.f_bsize,
      (1.0-(long double)fs.f_bavail/(long double)fs.f_blocks)*100);
}

/* Extract substring using regex */
char *expr_parse_extract( struct context *context )
  {
  gchar *str, *pattern, *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  g_scanner_get_next_token( context->escan );
  parser_expect_symbol(context,'(');
  str = expr_parse_str_l1(context);
  parser_expect_symbol(context,',');
  pattern = expr_parse_str_l1(context);
  parser_expect_symbol(context,')');

  if((str!=NULL)||(pattern!=NULL))
  {
    regex = g_regex_new(pattern,0,0,NULL);
    g_regex_match (regex, str, 0, &match);
    if(g_match_info_matches (match))
      sres = g_match_info_fetch (match, 0);
    g_match_info_free (match);
    g_regex_unref (regex);
  }

  g_free(str);
  g_free(pattern);

  return sres;
  }

/* Get current time string */
char *expr_parse_time ( struct context *context )
  {
  time_t tp;
  gchar *str, *tz, *oldtz;

  g_scanner_get_next_token( context->escan );
  parser_expect_symbol(context,'(');
  if(g_scanner_peek_next_token(context->escan)==')')
    tz = NULL;
  else
    tz = expr_parse_str_l1(context);
  parser_expect_symbol(context,')');

  if(tz!=NULL)
  {
    oldtz = g_strdup(getenv("TZ"));
    setenv("TZ",tz,1);
    tzset();
  }
  if((str=malloc(26*sizeof(char)))==NULL)
    return NULL;
  time(&tp);
  ctime_r(&tp,str);
  if(tz!=NULL)
  {
    if(oldtz!=NULL)
    {
      setenv("TZ",oldtz,1);
      g_free(oldtz);
    }
    else
      unsetenv("TZ");
    tzset();
  }

  g_free(tz);

  return str;
  }

gchar *expr_parse_str ( struct context *context )
{
  gchar *str;
  gdouble n1, n2;
  switch((gint)g_scanner_peek_next_token(context->escan))
  {
    case G_TOKEN_STRING:
      g_scanner_get_next_token( context->escan );
      str = strdup(context->escan->value.v_string);
      break;
    case G_TOKEN_STRW:
      g_scanner_get_next_token( context->escan );
      parser_expect_symbol(context,'(');
      n1 = expr_parse_num_l1(context);
      parser_expect_symbol(context,',');
      n2 = expr_parse_num_l1(context);
      parser_expect_symbol(context,')');
      str = numeric_to_str(n1,n2);
      break;
    case G_TOKEN_MIDW:
      str = expr_parse_str_mid( context );
      break;
    case G_TOKEN_DF:
      str = expr_parse_df ( context );
      break;
    case G_TOKEN_EXTRACT:
      str = expr_parse_extract ( context );
      break;
    case G_TOKEN_TIME:
      str = expr_parse_time ( context );
      break;
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token( context->escan );
      str = string_from_name(context,context->escan->value.v_identifier);
      break;
    default:
      g_scanner_get_next_token ( context->escan );
      str = strdup("");
  }
  return str;
}

gchar *expr_parse_str_l1 ( struct context *context )
{
  gchar *str,*next,*tmp;;
  str = expr_parse_str( context );
  while(g_scanner_peek_next_token( context->escan )=='+')
  {
    g_scanner_get_next_token( context->escan );
    next = expr_parse_str( context );
    tmp = g_strconcat(str,next,NULL);
    g_free(str);
    g_free(next);
    str=tmp;
  }
  return str;
}

gdouble expr_parse_value ( struct context *context )
{
  gdouble val;
  gint sign = 1;

  switch((gint)g_scanner_peek_next_token(context->escan) )
  {
    case '-':
      g_scanner_get_next_token(context->escan);
      sign = -1;
    case G_TOKEN_FLOAT: 
      g_scanner_get_next_token ( context->escan );
      val = context->escan->value.v_float * sign;
      break;
    case '(':
      g_scanner_get_next_token ( context->escan );
      val = expr_parse_num_l1 ( context );
      parser_expect_symbol(context, ')');
      break;
    case G_TOKEN_VAL:
      g_scanner_get_next_token ( context->escan );
      parser_expect_symbol(context,'(');
      gchar *str = expr_parse_str_l1(context);
      val = strtod(str,NULL);
      g_free(str);
      parser_expect_symbol(context,')');
      break;
    case G_TOKEN_IDENTIFIER:
      g_scanner_get_next_token( context->escan );
      val = numeric_from_name(context, context->escan->value.v_identifier);
      break;
    default:
      g_scanner_get_next_token ( context->escan );
      val = 0;
      // warning
  }
  
  return val;
}

gdouble expr_parse_num_l2 ( struct context *context )
{
  gdouble val;

  val = expr_parse_value ( context );
  while(strchr("*/",g_scanner_peek_next_token ( context->escan )))
  {
    g_scanner_get_next_token ( context->escan );
    if(context->escan->token == '*')
      val*=expr_parse_value( context );
    if(context->escan->token == '/')
      val/=expr_parse_value( context );
    if(g_scanner_eof(context->escan))
      break;
  }
  return val;
}

gdouble expr_parse_num_l1 ( struct context *context )
{
  gdouble val;

  val = expr_parse_num_l2 ( context );
  while(strchr("+-",g_scanner_peek_next_token( context->escan )))
  {
    g_scanner_get_next_token (context->escan );
    if(context->escan->token == '+')
      val+=expr_parse_num_l2( context );
    if(context->escan->token == '-')
      val-=expr_parse_num_l2( context );
    if(g_scanner_eof(context->escan))
      break;
  }
  return val;
}

gchar *expr_parse( struct context *context, gchar *expr )
{
  context->escan->input_name = expr;
  g_scanner_input_text(context->escan, expr, strlen(expr));

  if((g_scanner_peek_next_token(context->escan)==G_TOKEN_FLOAT)||
      (g_scanner_peek_next_token(context->escan)==(GTokenType)G_TOKEN_VAL)||
      (g_scanner_peek_next_token(context->escan)==(GTokenType)G_TOKEN_LEFT_PAREN)||
      ((g_scanner_peek_next_token(context->escan)==G_TOKEN_IDENTIFIER)&&
       (*(context->escan->next_value.v_identifier)!='$')))
    return numeric_to_str(expr_parse_num_l1(context),context->default_dec);
  else
    return expr_parse_str_l1(context);
}

void expr_parser_init ( struct context *context )
{
  context->escan = g_scanner_new(NULL);
  context->escan->config->scan_octal = 0;
  context->escan->config->symbol_2_token = 1;
  context->escan->config->case_sensitive = 0;
  context->escan->config->numbers_2_int = 1;
  context->escan->config->int_2_float = 1;

  context->escan->config->cset_identifier_nth = g_strconcat(".",
      context->escan->config->cset_identifier_nth,NULL);
  context->escan->config->cset_identifier_first = g_strconcat("$",
      context->escan->config->cset_identifier_first,NULL);

  g_scanner_scope_add_symbol(context->escan,0, "Mid", (gpointer)G_TOKEN_MIDW );
  g_scanner_scope_add_symbol(context->escan,0, "Extract", (gpointer)G_TOKEN_EXTRACT );
  g_scanner_scope_add_symbol(context->escan,0, "Str", (gpointer)G_TOKEN_STRW);
  g_scanner_scope_add_symbol(context->escan,0, "Val", (gpointer)G_TOKEN_VAL );
  g_scanner_scope_add_symbol(context->escan,0, "Time", (gpointer)G_TOKEN_TIME );
  g_scanner_scope_add_symbol(context->escan,0, "Df", (gpointer)G_TOKEN_DF );

  g_scanner_set_scope(context->escan,0);
}


/* convert a number to a string with specified number of decimals */
char *numeric_to_str ( double num, gint dec )
  {
  static gchar *format = "%%0.%df";
  static gchar fbuf[16];
  static gchar res[256];

  if((dec>99)||(dec<0))
    return NULL;

  snprintf(fbuf,15,format,dec);
  snprintf(res,255,fbuf,num);
  return g_strdup(res);
  }
