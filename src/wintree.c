/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"
#include "wintree.h"
#include "taskbar.h"
#include "switcher.h"
#include "wayland.h"
#include "sway_ipc.h"
#include "wlr-foreign-toplevel-management-unstable-v1.h"

static GList *wt_list;
static GList *appid_map;
static gpointer wt_focus;
static gchar *wt_active;

struct appid_mapper{
  GRegex *regex;
  gchar *app_id;
};

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
  return w;
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
  taskbar_invalidate_all(win);
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
  g_free(win->appid);
  win->appid = g_strdup(app_id);
  if(!win->title)
    win->title = g_strdup(app_id);
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
  g_free(win->output);
  wt_list = g_list_delete_link(wt_list,item);
  g_free(win);
}

GList *wintree_get_list ( void )
{
  wt_list = g_list_sort(wt_list, (GCompareFunc)wintree_compare);
  return wt_list;
}

void wintree_focus ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] focus",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
  {
    zwlr_foreign_toplevel_handle_v1_unset_minimized(id);
    foreign_toplevel_activate(id);
  }
}

void wintree_minimize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] move window to scratchpad",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
    zwlr_foreign_toplevel_handle_v1_set_minimized(id);
  wintree_set_focus(NULL);
}

void wintree_unminimize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] focus",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
    zwlr_foreign_toplevel_handle_v1_unset_minimized(id);
}

void wintree_maximize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] fullscreen enable",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
    zwlr_foreign_toplevel_handle_v1_set_maximized(id);
}

void wintree_unmaximize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] fullscreen disable",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
    zwlr_foreign_toplevel_handle_v1_unset_maximized(id);
}

void wintree_close ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] kill",GPOINTER_TO_INT(id));
  else if(foreign_toplevel_is_active())
    zwlr_foreign_toplevel_handle_v1_close(id);
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
