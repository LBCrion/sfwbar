/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "wintree.h"
#include "trigger.h"
#include "util/string.h"

static struct wintree_api *api;
static GList *wt_list;
static GList *appid_map;
static GList *appid_filter_list;
static GList *title_filter_list;
static GList *wintree_listeners;
static gpointer wt_focus;
static gboolean disown;

struct appid_mapper {
  GRegex *regex;
  gchar *app_id;
};

#define LISTENER_CALL(method, win) { \
  for(GList *li=wintree_listeners; li; li=li->next) \
    if(WINTREE_LISTENER(li->data)->method) \
      WINTREE_LISTENER(li->data)->method(win, \
          WINTREE_LISTENER(li->data)->data); \
}

#define api_call(x) if(api->x) api->x(id);
void wintree_minimize ( gpointer id ) { api_call(minimize) }
void wintree_unminimize ( gpointer id ) { api_call(unminimize) }
void wintree_maximize ( gpointer id ) { api_call(maximize) }
void wintree_unmaximize ( gpointer id ) { api_call(unmaximize) }
void wintree_focus ( gpointer id ) { api_call(focus) }
void wintree_close ( gpointer id ) { api_call(close) }

void wintree_api_register ( struct wintree_api *new )
{
  api = new;
}

gboolean wintree_api_check ( void )
{
  return !!api;
}

void wintree_listener_register ( window_listener_t *listener, void *data )
{
  window_listener_t *copy;
  GList *iter;

  if(!listener)
    return;

  copy = g_memdup(listener, sizeof(window_listener_t));
  copy->data = data;
  wintree_listeners = g_list_append(wintree_listeners, copy);

  if(copy->window_new)
  {
    for(iter=wt_list; iter; iter=g_list_next(iter))
      copy->window_new(iter->data, copy->data);
  }
}

void wintree_listener_remove ( void *data )
{
  GList *iter;

  for(iter=wintree_listeners; iter; iter=g_list_next(iter))
    if(WINTREE_LISTENER(iter->data)->data == data)
      break;
  if(iter)
    wintree_listeners = g_list_remove(wintree_listeners, iter->data);
}

void wintree_move_to ( gpointer id, gpointer wsid )
{
  if(api->move_to)
    api->move_to(id, wsid);
}

void wintree_set_disown ( gboolean new )
{
  disown = new;
}

gboolean wintree_get_disown ( void )
{
  return disown;
}

gchar *wintree_get_active ( void )
{
  window_t *win;

  win = wintree_from_id(wintree_get_focus());
  return win?win->title:"";
}

window_t *wintree_window_init ( void )
{
  window_t *w;

  w = g_malloc0(sizeof(window_t));
  w->pid=-1;
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
  s = g_strcmp0(a->title, b->title);
  if(s)
    return s;
  return GPOINTER_TO_INT(a->uid - b->uid);
}

void wintree_set_focus ( gpointer id )
{
  GList *iter;

  if(wt_focus == id)
    return;
  wintree_commit(wintree_from_id(wt_focus));
  wt_focus = id;
  for(iter=wt_list; iter; iter=g_list_next(iter) )
    if (((window_t *)(iter->data))->uid == id)
      break;
  if(!iter)
    return;
  if(g_list_previous(iter))
  {
    g_list_previous(iter)->next = NULL;
    iter->prev = NULL;
    wt_list = g_list_concat(iter, wt_list);
  }
  wintree_commit(wt_list->data);
  trigger_emit("window_focus");
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
  GList *iter;

  for(iter = wt_list; iter; iter=g_list_next(iter) )
    if ( ((window_t *)(iter->data))->uid == id )
      break;

  return iter?iter->data:NULL;
}

window_t *wintree_from_pid ( gint64 pid )
{
  GList *iter;

  for(iter = wt_list; iter; iter=g_list_next(iter) )
    if ( ((window_t *)(iter->data))->pid == pid )
      break;

  return iter?iter->data:NULL;
}

void wintree_commit ( window_t *win )
{
  if(!win)
    return;

  LISTENER_CALL(window_invalidate, win);
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
  wintree_commit(win);
}

void wintree_set_app_id ( gpointer wid, const gchar *app_id)
{
  window_t *win;

  if(!app_id || !( win=wintree_from_id(wid)) || !g_strcmp0(win->appid, app_id))
    return;
  LISTENER_CALL(window_destroy, win);
  g_free(win->appid);
  win->appid = g_strdup(app_id);
  if(!win->title)
    win->title = g_strdup(app_id);
  LISTENER_CALL(window_new, win);

  wintree_commit(win);
}

void wintree_set_workspace ( gpointer wid, gpointer wsid )
{
  window_t *win;
  workspace_t *ws;

  win = wintree_from_id(wid);
  ws = workspace_from_id(wsid);
  if(!win || !ws || win->workspace == ws)
    return;

  LISTENER_CALL(window_destroy, win);
  if(win->workspace)
    workspace_unref(win->workspace->id);
  win->workspace = ws;
  workspace_ref(wsid);
  LISTENER_CALL(window_new, win);
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

  if(win->title || win->appid)
    LISTENER_CALL(window_new, win);
  if(!g_list_find(wt_list, win))
    wt_list = g_list_append (wt_list, win);
  wintree_commit(win);
}

void wintree_window_delete ( gpointer id )
{
  GList *iter;
  window_t *win;

  for(iter=wt_list; iter; iter=g_list_next(iter) )
    if ( ((window_t *)(iter->data))->uid == id )
      break;
  if(!iter || !iter->data)
    return;
  win = iter->data;

  wt_list = g_list_delete_link(wt_list, iter);
  LISTENER_CALL(window_destroy, win);
  workspace_unref(win->workspace->id);
  g_free(win->appid);
  g_free(win->title);
  g_list_free_full(win->outputs,g_free);
  g_free(win);
}

GList *wintree_get_list ( void )
{
  return wt_list;
}

void wintree_appid_map_add ( gchar *pattern, gchar *appid )
{
  struct appid_mapper *map;
  GList *iter;

  if(!pattern || !appid)
    return;

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

gboolean wintree_placer_calc ( gpointer wid, GdkRectangle *place )
{
  window_t *win;
  GdkRectangle *obs, output;
  GList *iter;
  gint *x, *y;
  gint i, j, c, nobs, count, focus;

  if(!placer || !place || !wid)
    return FALSE;
  if( !(win = wintree_from_id(GINT_TO_POINTER(wid))) )
    return FALSE;
  count = 0;
  for(iter=wt_list; iter; iter=g_list_next(iter) )
    if ( ((window_t *)(iter->data))->pid == win->pid )
      count++;
  if(count>1)
    return FALSE;

  place->width = place->height = 0;
  nobs = workspace_get_geometry(wid, place, win->workspace->id, &obs, &output,
      &focus);

  x = g_malloc((nobs+1)*sizeof(int));
  y = g_malloc((nobs+1)*sizeof(int));
  for(c=0; c<nobs; c++)
  {
    x[c] = obs[c].x + obs[c].width;
    y[c] = obs[c].y + obs[c].height;
  }
  x[c]=output.x;
  y[c]=output.y;
  qsort(x, nobs+1, sizeof(int), comp_int);
  qsort(y, nobs+1, sizeof(int), comp_int);

  place->x = output.x + x_origin*output.width/100;
  place->y = output.y + y_origin*output.height/100;
  do
  {
    for(c=0; c<nobs; c++)
      if((place->x==obs[c].x) && (place->y==obs[c].y))
        break;
    if(c==nobs)
      break;
    place->x += output.width*x_step/100;
    place->y += output.height*y_step/100;;
  } while((place->x+place->width) < (output.x+output.width) &&
      (place->y+place->height) < (output.y+output.height));

  for(j=nobs; j>=0; j--)
    for(i=nobs; i>=0; i--)
    {
      for(c=0; c<nobs; c++)
        if(!(((x[i]+place->width-1) < obs[c].x) ||
              (x[i] > (obs[c].x+obs[c].width-1)) ||
              ((y[j]+place->height-1) < obs[c].y) ||
              (y[j] > (obs[c].y+obs[c].height-1))))
          break;

      if(c==nobs && !(x[i]<output.x || x[i]+place->width>output.x+output.width ||
          y[j]<output.y || y[j]+place->height>output.y+output.height))
      {
        place->x = x[i];
        place->y = y[j];
      }
    }
  g_free(x);
  g_free(y);
  g_free(obs);
  return TRUE;
}
