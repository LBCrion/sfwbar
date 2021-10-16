/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- Lev Babiev
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


int yy_scan_string (char *);
int yyparse (struct context *);
int yy_delete_buffer (int );
int yylex_destroy ( void );

int yyerror(struct context *x,const gchar *y)
  {
  return 0;
  }

/* call yacc to evaluate expression */
char *parse_expr ( struct context *context, gchar *expr_str )
  {
  context->ret_val=NULL;
  yy_scan_string (expr_str);
  yyparse (context);
  yylex_destroy();
  return context->ret_val;
  }

/* extract a substring */
char *str_mid ( gchar *str, gint c1, gint c2 )
  {
  gchar *res;
  gint len;
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
  return res;
  }

/* generate disk space utilization for a device */
char *df_str ( gchar *fpath )
{
  gchar buf[256];
  struct statvfs fs;
  if(statvfs(fpath,&fs)!=0)
    return g_strdup("");
  snprintf(buf,255,"%ld %ld %ld %02Lf%%",fs.f_blocks*fs.f_bsize,
      fs.f_bavail*fs.f_bsize, (fs.f_blocks-fs.f_bavail)*fs.f_bsize,
      (1.0-(long double)fs.f_bavail/(long double)fs.f_blocks)*100);
  return g_strdup(buf);
}

/* Get current time string */
char *time_str ( gchar *tz )
  {
  time_t tp;
  gchar *str;
  gchar *oldtz;
  if(tz!=NULL)
  {
    oldtz = g_strdup(getenv("TZ"));
    setenv("TZ",tz,1);
    tzset();
  }
  if((str=g_try_malloc(26*sizeof(char)))==NULL)
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

  return str;
  }

/* Extract substring using pcre */
char *extract_str ( gchar *str, gchar *pattern )
  {
  gchar *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  if((str==NULL)||(pattern==NULL))
    return NULL;

  regex = g_regex_new(pattern,0,0,NULL);
  g_regex_match (regex, str, 0, &match);
  if(g_match_info_matches (match))
    sres = g_match_info_fetch (match, 0);
  g_match_info_free (match);
  g_regex_unref (regex);

  return sres;
  }

/* get node by name from the list root */
void *list_by_name ( GList *prev, gchar *name )
  {
  GList *node;
  for(node=prev;node!=NULL;node=g_list_next(node))
      if(!g_strcmp0(SCAN_VAR(node->data)->name,name))
        return node->data;
  return NULL;
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
