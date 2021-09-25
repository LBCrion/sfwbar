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

gint wintree_compare ( struct wt_window *a, struct wt_window *b)
{
  gint s;
  s = g_strcmp0(a->title,b->title);
  if(s==0)
    return a->wid-b->wid;
  return s;
}

void wintree_window_append ( struct context *context, struct wt_window *win )
{
  if(win->button==NULL)
    taskbar_window_init(context,win);
  if(win->switcher==NULL)
    switcher_window_init(context,win);
  if(g_list_find(context->wt_list,win)==NULL)
    context->wt_list = g_list_insert_sorted (context->wt_list,win,
      (GCompareFunc)wintree_compare);
  context->wt_dirty = 1;
}

void wintree_window_new (const ucl_object_t *container, struct context *context)
{
  GList *item;
  struct wt_window *win;
  gint64 wid;

  wid = ucl_int_by_name(container,"id",G_MININT64); 
  for(item=context->wt_list;item!=NULL;item = g_list_next(item))
    if (AS_WINDOW(item->data)->wid == wid)
      return;
  
  win = wintree_window_init();
  if(win==NULL)
    return;

  win->wid = wid;
  win->pid = ucl_int_by_name(container,"pid",G_MININT64); 

  if((context->features & F_PLACEMENT)&&(context->ipc!=-1))
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

  if(ucl_bool_by_name(container,"focused",FALSE) == TRUE)
    context->tb_focus = win->wid;

  wintree_window_append(context,win);
}

void wintree_window_title (const ucl_object_t *container, struct context *context)
{
  GList *item;
  gchar *title;
  gint64 wid;

  if(!(context->features & F_TB_LABEL))
    return;

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

void wintree_window_close (const ucl_object_t *container, struct context *context)
{
  GList *item;
  gint64 wid;

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

void wintree_set_focus (const ucl_object_t *container, struct context *context)
{
  context->tb_focus = ucl_int_by_name(container,"id",G_MININT64);
}

void wintree_event ( struct context *context )
{
  const ucl_object_t *obj,*container;
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
          container = ucl_object_lookup(obj,"container");
          if(g_strcmp0(change,"new")==0)
            wintree_window_new (container,context);
          if(g_strcmp0(change,"close")==0)
            wintree_window_close(container,context);
          if(g_strcmp0(change,"title")==0)
            wintree_window_title(container,context);
          if(g_strcmp0(change,"focus")==0)
            wintree_set_focus(container,context);
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

void wintree_traverse_tree ( const ucl_object_t *obj, struct context *context )
{
  const ucl_object_t *iter,*arr;
  ucl_object_iter_t *itp;

  arr = ucl_object_lookup(obj,"floating_nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
      wintree_window_new (iter, context);
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

