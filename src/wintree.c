/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"


void wintree_traverse_tree ( const ucl_object_t *obj, struct context *context )
{
  const ucl_object_t *iter,*arr;
  ucl_object_iter_t *itp;
  struct ipc_event ev;
  arr = ucl_object_lookup(obj,"floating_nodes");
  if( arr )
  {
    itp = ucl_object_iterate_new(arr);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
    {
      ev.appid = ucl_string_by_name(iter,"app_id");
      if(ev.appid!=NULL)
      {
        ev.title = ucl_string_by_name(iter,"name");
        ev.pid = ucl_int_by_name(iter,"pid",G_MININT64); 
        ev.wid = ucl_int_by_name(iter,"id",G_MININT64); 
        if(ucl_bool_by_name(iter,"focused",FALSE) == TRUE)
          context->tb_focus = ev.wid;
        ev.event = 99;
        dispatch_event(&ev,context);
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
}

gint wintree_compare ( gconstpointer a, gconstpointer b)
{
  gint s;
  s = g_strcmp0(a,b);
  if(s==0)
    return a-b;
  return s;
}

void wintree_update_window (struct ipc_event *ev, struct context *context)
{
  GList *iter,*item;
  struct wt_window *win;

  if (ev->title == NULL )
    ev->title = g_strdup(ev->appid);
  if (ev->title == NULL )
    return;

  item = NULL;
  for(iter=context->wt_list;iter!=NULL;iter = g_list_next(iter))
    if (AS_WINDOW(iter->data)->wid == ev->wid)
      item=iter;
  if (item != NULL)
  {
    if(context->features & F_TB_LABEL)
      gtk_label_set_text(GTK_LABEL(AS_WINDOW(item->data)->label),ev->title);
    return;
  }

  win = g_malloc(sizeof(struct wt_window));
  if ( win != NULL && ev->pid != getpid())
  {
    win->pid = ev->pid;
    win->wid = ev->wid;
    win->appid = g_strdup(ev->appid);
    win->title = g_strdup(ev->title);

    taskbar_update_window(ev,context,win);
    switcher_update_window(ev,context,win);


    context->wt_list = g_list_insert_sorted (context->wt_list,win,wintree_compare);
  }
}

void wintree_delete_window (gint64 wid, struct context *context)
{
  GList *iter,*item;
  item = NULL;
  for(iter=context->wt_list;iter!=NULL;iter = g_list_next(iter))
    if (AS_WINDOW(iter->data)->wid == wid)
      item=iter;
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
