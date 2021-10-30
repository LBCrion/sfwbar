/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

struct wt_window *wintree_window_init ( void )
{
  struct wt_window *w;
  w = g_malloc(sizeof(struct wt_window));
  w->button = NULL;
  w->label = NULL;
  w->switcher = NULL;
  w->title = NULL;
  w->appid = NULL;
  w->pid=-1;
  w->wid=-1;
  w->wlr=NULL;
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

void wintree_window_append ( struct wt_window *win )
{
  if(win->button==NULL)
    taskbar_window_init(win);
  if(win->switcher==NULL)
    switcher_window_init(win);
  if(g_list_find(context->wt_list,win)==NULL)
    context->wt_list = g_list_append (context->wt_list,win);
  context->wt_dirty = 1;
}
