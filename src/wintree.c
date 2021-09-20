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
  w = malloc(sizeof(struct wt_window));
  if(w==NULL)
    return NULL;
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

void wintree_traverse_tree ( const ucl_object_t *obj, struct context *context )
{
  const ucl_object_t *iter,*arr;
  struct wt_window *win;
  ucl_object_iter_t *itp;
  gchar *appid;

  arr = ucl_object_lookup(obj,"floating_nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
    {
      appid = ucl_string_by_name(iter,"app_id");
      if(appid!=NULL)
      {
        win = wintree_window_init();
        win->appid = appid;
        win->title = ucl_string_by_name(iter,"name");
        win->pid = ucl_int_by_name(iter,"pid",G_MININT64); 
        win->wid = ucl_int_by_name(iter,"id",G_MININT64); 
        if(ucl_bool_by_name(iter,"focused",FALSE) == TRUE)
          context->tb_focus = win->wid;
        taskbar_window_init(context,win);
        switcher_window_init(context,win);
        context->wt_list = g_list_insert_sorted (context->wt_list,win,wintree_compare);
      }
    }
    ucl_object_iterate_free(itp);
  }
  arr = ucl_object_lookup(obj,"nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
      wintree_traverse_tree(iter,context);
    ucl_object_iterate_free(itp);
  }
}

void wintree_populate ( struct context *context )
{
  const ucl_object_t *obj;
  struct ucl_parser *parse;
  gchar *response;
  int sock;
  gint32 etype;
  sock=ipc_open(3000);
  if(sock==-1)
    return;
  ipc_send(sock,4,"");
  response = ipc_poll(sock,&etype);
  close(sock);
  if(response==NULL)
    return;
  parse = ucl_parser_new(0);
  ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);
  if(obj!=NULL)
  {
    wintree_traverse_tree(obj,context);
    ucl_object_unref((ucl_object_t *)obj);
  }
  ucl_parser_free(parse);
  g_free(response);
  taskbar_refresh(context);
}

gint wintree_compare ( gconstpointer a, gconstpointer b)
{
  gint s;
  s = g_strcmp0(a,b);
  if(s==0)
    return a-b;
  return s;
}

void wintree_window_new (const ucl_object_t *obj, struct context *context)
{
  GList *item;
  struct wt_window *win;
  const ucl_object_t *container;
  gint64 wid;

  container = ucl_object_lookup(obj,"container");
  wid = ucl_int_by_name(container,"id",G_MININT64); 
  for(item=context->wt_list;item!=NULL;item = g_list_next(item))
    if (AS_WINDOW(item->data)->wid == wid)
      return;
  
  win = wintree_window_init();
  if(win==NULL)
    return;

  win->wid = wid;
  win->pid = ucl_int_by_name(container,"pid",G_MININT64); 

  if (context->features & F_PLACEMENT)
    place_window(win->wid, win->pid, context);

  win->appid = ucl_string_by_name(container,"app_id");
  if(win->appid==NULL)
  {
    g_free(win);
    return;
  }

  win->title = ucl_string_by_name(container,"name");
  if (win->title == NULL )
    win->title = g_strdup(win->appid);

  taskbar_window_init(context,win);
  switcher_window_init(context,win);
  context->wt_list = g_list_insert_sorted (context->wt_list,win,wintree_compare);
}

void wintree_window_title (const ucl_object_t *obj, struct context *context)
{
  GList *item;
  gchar *title;
  const ucl_object_t *container;
  gint64 wid;

  if(!(context->features & F_TB_LABEL))
    return;

  container = ucl_object_lookup(obj,"container");
  wid = ucl_int_by_name(container,"id",G_MININT64);
  title = ucl_string_by_name(container,"name");

  if(title==NULL)
    return;

  for(item=context->wt_list;item!=NULL;item = g_list_next(item))
    if (AS_WINDOW(item->data)->wid == wid)
    {
      str_assign(&(AS_WINDOW(item->data)->title),title);
      gtk_label_set_text(GTK_LABEL((AS_WINDOW(item->data))->label),title);
    }

  g_free(title);
}

void wintree_window_close (const ucl_object_t *obj, struct context *context)
{
  GList *item;
  const ucl_object_t *container;
  gint64 wid;

  container = ucl_object_lookup(obj,"container");
  wid = ucl_int_by_name(container,"id",G_MININT64);

  for(item=context->wt_list;item!=NULL;item = g_list_next(item))
    if (AS_WINDOW(item->data)->wid == wid)
      break;
  if(item==NULL)
    return;

  if(context->features & F_TASKBAR)
  {
    gtk_widget_destroy((AS_WINDOW(item->data))->button);
    g_object_unref(G_OBJECT(AS_WINDOW(item->data)->button));
  }
  if(context->features & F_SWITCHER)
  {
    gtk_widget_destroy((AS_WINDOW(item->data))->switcher);
    g_object_unref(G_OBJECT(AS_WINDOW(item->data)->switcher));
  }
  g_free(AS_WINDOW(item->data)->appid);
  g_free(AS_WINDOW(item->data)->title);
  context->wt_list = g_list_delete_link(context->wt_list,item);
}

void wintree_set_focus (const ucl_object_t *obj, struct context *context)
{
  const ucl_object_t *container;

  container = ucl_object_lookup(obj,"container");
  context->tb_focus = ucl_int_by_name(container,"id",G_MININT64);
}

void wintree_event ( struct context *context )
{
  const ucl_object_t *obj;
  struct ucl_parser *parse;
  gchar *response,*change;
  gint32 etype;

  if(context->ipc==-1)
    return;

  response = ipc_poll(context->ipc,&etype);
  while (response != NULL)
  { 
    parse = ucl_parser_new(0);
    ucl_parser_add_string(parse,response,strlen(response));
    obj = ucl_parser_get_object(parse);

    if(etype==0x80000000)
      pager_update(context);

    if(etype==0x80000004)
      switcher_event(context,obj);

    if(etype==0x80000003)
    {
      if(obj!=NULL)
      {
        change = ucl_string_by_name(obj,"change");
        if(change!=NULL)
        {
          if(g_strcmp0(change,"new")==0)
            wintree_window_new (obj,context);
          if(g_strcmp0(change,"close")==0)
            wintree_window_close(obj,context);
          if(g_strcmp0(change,"title")==0)
            wintree_window_title(obj,context);
          if(g_strcmp0(change,"focus")==0)
            wintree_set_focus(obj,context);
          context->wt_dirty=1;
        }
      }
    }

    ucl_object_unref((ucl_object_t *)obj);
    ucl_parser_free(parse);
    g_free(response);
    response = ipc_poll(context->ipc,&etype);
  }
}
