/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */


#include <gtk/gtk.h>
#include "sfwbar.h"

GtkWidget *pager_init ( struct context *context )
{
  context->pager = gtk_grid_new();
  context->features |= F_PAGER;
  pager_update(context);
  return context->pager;
}

void pager_remove_button ( GtkWidget *widget, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->pager), widget );
}

void pager_button_click( GtkWidget *widget, struct context *context )
{
  char buff[256];
  gchar *label;
  if(context->ipc==-1)
    return;
  label = (gchar *)gtk_button_get_label(GTK_BUTTON(widget));
  if(label==NULL)
    return;
  snprintf(buff,255,"workspace '%s'",label);
  ipc_send ( context->ipc, 0, buff );
}

void pager_update ( struct context *context )
{
  int c=0;
  int sock;
  gint32 etype;
  const ucl_object_t *obj,*iter;
  struct ucl_parser *parse;
  GList *wslist = NULL;
  GList *visible = NULL;
  GList *node;
  gchar *response;
  ucl_object_iter_t *itp;
  GtkWidget *widget;
  gchar *label;
  gchar *focus=NULL;

  sock=ipc_open(3000);
  if(sock==-1)
    return;
  
  ipc_send(sock,1,"");
  response = ipc_poll(sock,&etype);
  close(sock);
  parse = ucl_parser_new(0);
  if(response!=NULL)
    ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);

  if(obj)
  {
    itp = ucl_object_iterate_new(obj);
    while((iter = ucl_object_iterate_safe(itp,true))!=NULL)
      {
        label = ucl_string_by_name(iter,"name");
        if(label!=NULL)
        {
          wslist = g_list_append(wslist,label);
          if(ucl_bool_by_name(iter,"visible",FALSE)==TRUE)
            visible = g_list_append(visible,label);
          if(ucl_bool_by_name(iter,"focused",FALSE)==TRUE)
            focus = label;
        }
      }
    ucl_object_unref((ucl_object_t *)obj);
  }

  for(node=context->pager_pins;node!=NULL;node=g_list_next(node))
    if(g_list_find_custom(wslist,node->data,(int (*)(const void *, const void *))g_strcmp0)==NULL)
      wslist = g_list_append(wslist,g_strdup(node->data));
  wslist = g_list_sort(wslist,(int (*)(const void *, const void *))g_strcmp0);

  if(wslist)
  {
    gtk_container_foreach(GTK_CONTAINER(context->pager),(GtkCallback)pager_remove_button,context);
    for(node=wslist;node!=NULL;node=g_list_next(node))
      {
        label = node->data;
        widget = gtk_button_new_with_label(label);
        gtk_widget_set_name(widget, "pager_normal");
        if(g_list_find_custom(visible,label,(int (*)(const void *, const void *))g_strcmp0)!=NULL)
          gtk_widget_set_name(widget, "pager_visible");
        if(focus==label)
          gtk_widget_set_name(widget, "pager_focused");
        g_signal_connect(widget,"clicked",G_CALLBACK(pager_button_click),context);
        gtk_grid_attach(GTK_GRID(context->pager),widget, c/(context->pager_rows),
          c%(context->pager_rows),1,1);
        c++;
      }
    gtk_widget_show_all(context->pager);
  }

  g_list_free(visible);
  g_list_free_full(wslist,g_free);
  ucl_parser_free(parse);
  g_free(response);
}
