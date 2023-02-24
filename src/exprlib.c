/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <sys/statvfs.h>
#include <glib.h>
#include "sfwbar.h"
#include "module.h"
#include "wintree.h"
#include "expr.h"

/* extract a substring */
static void *expr_lib_mid ( void **params )
{

  gint len, c1, c2;

  if(!params || !params[0] || !params[1] || !params[2])
    return g_strdup("");

  c1 = *((gdouble *)params[1]);
  c2 = *((gdouble *)params[2]);

  len = strlen(params[0]);
  if(c1<0)	/* negative offsets are relative to the end of the string */
    c1+=len;
  if(c2<0)
    c2+=len;
  c1 = CLAMP(c1,0,len-1);
  c2 = CLAMP(c2,0,len-1);
  if(c1>c2)
  {
    c2^=c1;	/* swap the ofsets */
    c1^=c2;
    c2^=c1;
  }

  return g_strndup( params[0]+c1*sizeof(gchar), (c2-c1+1)*sizeof(gchar));
}

/* Extract substring using regex */
static void *expr_lib_extract( void **params )
{
  gchar *sres=NULL;
  GRegex *regex;
  GMatchInfo *match;

  if(!params || !params[0] || !params[1])
    return g_strdup("");

  regex = g_regex_new(params[1],0,0,NULL);
  g_regex_match (regex, params[0], 0, &match);
  if(g_match_info_matches (match))
    sres = g_match_info_fetch (match, 0);
  g_match_info_free (match);
  g_regex_unref (regex);

  return sres?sres:g_strdup("");
}

static void *expr_lib_pad ( void **params )
{
  if(!params || !params[0] || !params[1])
    return g_strdup("");

  return g_strdup_printf("%*s",(gint)*((gdouble *)params[1]),
      (gchar *)params[0]);
}

/* Get current time string */
static void *expr_lib_time ( void **params )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str;

  if(!params)
    return g_strdup("");

  if(!params[1])
    time = g_date_time_new_now_local();
  else
  {
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    tz = g_time_zone_new_identifier(params[1]);
#else
    tz = g_time_zone_new(params[1]);
#endif
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  str = g_date_time_format (time, params[0]?params[0]:"%a %b %d %H:%M:%S %Y" );
  g_date_time_unref(time);

  return str;
}

/* generate disk space utilization for a device */
static void *expr_lib_disk ( void **params )
{
  struct statvfs fs;
  gdouble *result = g_malloc0(sizeof(gdouble));

  if(!params || !params[0] || !params[1])
    return result;

  if(statvfs(params[0],&fs))
    return result;

  if(!g_ascii_strcasecmp(params[1],"total"))
    *result = fs.f_blocks * fs.f_frsize;
  if(!g_ascii_strcasecmp(params[1],"avail"))
    *result = fs.f_bavail * fs.f_bsize;
  if(!g_ascii_strcasecmp(params[1],"free"))
    *result = fs.f_bfree * fs.f_bsize;
  if(!g_ascii_strcasecmp(params[1],"%avail"))
    *result = ((gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;
  if(!g_ascii_strcasecmp(params[1],"%used"))
    *result = (1.0 - (gdouble)(fs.f_bfree*fs.f_bsize) / (gdouble)(fs.f_blocks*fs.f_frsize))*100;

  return result;
}

static void *expr_lib_active ( void **params )
{
  return g_strdup(wintree_get_active());
}

static void *expr_lib_str ( void **params )
{
  if(!params || !params[0])
    return g_strdup("");
  else
    return expr_dtostr(*(gdouble *)params[0],
        params[1]?(gint)*(gdouble *)params[1]:0);
}

static void *expr_lib_val ( void **params )
{
  gdouble *result;

  if(!params || !params[0])
    return g_malloc0(sizeof(gdouble));

  result = g_malloc(sizeof(gdouble));
  *result = strtod(params[0],NULL);

  return result;
}
ModuleExpressionHandlerV1 mid_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "mid",
  .parameters = "SNN",
  .function = expr_lib_mid
};

ModuleExpressionHandlerV1 pad_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "pad",
  .parameters = "SN",
  .function = expr_lib_pad
};

ModuleExpressionHandlerV1 extract_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "extract",
  .parameters = "SS",
  .function = expr_lib_extract
};

ModuleExpressionHandlerV1 time_handler = {
  .flags = 0,
  .name = "time",
  .parameters = "ss",
  .function = expr_lib_time
};

ModuleExpressionHandlerV1 disk_handler = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "disk",
  .parameters = "SS",
  .function = expr_lib_disk
};

ModuleExpressionHandlerV1 active_handler = {
  .flags = 0,
  .name = "ActiveWin",
  .parameters = "",
  .function = expr_lib_active
};

ModuleExpressionHandlerV1 str_handler = {
  .flags = MODULE_EXPR_DETERMINISTIC,
  .name = "str",
  .parameters = "Nn",
  .function = expr_lib_str
};

ModuleExpressionHandlerV1 val_handler = {
  .flags = MODULE_EXPR_NUMERIC | MODULE_EXPR_DETERMINISTIC,
  .name = "val",
  .parameters = "S",
  .function = expr_lib_val
};

ModuleExpressionHandlerV1 *expr_lib_handlers[] = {
  &mid_handler,
  &pad_handler,
  &extract_handler,
  &time_handler,
  &disk_handler,
  &active_handler,
  &str_handler,
  &val_handler,
  NULL
};

void expr_lib_init ( void )
{
  module_expr_funcs_add(expr_lib_handlers,"expression library");
}
