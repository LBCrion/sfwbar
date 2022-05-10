/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GList *wt_list;
static gpointer wt_focus;
static gchar *wt_active;

void wintree_set_active ( gchar *title )
{
  str_assign(&wt_active,title);
}

gchar *wintree_get_active ( void )
{
  return wt_active;
}

struct wt_window *wintree_window_init ( void )
{
  struct wt_window *w;
  w = g_malloc0(sizeof(struct wt_window));
  w->pid=-1;
  w->wid=-1;
  return w;
}

gint wintree_compare ( struct wt_window *a, struct wt_window *b)
{
  gint s;
  s = g_strcmp0(a->title,b->title);
  if(s==0)
    return a->wid-b->wid;
  return s;
}

void wintree_set_focus ( gpointer id )
{
  wt_focus = id;
}

gpointer wintree_get_focus ( void )
{
  return wt_focus;
}

gboolean wintree_is_focused ( gpointer id )
{
  return ( id == wt_focus );
}

struct wt_window *wintree_from_id ( gpointer id )
{
  GList *item;
  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((struct wt_window *)(item->data))->uid == id )
      break;
  if(!item)
    return NULL;
  return item->data;
}

struct wt_window *wintree_from_pid ( gint64 pid )
{
  GList *item;
  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((struct wt_window *)(item->data))->pid == pid )
      break;
  if(!item)
    return NULL;
  return item->data;
}

void wintree_window_append ( struct wt_window *win )
{
  if(!win)
    return;

  if( !win->button && (win->title || win->appid) )
    taskbar_item_init_for_all(win);
  if( !win->switcher && (win->title || win->appid) )
    switcher_window_init(win);
  if(g_list_find(wt_list,win)==NULL)
    wt_list = g_list_append (wt_list,win);
  taskbar_invalidate_all();
  switcher_invalidate();
}

void wintree_window_delete ( gpointer id )
{
  GList *item;
  struct wt_window *win;

  for(item = wt_list; item; item = g_list_next(item) )
    if ( ((struct wt_window *)(item->data))->uid == id )
      break;
  if(!item)
    return;
  win = item->data;
  if(!win)
    return;
  if(win->button)
  {
    gtk_widget_destroy(win->button);
    g_object_unref(G_OBJECT(win->button));
  }
  if(win->switcher)
  {
    gtk_widget_destroy(win->switcher);
    g_object_unref(G_OBJECT(win->switcher));
  }
  str_assign(&(win->appid),NULL);
  str_assign(&(win->title),NULL);
  wt_list = g_list_delete_link(wt_list,item);
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
  else
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
  else
    zwlr_foreign_toplevel_handle_v1_set_minimized(id);
  wintree_set_focus(NULL);
}

void wintree_unminimize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] focus",GPOINTER_TO_INT(id));
  else
    zwlr_foreign_toplevel_handle_v1_unset_minimized(id);
}

void wintree_maximize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] fullscreen enable",GPOINTER_TO_INT(id));
  else
    zwlr_foreign_toplevel_handle_v1_set_maximized(id);
}

void wintree_unmaximize ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] fullscreen disable",GPOINTER_TO_INT(id));
  else
    zwlr_foreign_toplevel_handle_v1_unset_maximized(id);
}

void wintree_close ( gpointer id )
{
  if(!id)
    return;

  if(sway_ipc_active())
    sway_ipc_command("[con_id=%ld] kill",GPOINTER_TO_INT(id));
  else
    zwlr_foreign_toplevel_handle_v1_close(id);
}
