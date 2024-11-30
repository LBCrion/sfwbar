/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <sys/statvfs.h>
#include <locale.h>
#include "module.h"
#include "wintree.h"
#include "scanner.h"
#include "gui/basewidget.h"
#include "gui/taskbaritem.h"
#include "gui/bar.h"
#include "util/file.h"
#include "util/string.h"
#include "vm/expr.h"
#include "vm/vm.h"

/* extract a substring */
static value_t expr_lib_mid ( vm_t *vm, value_t stack[], gint np )
{

  gint len, c1, c2;

  if(np!=3 || !value_is_string(stack[0]) || !value_is_numeric(stack[1]) ||
        !value_is_numeric(stack[2]))
    return value_new_string(g_strdup(""));

  c1 = stack[1].value.numeric;
  c2 = stack[2].value.numeric;

  len = strlen(stack[0].value.string);
  if(c1<0)	/* negative offsets are relative to the end of the string */
    c1+=len;
  if(c2<0)
    c2+=len;
  c1 = CLAMP(c1, 0, len-1);
  c2 = CLAMP(c2, 0, len-1);
  if(c1>c2)
  {
    c2^=c1;	/* swap the ofsets */
    c1^=c2;
    c2^=c1;
  }

  return value_new_string(g_strndup(stack[0].value.string + c1,
        (c2-c1+1)*sizeof(gchar)));
}

/* replace a substring within a string */
static value_t expr_lib_replace( vm_t *vm, value_t stack[], gint np )
{
  if(np!=3 || value_is_string(stack[0]) || value_is_string(stack[1]) ||
      value_is_string(stack[2]))
    return value_na;

  return value_new_string(str_replace(stack[0].value.string,
      stack[1].value.string, stack[2].value.string));
}

static value_t expr_lib_replace_all( vm_t *vm, value_t stack[], gint np )
{
  value_t result;
  gchar *tmp;
  gint i;

  if(np<1 || !(np%2) || !value_is_string(stack[0]) || !stack[0].value.string)
    return value_na;

  result = value_new_string(g_strdup(stack[0].value.string));
  for(i=1; i<np; i+=2)
    if(stack[i].type==EXPR_TYPE_STRING && stack[i+1].type==EXPR_TYPE_STRING)
    {
      tmp = result.value.string;
      result.value.string = str_replace(tmp, stack[i].value.string,
          stack[i+1].value.string);
      g_free(tmp);
    }

  return result;
}

static value_t expr_lib_map( vm_t *vm, value_t stack[], gint np )
{
  gint i;

  if(np<2 || np%2 || !stack[0].value.string)
    return value_na;

  for(i=0; i<np; i++)
    if(value_is_string(stack[i]))
      return value_na;

  for(i=1; i<(np-1); i+=2)
    if(stack[i].value.string &&
        !g_strcmp0(stack[0].value.string, stack[i].value.string))
      return value_new_string(g_strdup(stack[i+1].value.string));
  return value_new_string(g_strdup(stack[np-1].value.string));
}

static value_t expr_lib_lookup( vm_t *vm, value_t p[], gint np )
{
  gchar *result = NULL;
  gint i;

  if(np<2 || np%2 || !value_is_numeric(p[0]))
    return value_new_string(g_strdup(""));

  for(i=(np-3); i>0; i-=2)
    if(value_is_numeric(p[i]) && value_is_string(p[i+1]) &&
        p[i].value.numeric < p[0].value.numeric)
      result = p[i+1].value.string;

  if(!result && value_is_string(p[np-1]))
    result = p[np-1].value.string;

  return value_new_string(g_strdup(result?result:""));
}

/* Extract substring using regex */
static value_t expr_lib_extract( vm_t *vm, value_t p[0], gint np )
{
  value_t res;
  GRegex *regex;
  GMatchInfo *match;

  if(np!=2 || !value_is_string(p[0]) || !value_is_string(p[1]))
    return value_na;

  if( !(regex = g_regex_new(p[1].value.string, 0, 0, NULL)) )
    return value_na;

  if(g_regex_match (regex, p[0].value.string, 0, &match) && match)
    res = value_new_string(g_match_info_fetch (match, 1));
  else
    res = value_na;

  if(match)
    g_match_info_free(match);
  if(regex)
    g_regex_unref(regex);

  return res;
}

static value_t expr_lib_pad ( vm_t *vm, value_t p[], gint np )
{
  gchar *result, *ptr;
  gint n, len, sign;
  gchar padchar;

  if(np<2 || np>3 || !value_is_string(p[0]) || !value_is_numeric(p[1]))
    return value_na;

  padchar = (np==3 && value_is_string(p[2]) && p[2].value.string)?
    *(p[2].value.string) : ' ';

  len = strlen(p[0].value.string);
  n = p[1].value.numeric;
  sign = n>=0;
  n = MAX(n>0?n:-n,len);

  result = g_malloc(n+1);
  if(sign)
  {
    memset(result, padchar, n-len);
    strcpy(result+n-len, p[0].value.string);
  }
  else
  {
    ptr = g_stpcpy(result, p[0].value.string);
    memset(ptr, padchar, n-len);
    *(result+n) = '\0';
  }

  return value_new_string(result);
}

/* Get current time string */
static value_t expr_lib_time ( vm_t *vm, value_t p[], gint np )
{
  GTimeZone *tz;
  GDateTime *time;
  gchar *str;

  if(np>2)
    return value_na;

  if(np<2 || !value_is_string(p[1]))
    time = g_date_time_new_now_local();
  else
  {
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    if( !(tz = g_time_zone_new_identifier(p[1].value.string)) )
      tz = g_time_zone_new_utc();
#else
    tz = g_time_zone_new(p[1].value_string);
#endif
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  str = g_date_time_format (time, (np>0 && value_is_string(p[0]))?
      p[0].value.string : "%a %b %d %H:%M:%S %Y" );
  g_date_time_unref(time);

  return value_new_string(str);
}

static value_t expr_lib_elapsed_str ( vm_t *vm, value_t p[], gint np )
{
  if(np!=1 || !value_is_numeric(p[0]))
    return value_na;

  if(p[0].value.numeric>3600*24)
    return value_new_string(g_strdup_printf("%d days ago",
          (gint)(p[0].value.numeric/(3600*24))));
  if(p[0].value.numeric>3600)
    return value_new_string(g_strdup_printf("%d hours ago",
        (gint)(p[0].value.numeric/3600)));
  if(p[0].value.numeric>60)
    return value_new_string(g_strdup_printf("%d minutes ago",
        (gint)(p[0].value.numeric/60)));
  return value_new_string(g_strdup("Just now"));
}

/* query current locale */
static value_t expr_lib_getlocale ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(g_strdup(setlocale(LC_ALL, NULL)));
}

/* generate disk space utilization for a device */
static value_t expr_lib_disk ( vm_t *vm, value_t p[], gint np )
{
  struct statvfs fs;

  if(np!=2 || !value_is_string(p[0]) || !value_is_string(p[1]))
    return value_na;

  if(statvfs(p[0].value.string, &fs))
    return value_na;

  if(!g_ascii_strcasecmp(p[1].value.string, "total"))
    return value_new_numeric(fs.f_blocks * fs.f_frsize);
  if(!g_ascii_strcasecmp(p[1].value.string, "avail"))
    return value_new_numeric(fs.f_bavail * fs.f_bsize);
  if(!g_ascii_strcasecmp(p[1].value.string, "free"))
    return value_new_numeric(fs.f_bfree * fs.f_bsize);
  if(!g_ascii_strcasecmp(p[1].value.string, "%avail"))
    return value_new_numeric(((gdouble)(fs.f_bfree*fs.f_bsize) /
        (gdouble)(fs.f_blocks*fs.f_frsize))*100);
  if(!g_ascii_strcasecmp(p[1].value.string, "%used"))
    return value_new_numeric((1.0 - (gdouble)(fs.f_bfree*fs.f_bsize) /
        (gdouble)(fs.f_blocks*fs.f_frsize))*100);

  return value_na;
}

static value_t expr_lib_active ( vm_t *vm, value_t p[], gint np )
{
  return value_new_string(g_strdup(wintree_get_active()));
}

static value_t expr_lib_max ( vm_t *vm, value_t p[], gint np )
{
  if(np==2 && value_is_numeric(p[0]) && value_is_numeric(p[1]))
    return value_new_numeric(MAX(p[0].value.numeric, p[1].value.numeric));

  return value_na;
}

static value_t expr_lib_min ( vm_t *vm, value_t p[], gint np )
{
  if(np==2 && value_is_numeric(p[0]) && value_is_numeric(p[1]))
    return value_new_numeric(MIN(p[0].value.numeric, p[1].value.numeric));

  return value_na;
}

static value_t expr_lib_val ( vm_t *vm, value_t p[], gint np )
{
  if(np==1 && value_is_string(p[0]))
    return value_new_numeric(strtod(p[0].value.string, NULL));

  return value_na;
}

static value_t expr_lib_str ( vm_t *vm, value_t p[], gint np )
{
  if(np>0 && value_is_numeric(p[0]))
    return value_new_string(numeric_to_string(p[0].value.numeric,
        (np==2 && value_is_numeric(p[1]))? p[1].value.numeric : 0));
  return value_na;
}

static value_t expr_lib_upper ( vm_t *vm, value_t p[], gint np )
{
  if(np==1 && value_is_string(p[0]))
    return value_new_string(g_ascii_strup(p[0].value.string, -1));
  return value_na;
}

static value_t expr_lib_lower ( vm_t *vm, value_t p[], gint np )
{
  if(np==1 && value_is_string(p[0]))
    return value_new_string(g_ascii_strdown(p[0].value.string, -1));
  return value_na;
}

static value_t expr_lib_escape ( vm_t *vm, value_t p[], gint np )
{
  if(np==1 && value_is_string(p[0]))
    return value_new_string(g_markup_escape_text(p[0].value.string, -1));
  return value_na;
}

static value_t expr_lib_bardir ( vm_t *vm, value_t p[], gint np )
{
  switch(bar_get_toplevel_dir(vm->cache->widget))
  {
    case GTK_POS_RIGHT:
      return value_new_string(g_strdup("right"));
    case GTK_POS_LEFT:
      return value_new_string(g_strdup("left"));
    case GTK_POS_TOP:
      return value_new_string(g_strdup("top"));
    case GTK_POS_BOTTOM:
      return value_new_string(g_strdup("bottom"));
    default:
      return value_new_string(g_strdup("unknown"));
  }
}

static value_t expr_lib_gtkevent ( vm_t *vm, value_t p[], gint np )
{
  GtkWidget *widget;
  GdkEventButton  *ev = (GdkEventButton *)vm->cache->event;
  GtkAllocation alloc;
  GtkStyleContext *style;
  GtkBorder margin, padding, border;
  gint x,y,w,h,dir;
  gdouble result;

  if(np!=1 || !value_is_string(p[0]))
    return value_na;

  if(GTK_IS_BIN(vm->cache->widget))
  {
    widget = gtk_bin_get_child(GTK_BIN(vm->cache->widget));
    gtk_widget_translate_coordinates(vm->cache->widget, widget,
        ev->x, ev->y, &x, &y);
  }
  else
  {
    widget = vm->cache->widget;
    x = ev->x;
    y = ev->y;
  }

  if(!g_ascii_strcasecmp(p[0].value.string, "x"))
    dir = GTK_POS_RIGHT;
  else if(!g_ascii_strcasecmp(p[0].value.string, "y"))
    dir = GTK_POS_BOTTOM;
  else if(!g_ascii_strcasecmp(p[0].value.string, "dir"))
    gtk_widget_style_get(widget, "direction", &dir, NULL);
  else
    return value_na;

  gtk_widget_get_allocation(widget, &alloc);
  style = gtk_widget_get_style_context(widget);
  gtk_style_context_get_margin(style, gtk_style_context_get_state(style),
      &margin);
  gtk_style_context_get_padding(style, gtk_style_context_get_state(style),
      &padding);
  gtk_style_context_get_border(style, gtk_style_context_get_state(style),
      &border);
  w = alloc.width - margin.left - margin.right - padding.left -
    padding.right - border.left - border.right;
  h = alloc.height - margin.top - margin.bottom - padding.top -
    padding.bottom - border.top - border.bottom;

  x = x - margin.left - padding.left - border.left;
  y = y - margin.top - padding.top - border.top;

  if(dir==GTK_POS_RIGHT || dir==GTK_POS_LEFT)
    result = CLAMP((gdouble)x / w, 0, 1);
  else
    result = CLAMP((gdouble)y / h, 0, 1);
  if(dir==GTK_POS_LEFT || dir==GTK_POS_TOP)
    result = 1.0 - result;

  return value_new_numeric(result);
}

static value_t expr_lib_widget_id ( vm_t *vm, value_t p[], gint np )
{
  gchar *id = base_widget_get_id(vm->cache->widget);

  return value_new_string(g_strdup(id?id:""));
}

static value_t expr_lib_window_info ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  if(np==1 && value_is_string(p[0]) &&
      (win = flow_item_get_source(vm->cache->widget)) )
  {
    if(!g_ascii_strcasecmp(p[0].value.string, "appid"))
      return value_new_string(g_strdup(win->appid));
    if(!g_ascii_strcasecmp(p[0].value.string, "title"))
      return value_new_string(g_strdup(win->title));
    if(!g_ascii_strcasecmp(p[0].value.string, "minimized"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_MINIMIZED)));
    if(!g_ascii_strcasecmp(p[0].value.string, "maximized"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_MAXIMIZED)));
    if(!g_ascii_strcasecmp(p[0].value.string, "fullscreen"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_FULLSCREEN)));
    if(!g_ascii_strcasecmp(p[0].value.string, "focused"))
      return value_new_string(g_strdup_printf("%d",
            wintree_is_focused(win->uid)));
  }

  return value_na;
}

static value_t expr_lib_read ( vm_t *vm, value_t p[], gint np )
{
  gchar *fname, *result;
  GIOChannel *in;

  if(np!=1 || !value_is_string(p[0]))
    return value_na;

  if( !(fname = get_xdg_config_file(p[0].value.string, NULL)) )
    return value_new_string(g_strdup_printf("Read: file not found '%s'",
          p[0].value.string));

  if( (in = g_io_channel_new_file(p[0].value.string, "r", NULL)) )
    g_io_channel_read_to_end(in, &result, NULL, NULL);
  else
    result = NULL;


  if(!result)
    result = g_strdup_printf("Read: can't open file '%s'", fname);

  g_free(fname);
  return value_new_string(result);
}

static value_t expr_iface_provider ( vm_t *vm, value_t p[], int np )
{
  if(np!=1 || !value_is_string(p[0]))
    return value_na;
  return value_new_string(module_interface_provider_get(p[0].value.string));
}

static value_t expr_ident ( vm_t *vm, value_t p[], int np )
{
  value_t result;

  if(np!=1 || !value_is_string(p[0]) || !p[0].value.string)
    return value_na;

  result = value_new_numeric(scanner_is_variable(p[0].value.string) ||
    module_is_function(p[0].value.string));
  if(!result.value.numeric)
    expr_dep_add(p[0].value.string, vm->cache);

  return result;
}

void expr_lib_init ( void )
{
  vm_func_init();
  vm_func_add("mid", expr_lib_mid, TRUE);
  vm_func_add("pad", expr_lib_pad, TRUE);
  vm_func_add("extract", expr_lib_extract, TRUE);
  vm_func_add("ident", expr_ident, TRUE);
  vm_func_add("replace", expr_lib_replace, TRUE);
  vm_func_add("replaceall", expr_lib_replace_all, TRUE);
  vm_func_add("map", expr_lib_map, TRUE);
  vm_func_add("lookup", expr_lib_lookup, TRUE);
  vm_func_add("time", expr_lib_time, FALSE);
  vm_func_add("elapsedstr", expr_lib_elapsed_str, TRUE);
  vm_func_add("getlocale", expr_lib_getlocale, FALSE);
  vm_func_add("disk", expr_lib_disk, FALSE);
  vm_func_add("activewin", expr_lib_active, FALSE);
  vm_func_add("max", expr_lib_max, TRUE);
  vm_func_add("min", expr_lib_min, TRUE);
  vm_func_add("val", expr_lib_val, TRUE);
  vm_func_add("str", expr_lib_str, TRUE);
  vm_func_add("upper", expr_lib_upper, TRUE);
  vm_func_add("lower", expr_lib_lower, TRUE);
  vm_func_add("escape", expr_lib_escape, TRUE);
  vm_func_add("bardir", expr_lib_bardir, FALSE);
  vm_func_add("gtkevent", expr_lib_gtkevent, FALSE);
  vm_func_add("widgetid", expr_lib_widget_id, FALSE);
  vm_func_add("windowinfo", expr_lib_window_info, FALSE);
  vm_func_add("read", expr_lib_read, FALSE);
  vm_func_add("interfaceprovider", expr_iface_provider, FALSE);
}
