/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020 Lev Babiev
 */


#include <glib.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

void taskbar_traverse_tree ( json_object *obj, struct context *context );

GtkWidget *taskbar_init ( struct context *context )
{
  GtkWidget *w;

  context->features |= F_TASKBAR;

  context->box = gtk_grid_new();
  g_object_set(context->box,"column-homogeneous",TRUE,NULL);

  w = gtk_button_new();
  gtk_widget_set_name(w, "taskbar_active");
  gtk_grid_attach(GTK_GRID(context->box),w,1,1,1,1);
  w = gtk_button_new();
  gtk_widget_set_name(w, "taskbar_normal");
  gtk_grid_attach(GTK_GRID(context->box),w,1,2,1,1);

  return context->box;
}

void taskbar_populate ( struct context *context )
{
  json_object *obj;
  int sock;
  sock=ipc_open();
  if(sock==-1)
    return;
  ipc_send(sock,4,"");
  obj = ipc_poll(sock);
  if(obj!=NULL)
  {
    taskbar_traverse_tree(obj,context);
    json_object_put(obj);
  }
  close(sock);
}

void taskbar_traverse_tree ( json_object *obj, struct context *context )
{
  json_object *ptr,*iter,*arr;
  struct ipc_event ev;
  int i;
  json_object_object_get_ex(obj,"floating_nodes",&arr);
  if( json_object_is_type(arr, json_type_array))
    for(i=0;i<json_object_array_length(arr);i++)
    {
      iter = json_object_array_get_idx(arr,i);
      ev.appid = json_string_by_name(iter,"app_id");
      if(ev.appid!=NULL)
      {
        ev.title = json_string_by_name(iter,"name");
        ev.pid = json_int_by_name(iter,"pid",G_MININT64); 
        json_object_object_get_ex(iter,"focused",&ptr);
        if(json_object_get_boolean(ptr) == TRUE)
          context->tb_focus = ev.pid;
        ev.event = 99;
        dispatch_event(&ev,context);
      }
    }
  json_object_object_get_ex(obj,"nodes",&arr);
  if( json_object_is_type(arr, json_type_array))
    for(i=0;i<json_object_array_length(arr);i++)
      taskbar_traverse_tree(json_object_array_get_idx(arr,i),context);
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

