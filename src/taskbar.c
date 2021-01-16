/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */


#include <glib.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

void taskbar_traverse_tree ( const ucl_object_t *obj, struct context *context );

GtkWidget *taskbar_init ( struct context *context )
{
  GtkWidget *w;
  int i;

  context->features |= F_TASKBAR;

  context->box = clamp_grid_new();
  g_object_set(context->box,"column-homogeneous",TRUE,NULL);

  w = gtk_button_new();
  gtk_widget_set_name(w, "taskbar_active");
  gtk_grid_attach(GTK_GRID(context->box),w,1,1,1,1);

  for(i=1;i<context->tb_rows;i++)
  {
    w = gtk_button_new();
    gtk_widget_set_name(w, "taskbar_normal");
    gtk_grid_attach(GTK_GRID(context->box),w,1,i+1,1,1);
  } 

  return context->box;
}

void taskbar_populate ( struct context *context )
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
  if(response==NULL)
    return;
  parse = ucl_parser_new(0);
  ucl_parser_add_string(parse,response,strlen(response));
  obj = ucl_parser_get_object(parse);
  if(obj!=NULL)
  {
    taskbar_traverse_tree(obj,context);
    ucl_object_unref((ucl_object_t *)obj);
  }
  ucl_parser_free(parse);
  g_free(response);
  close(sock);
}

void taskbar_traverse_tree ( const ucl_object_t *obj, struct context *context )
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
        if(ucl_bool_by_name(iter,"focused",FALSE) == TRUE)
          context->tb_focus = ev.pid;
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
      taskbar_traverse_tree(iter,context);
    ucl_object_iterate_free(itp);
  }
}

void taskbar_remove_button ( GtkWidget *widget, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->box), widget );
}

void taskbar_button_click( GtkWidget *widget, struct context *context )
{
  char buff[256];
  struct tb_button *button = g_object_get_data(G_OBJECT(widget),"parent");
  if(button == NULL)
    return;
  if ( button->pid == context->tb_focus)
    snprintf(buff,255,"[pid=%ld] move window to scratchpad",button->pid);
  else
    snprintf(buff,255,"[pid=%ld] focus",button->pid);
  ipc_send ( context->ipc, 0, buff );
}

void taskbar_update_window (struct ipc_event *ev, struct context *context)
{
  GList *iter,*item;
  GtkWidget *box,*icon;
  struct tb_button *button;

  if (ev->title == NULL )
    ev->title = g_strdup(ev->appid);
  if (ev->title == NULL )
    return;

  item = NULL;
  for(iter=context->buttons;iter!=NULL;iter = g_list_next(iter))
    if (AS_BUTTON(iter->data)->pid == ev->pid)
      item=iter;
  if (item != NULL)
  {
    if(context->features & F_TB_LABEL)
      gtk_label_set_text(GTK_LABEL(AS_BUTTON(item->data)->label),ev->title);
    return;
  }

  button = g_malloc(sizeof(struct tb_button));
  if ( button != NULL)
  {
    button->pid = ev->pid;
    button->button = gtk_button_new();

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_container_add(GTK_CONTAINER(button->button),box);
    if(context->features & F_TB_ICON)
    {
      icon = gtk_image_new_from_icon_name(ev->appid,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_image_set_pixel_size(GTK_IMAGE(icon),context->tb_isize);
      gtk_container_add(GTK_CONTAINER(box),icon);
    }
    if(context->features & F_TB_LABEL)
    {
      button->label = gtk_label_new(ev->title);
      gtk_label_set_ellipsize (GTK_LABEL(button->label),PANGO_ELLIPSIZE_END);
      gtk_container_add(GTK_CONTAINER(box),button->label);
    }

    g_object_set_data(G_OBJECT(button->button),"parent",button);
    g_object_ref(G_OBJECT(button->button));
    g_signal_connect(button->button,"clicked",G_CALLBACK(taskbar_button_click),context);
    context->buttons = g_list_append (context->buttons,button);
  }
}

void taskbar_refresh( struct context *context )
{
  GList *item;
  gint tb_count=0;
  gtk_container_foreach(GTK_CONTAINER(context->box),(GtkCallback)taskbar_remove_button,context);
  for (item = context->buttons; item!= NULL; item = g_list_next(item) )
  {
    if (AS_BUTTON(item->data)->pid == context->tb_focus)
      gtk_widget_set_name(AS_BUTTON(item->data)->button, "taskbar_active");
    else
      gtk_widget_set_name(AS_BUTTON(item->data)->button, "taskbar_normal");
    gtk_grid_attach(GTK_GRID(context->box),AS_BUTTON(item->data)->button,
      (tb_count)/(context->tb_rows),(tb_count)%(context->tb_rows),1,1);
    tb_count++;
  }
  gtk_widget_show_all(context->box);
}

void taskbar_delete_window (gint64 pid, struct context *context)
{
  GList *iter,*item;
  item = NULL;
  for(iter=context->buttons;iter!=NULL;iter = g_list_next(iter))
    if (AS_BUTTON(iter->data)->pid == pid)
      item=iter;
  if(item==NULL)
    return;
  gtk_widget_destroy((AS_BUTTON(item->data))->button);
  g_object_unref(G_OBJECT(AS_BUTTON(item->data)->button));
  context->buttons = g_list_delete_link(context->buttons,item);
}

