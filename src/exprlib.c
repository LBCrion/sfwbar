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
static value_t expr_lib_mid ( vm_t *vm, value_t p[], gint np )
{

  gint len, c1, c2;

  vm_param_check_np(vm, np, 3, "mid");
  vm_param_check_string(vm, p, 0, "mid");
  vm_param_check_numeric(vm, p, 1, "mid");
  vm_param_check_numeric(vm, p, 2, "mid");

  c1 = value_get_numeric(p[1]);
  c2 = value_get_numeric(p[2]);
  len = strlen(value_get_string(p[0]));

  /* negative offsets are relative to the end of the string */
  c1 = CLAMP(c1<0? c1+len+1 : c1, 1, len);
  c2 = CLAMP(c2<0? c2+len+1 : c2, 1, len);

  return value_new_string(g_strndup(value_get_string(p[0]) + MIN(c1, c2)-1,
        (ABS(c2-c1)+1)*sizeof(gchar)));
}

/* replace a substring within a string */
static value_t expr_lib_replace( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 3, "replace");
  vm_param_check_string(vm, p, 0, "replace");
  vm_param_check_string(vm, p, 1, "replace");
  vm_param_check_string(vm, p, 2, "replace");

  return value_new_string(str_replace(value_get_string(p[0]), value_get_string(p[1]),
        value_get_string(p[2])));
}

static value_t expr_lib_replace_all( vm_t *vm, value_t p[], gint np )
{
  value_t result;
  gchar *tmp;
  gint i;

  if(np<1 || !(np%2) || !value_like_string(p[0]) || !value_get_string(p[0]))
    return value_na;

  result = value_new_string(g_strdup(value_get_string(p[0])));
  for(i=1; i<np; i+=2)
    if(value_like_string(p[i]) && value_like_string(p[i+1]))
    {
      tmp = result.value.string;
      result.value.string = str_replace(tmp, p[i].value.string,
          p[i+1].value.string);
      g_free(tmp);
    }

  return result;
}

static value_t expr_lib_map( vm_t *vm, value_t p[], gint np )
{
  gint i;

  if(np<2 || np%2)
    return value_na;

  for(i=0; i<np; i++)
    if(!value_like_string(p[i]))
      return value_na;

  for(i=1; i<(np-1); i+=2)
    if(value_get_string(p[i]) &&
        !g_strcmp0(value_get_string(p[0]), value_get_string(p[i])))
      return value_new_string(g_strdup(value_get_string(p[i+1])));
  return value_new_string(g_strdup(value_get_string(p[np-1])));
}

static value_t expr_lib_lookup( vm_t *vm, value_t p[], gint np )
{
  gchar *result = NULL;
  gint i;

  if(np<2 || np%2 || !value_like_numeric(p[0]))
    return value_new_string(g_strdup(""));

  for(i=(np-3); i>0; i-=2)
    if(value_like_numeric(p[i]) && value_like_string(p[i+1]) &&
        value_get_numeric(p[i]) < value_get_numeric(p[0]))
      result = value_get_string(p[i+1]);

  if(!result && value_like_string(p[np-1]))
    result = value_get_string(p[np-1]);

  return value_new_string(g_strdup(result?result:""));
}

/* Extract substring using regex */
static value_t expr_lib_extract( vm_t *vm, value_t p[], gint np )
{
  value_t res;
  GRegex *regex;
  GMatchInfo *match;

  vm_param_check_np(vm, np, 2, "extract");
  vm_param_check_string(vm, p, 0, "extract");
  vm_param_check_string(vm, p, 1, "extract");

  if( !(regex = g_regex_new(value_get_string(p[1]), 0, 0, NULL)) )
    return value_na;

  if(g_regex_match (regex, value_get_string(p[0]), 0, &match) && match)
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

  vm_param_check_np_range(vm, np, 2, 3, "pad");
  vm_param_check_string(vm, p, 0, "pad");
  vm_param_check_numeric(vm, p, 1, "pad");
  if(np==3)
    vm_param_check_string(vm, p, 2, "pad");

  padchar = (np==3)?  *(value_get_string(p[2])) : ' ';

  len = strlen(value_get_string(p[0]));
  n = value_get_numeric(p[1]);
  sign = n>=0;
  n = MAX(n>0?n:-n,len);

  result = g_malloc(n+1);
  if(sign)
  {
    memset(result, padchar, n-len);
    strcpy(result+n-len, value_get_string(p[0]));
  }
  else
  {
    ptr = g_stpcpy(result, value_get_string(p[0]));
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

  vm_param_check_np_range(vm, np, 0, 2, "time");
  if(np>0)
    vm_param_check_string(vm, p, 0, "time");
  if(np==2)
    vm_param_check_string(vm, p, 1, "time");
  if(np<2)
    time = g_date_time_new_now_local();
  else
  {
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 68
    if( !(tz = g_time_zone_new_identifier(value_get_string(p[1]))) )
      tz = g_time_zone_new_utc();
#else
    tz = g_time_zone_new(value_get_string(p[1]));
#endif
    time = g_date_time_new_now(tz);
    g_time_zone_unref(tz);
  }

  str = g_date_time_format (time, (np>0)? value_get_string(p[0]) :
      "%a %b %d %H:%M:%S %Y" );
  g_date_time_unref(time);

  return value_new_string(str);
}

static value_t expr_lib_elapsed_str ( vm_t *vm, value_t p[], gint np )
{
  if(np!=1 || !value_like_numeric(p[0]))
    return value_na;

  if(value_get_numeric(p[0])>3600*24)
    return value_new_string(g_strdup_printf("%d days ago",
          (gint)(value_get_numeric(p[0])/(3600*24))));
  if(value_get_numeric(p[0])>3600)
    return value_new_string(g_strdup_printf("%d hours ago",
        (gint)(value_get_numeric(p[0])/3600)));
  if(value_get_numeric(p[0])>60)
    return value_new_string(g_strdup_printf("%d minutes ago",
        (gint)(value_get_numeric(p[0])/60)));
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

  vm_param_check_np(vm, np, 2, "disk");
  vm_param_check_string(vm, p, 0, "disk");
  vm_param_check_string(vm, p, 1, "disk");

  if(statvfs(value_get_string(p[0]), &fs))
    return value_na;

  if(!g_ascii_strcasecmp(value_get_string(p[1]), "total"))
    return value_new_numeric(fs.f_blocks * fs.f_frsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "avail"))
    return value_new_numeric(fs.f_bavail * fs.f_bsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "free"))
    return value_new_numeric(fs.f_bfree * fs.f_bsize);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "%avail"))
    return value_new_numeric(((gdouble)(fs.f_bfree*fs.f_bsize) /
        (gdouble)(fs.f_blocks*fs.f_frsize))*100);
  if(!g_ascii_strcasecmp(value_get_string(p[1]), "%used"))
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
  vm_param_check_np(vm, np, 2, "max");
  vm_param_check_numeric(vm, p, 0, "max");
  vm_param_check_numeric(vm, p, 1, "max");

  return value_new_numeric(MAX(value_get_numeric(p[0]), value_get_numeric(p[1])));
}

static value_t expr_lib_min ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "min");
  vm_param_check_numeric(vm, p, 0, "min");
  vm_param_check_numeric(vm, p, 1, "min");

  return value_new_numeric(MIN(value_get_numeric(p[0]), value_get_numeric(p[1])));
}

static value_t expr_lib_val ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "val");
  vm_param_check_string(vm, p, 0, "val");

  return value_new_numeric(strtod(value_get_string(p[0]), NULL));
}

static value_t expr_lib_str ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 1, 2, "str");
  vm_param_check_numeric(vm, p, 0, "str");
  if(np==2)
    vm_param_check_numeric(vm, p, 1, "str");

  return value_new_string(numeric_to_string(value_get_numeric(p[0]),
      (np==2)? value_get_numeric(p[1]) : 0));
}

static value_t expr_lib_upper ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "upper");
  vm_param_check_string(vm, p, 0, "upper");

  return value_new_string(g_ascii_strup(value_get_string(p[0]), -1));
}

static value_t expr_lib_lower ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "lower");
  vm_param_check_string(vm, p, 0, "lower");

  return value_new_string(g_ascii_strdown(value_get_string(p[0]), -1));
}

static value_t expr_lib_escape ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "escape");
  vm_param_check_string(vm, p, 0, "escape");

  return value_new_string(g_markup_escape_text(value_get_string(p[0]), -1));
}

static value_t expr_lib_bardir ( vm_t *vm, value_t p[], gint np )
{
  switch(bar_get_toplevel_dir(vm->widget))
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
  GdkEventButton  *ev = (GdkEventButton *)vm->event;
  GtkAllocation alloc;
  GtkStyleContext *style;
  GtkBorder margin, padding, border;
  gint x,y,w,h,dir;
  gdouble result;

  vm_param_check_np(vm, np, 1, "GtkEvent");
  vm_param_check_string(vm, p, 0, "GtkEvent");

  if(GTK_IS_BIN(vm->widget))
  {
    widget = gtk_bin_get_child(GTK_BIN(vm->widget));
    gtk_widget_translate_coordinates(vm->widget, widget,
        ev->x, ev->y, &x, &y);
  }
  else
  {
    widget = vm->widget;
    x = ev->x;
    y = ev->y;
  }

  if(!g_ascii_strcasecmp(value_get_string(p[0]), "x"))
    dir = GTK_POS_RIGHT;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "y"))
    dir = GTK_POS_BOTTOM;
  else if(!g_ascii_strcasecmp(value_get_string(p[0]), "dir"))
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
  gchar *id = base_widget_get_id(vm->widget);

  return value_new_string(g_strdup(id?id:""));
}

static value_t expr_lib_window_info ( vm_t *vm, value_t p[], gint np )
{
  window_t *win;

  vm_param_check_np(vm, np, 1, "WindowInfo");
  vm_param_check_string(vm, p, 0, "WindowInfo");

  if( (win = flow_item_get_source(vm->widget)) )
  {
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "appid"))
      return value_new_string(g_strdup(win->appid));
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "title"))
      return value_new_string(g_strdup(win->title));
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "minimized"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_MINIMIZED)));
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "maximized"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_MAXIMIZED)));
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "fullscreen"))
      return value_new_string(g_strdup_printf("%d",
            !!(win->state & WS_FULLSCREEN)));
    if(!g_ascii_strcasecmp(value_get_string(p[0]), "focused"))
      return value_new_string(g_strdup_printf("%d",
            wintree_is_focused(win->uid)));
  }

  return value_na;
}

static value_t expr_lib_read ( vm_t *vm, value_t p[], gint np )
{
  gchar *fname, *result;
  GIOChannel *in;

  vm_param_check_np(vm, np, 1, "read");
  vm_param_check_string(vm, p, 0, "read");

  if( !(fname = get_xdg_config_file(value_get_string(p[0]), NULL)) )
    return value_new_string(g_strdup_printf("Read: file not found '%s'",
          value_get_string(p[0])));

  if( (in = g_io_channel_new_file(value_get_string(p[0]), "r", NULL)) )
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
  vm_param_check_np(vm, np, 1, "InterfaceProvider");
  vm_param_check_string(vm, p, 0, "InterfaceProvider");

  return value_new_string(module_interface_provider_get(value_get_string(p[0])));
}

static value_t expr_ident ( vm_t *vm, value_t p[], int np )
{
  value_t result;
  vm_function_t *func;

  vm_param_check_np(vm, np, 1, "Ident");
  vm_param_check_string(vm, p, 0, "Ident");
  if(!value_get_string(p[0]))
    return value_na;

  func = vm_func_lookup(value_get_string(p[0]));
  if(func->ptr.function)
    result = value_new_numeric(TRUE);
  else
    result = value_new_numeric(scanner_is_variable(value_get_string(p[0])));

  if(!result.value.numeric)
    expr_dep_add(value_get_string(p[0]), vm->expr);

  return result;
}

static value_t expr_gettext ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np_range(vm, np, 1, 2, "GT");
  vm_param_check_string(vm, p, 0, "GT");
  if(np==2)
    vm_param_check_string(vm, p, 1, "GT");

  return value_new_string(g_strdup(g_dgettext(
        np==2? value_get_string(p[1]) : "sfwbar", value_get_string(p[0]))));
}

static value_t expr_array_build ( vm_t *vm, value_t p[], gint np )
{
  GArray *array;
  value_t v1;
  gint i;

  array = g_array_new(FALSE, FALSE, sizeof(value_t));
  g_array_set_clear_func(array, (GDestroyNotify)value_free);

  for(i=0; i<np; i++)
  {
    v1 = value_dup(p[i]);
    g_array_append_val(array, v1);
  }

  return value_new_array(array);
}

static value_t expr_array_index ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayIndex");
  vm_param_check_array(vm, p, 0, "ArrayIndex");
  vm_param_check_numeric(vm, p, 1, "ArrayIndex");

  if(!value_is_array(p[0]) || (gint)value_get_numeric(p[1])<0 ||
      p[0].value.array->len <= ((gint)value_get_numeric(p[1])))
  return value_na;

  return g_array_index(p[0].value.array, value_t,
      (gint)value_get_numeric(p[1]));
}

static value_t expr_array_assign ( vm_t *vm, value_t p[], gint np )
{
  value_t *v1;
  GArray *arr;
  gint n;

  vm_param_check_np(vm, np, 3, "ArrayAssign");
  vm_param_check_array(vm, p, 0, "ArrayAssign");
  vm_param_check_numeric(vm, p, 1, "ArrayAssign");

  if(!value_is_array(p[0]))
    return value_na;

  arr = g_array_ref(p[0].value.array);
  n = (gint)value_get_numeric(p[1]);
  if(n<0 || n>=arr->len)
    g_array_set_size(arr, n+1);

  v1 = &g_array_index(arr, value_t, n);
  value_free(*v1);
  *v1 = value_dup(p[2]);

  return value_new_array(arr);
}

static value_t expr_array_concat ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 2, "ArrayConcat");

  return value_array_concat(p[0], p[1]);
}

static value_t expr_array_size ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "ArraySize");

  return value_new_numeric(value_is_array(p[0])? p[0].value.array->len : 0);
}

static value_t expr_test_file ( vm_t *vm, value_t p[], gint np )
{
  vm_param_check_np(vm, np, 1, "TestFile");
  vm_param_check_string(vm, p, 0, "TestFile");

  return value_new_numeric(file_test_read(value_get_string(p[0])));
}

static value_t expr_ls ( vm_t *vm, value_t p[], gint np )
{
  GArray *array;
  GDir *dir;
  value_t v1;
  const gchar *file;

  vm_param_check_np(vm, np, 1, "ls");
  vm_param_check_string(vm, p, 0, "ls");

  array = g_array_new(FALSE, FALSE, sizeof(value_t));
  g_array_set_clear_func(array, (GDestroyNotify)value_free);
  if( (dir = g_dir_open(value_get_string(p[0]), 0, NULL)) )
  {
    while( (file = g_dir_read_name(dir)) )
    {
      v1 = value_new_string(g_strdup(file));
      g_array_append_val(array, v1);
    }
    g_dir_close(dir);
  }

  return value_new_array(array);
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
  vm_func_add("gt", expr_gettext, FALSE);
  vm_func_add("arraybuild", expr_array_build, FALSE);
  vm_func_add("arrayindex", expr_array_index, FALSE);
  vm_func_add("arrayassign", expr_array_assign, FALSE);
  vm_func_add("arrayconcat", expr_array_concat, FALSE);
  vm_func_add("arraysize", expr_array_size, FALSE);
  vm_func_add("testfile", expr_test_file, FALSE);
  vm_func_add("ls", expr_ls, FALSE);
}
