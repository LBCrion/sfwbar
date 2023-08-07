/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <gtk/gtk.h>
#include "sfwbar.h"
#include "wintree.h"
#include "taskbar.h"
#include "switcher.h"

static struct wintree_api api;
static GList *wt_list;
static GList *appid_map;
static GList *appid_filter_list;
static GList *title_filter_list;
static gpointer wt_focus;
static gchar *wt_active;
static guint64 seq;
static gboolean disown;

struct appid_mapper{
  GRegex *regex;
  gchar *app_id;
};

#define api_call(x) if(api.x) api.x(id);
void wintree_minimize ( gpointer id ) { api_call(minimize) }
void wintree_unminimize ( gpointer id ) { api_call(unminimize) }
void wintree_maximize ( gpointer id ) { api_call(maximize) }
void wintree_unmaximize ( gpointer id ) { api_call(unmaximize) }
void wintree_focus ( gpointer id ) { api_call(focus) }
void wintree_close ( gpointer id ) { api_call(close) }
void wintree_free_workspace ( gpointer id ) { api_call(free_workspace) }

void wintree_api_register ( struct wintree_api *new )
{
  api = *new;
}

void wintree_set_disown ( gboolean new )
{
  disown = new;
}

gboolean wintree_get_disown ( void )
{
  return disown;
}

void wintree_set_active ( gchar *title )
{
  g_free(wt_active);
  wt_active = g_strdup(title);
}

gchar *wintree_get_active ( void )
{
  return wintree_from_id(wintree_get_focus())->title;
}

window_t *wintree_window_init ( void )
{
  window_t *w;
  w = g_malloc0(sizeof(window_t));
  w->pid=-1;
  w->seq = seq++;
  return w;
}

void wintree_log ( gpointer id )
{
  window_t *win;

  win = wintree_from_id(id);
  if(win)
    g_debug("app_id: '%s', title '%s'",
        win->appid?win->appid:"(null)",win->title?win->title:"(null)");
}

gint wintree_compare ( window_t *a, window_t *b)
{
  gint s;
  s = g_strcmp0(a->title,b->title);
  if(s)
    return s;
  return GPOINTER_TO_INT(a->uid)-GPOINTER_TO_INT(b->uid);
}

void wintree_set_focus ( gpointer id )
{
  window_t *win;

  wintree_commit(wintree_from_id(wt_focus));
  wt_focus = id;
  win = wintree_from_id(id);
  if(!win)
    return;
  wintree_commit(win);
  wintree_set_active(win->title);
}

gpointer wintree_get_focus ( void )
{
  return wt_focus;
}

gboolean wintree_is_focused ( gpointer id )
{
  return ( id == wt_focus );
}

window_t *wintree_from_id ( gpointer id )
{
  GList *item;
  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((window_t *)(item->data))->uid == id )
      break;
  if(!item)
    return NULL;
  return item->data;
}

window_t *wintree_from_pid ( gint64 pid )
{
  GList *item;
  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((window_t *)(item->data))->pid == pid )
      break;
  if(!item)
    return NULL;
  return item->data;
}

void wintree_commit ( window_t *win )
{
  if(!win)
    return;

  taskbar_invalidate_all(win,FALSE);
  switcher_invalidate(win);
}

void wintree_set_title ( gpointer wid, const gchar *title )
{
  window_t *win;

  if(!title)
    return;

  win = wintree_from_id(wid);
  if(!win)
    return;
  if(!g_strcmp0(win->title,title))
    return;

  g_free(win->title);
  win->title = g_strdup(title);
  wintree_set_active(win->title);
  wintree_commit(win);
}

void wintree_set_app_id ( gpointer wid, const gchar *app_id)
{
  window_t *win;

  if(!app_id)
    return;

  win = wintree_from_id(wid);
  if(!win)
    return;
  taskbar_reparent_item(win,app_id);
  g_free(win->appid);
  win->appid = g_strdup(app_id);
  if(!win->title)
    win->title = g_strdup(app_id);
  wintree_commit(win);
}

void wintree_set_float ( gpointer wid, gboolean floating )
{
  window_t *win;
  win = wintree_from_id(wid);
  if(!win)
    return;
  win->floating = floating;
  wintree_commit(win);
}

void wintree_window_append ( window_t *win )
{
  if(!win)
    return;

  if( !win->title )
    win->title = g_strdup("");
  if(! win->appid)
    win->appid = g_strdup("");
  if( !win->valid )
  {
    taskbar_init_item  (win);
    win->valid = TRUE;
  }
  if( !win->switcher && (win->title || win->appid) )
    switcher_window_init(win);
  if(g_list_find(wt_list,win)==NULL)
    wt_list = g_list_append (wt_list,win);
  wintree_commit(win);
}

void wintree_window_delete ( gpointer id )
{
  GList *item;
  window_t *win;

  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((window_t *)(item->data))->uid == id )
      break;
  if(!item)
    return;
  win = item->data;
  if(!win)
    return;

  taskbar_destroy_item (win);
  switcher_window_delete(win);
  g_free(win->appid);
  g_free(win->title);
  g_list_free_full(win->outputs,g_free);
  wintree_free_workspace(win->workspace);
  wt_list = g_list_delete_link(wt_list,item);
  g_free(win);
}

GList *wintree_get_list ( void )
{
  wt_list = g_list_sort(wt_list, (GCompareFunc)wintree_compare);
  return wt_list;
}

void wintree_appid_map_add ( gchar *pattern, gchar *appid )
{
  struct appid_mapper *map;
  GList *iter;

  for(iter=appid_map;iter;iter=g_list_next(iter))
    if(!g_strcmp0(pattern,
          g_regex_get_pattern(((struct appid_mapper *)iter->data)->regex)))
      return;
  map = g_malloc0(sizeof(struct appid_mapper));
  map->regex = g_regex_new(pattern,0,0,NULL);
  if(!map->regex)
  {
    g_message("MapAppId: invalid paatern '%s'",pattern);
    g_free(map);
    return;
  }
  map->app_id = g_strdup(appid);
  appid_map = g_list_prepend(appid_map,map);
}

gchar *wintree_appid_map_lookup ( gchar *title )
{
  GList *iter;

  for(iter=appid_map;iter;iter=g_list_next(iter))
    if(g_regex_match (((struct appid_mapper *)iter->data)->regex,
          title, 0, NULL))
      return ((struct appid_mapper *)iter->data)->app_id;
  return NULL;
}

void wintree_filter_appid ( gchar *pattern )
{
  GList *iter;
  GRegex *regex;

  for(iter=appid_filter_list;iter;iter=g_list_next(iter))
    if(!g_strcmp0(pattern,
          g_regex_get_pattern(((struct appid_mapper *)iter->data)->regex)))
      return;

  regex = g_regex_new(pattern,0,0,NULL);
  if(!regex)
    return;

  appid_filter_list = g_list_prepend(appid_filter_list, regex);
}

void wintree_filter_title ( gchar *pattern )
{
  GList *iter;
  GRegex *regex;

  for(iter=title_filter_list;iter;iter=g_list_next(iter))
    if(!g_strcmp0(pattern,
          g_regex_get_pattern(((struct appid_mapper *)iter->data)->regex)))
      return;

  regex = g_regex_new(pattern,0,0,NULL);
  if(!regex)
    return;

  title_filter_list = g_list_prepend(title_filter_list, regex);
}

gboolean wintree_is_filtered ( window_t *win )
{
  return (regex_match_list(appid_filter_list, win->appid) ||
    regex_match_list(title_filter_list, win->title));
}

static gint x_step, y_step, x_origin, y_origin;
static gboolean check_pid;
static gboolean placer;

void wintree_placer_conf( gint xs, gint ys, gint xo, gint yo, gboolean pid )
{
  x_step = MAX(1,xs);
  y_step = MAX(1,ys);
  x_origin = xo;
  y_origin = yo;
  check_pid = !pid;
  placer = TRUE;
}

gboolean wintree_placer_state ( void )
{
  return placer;
}

gboolean wintree_placer_check ( gint pid )
{
  GList *iter;
  gint count = 0;

  if(!placer)
    return FALSE;
  for(iter = wt_list; iter; iter = g_list_next(iter) )
    if ( ((window_t *)(iter->data))->pid == pid )
      count++;

  return (count<2);
}

static int comp_int ( const void *x1, const void *x2)
{
  return ( *(int*)x1 - *(int*)x2 );
}

void wintree_placer_calc ( gint nobs, GdkRectangle *obs, GdkRectangle output,
    GdkRectangle *win )
{
  gint xoff,yoff;
  gint *x,*y;
  gint i,j,c;
  gboolean success;

  xoff = (win->x*2+win->width)/2 - (output.x*2+output.width)/2;
  yoff = (win->y*2+win->height)/2 - (output.y*2+output.height)/2;
  if((xoff<-1)||(xoff>1)||(yoff<-1)||(yoff>1))
    return;

  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  for(c=0;c<nobs;c++)
  {
    x[c] = obs[c].x + obs[c].width;
    y[c] = obs[c].y + obs[c].height;
  }
  x[c]=output.x;
  y[c]=output.y;
  qsort(x,nobs+1,sizeof(int),comp_int);
  qsort(y,nobs+1,sizeof(int),comp_int);

  win->x = output.x+x_origin*output.width/100;
  win->y = output.y+y_origin*output.height/100;
  do
  {
    success=TRUE;
    for(c=0;c<nobs;c++)
      if((win->x==obs[c].x) && (win->y==obs[c].y))
        success=FALSE;
    if (success)
      break;
    win->x += output.width*x_step/100;
    win->y += output.height*y_step/100;;
  } while((win->x+win->width)<(output.x+output.width) &&
      (win->y+win->height)<(output.y+output.height));

  for(j=nobs;j>=0;j--)
    for(i=nobs;i>=0;i--)
    {
      success=TRUE;
      for(c=0;c<nobs;c++)
        if(!(((x[i]+win->width-1)<obs[c].x) ||
              (x[i]>(obs[c].x+obs[c].width-1)) ||
              ((y[j]+win->height-1)<obs[c].y) ||
              (y[j]>(obs[c].y+obs[c].height-1))))
          success=FALSE;
      if((x[i]<output.x)||(x[i]+win->width>output.x+output.width)||
          (y[j]<output.y)||(y[j]+win->height>output.y+output.height))
        success=FALSE;
      if(success)
      {
        win->x = x[i];
        win->y = y[j];
      }
    }
  g_free(x);
  g_free(y);
}
