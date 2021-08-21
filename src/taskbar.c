/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

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

  if(context->tb_rows>0)
    for(i=1;i<context->tb_rows;i++)
    {
      w = gtk_button_new();
      gtk_widget_set_name(w, "taskbar_normal");
      gtk_grid_attach(GTK_GRID(context->box),w,1,i+1,1,1);
    } 

  return context->box;
}

void taskbar_remove_button ( GtkWidget *widget, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->box), widget );
}

void taskbar_button_click( GtkWidget *widget, struct context *context )
{
  char buff[256];
  struct wt_window *button = g_object_get_data(G_OBJECT(widget),"parent");

  if((button == NULL)||(context->ipc==-1))
    return;
  if ( button->wid == context->tb_focus)
    snprintf(buff,255,"[con_id=%ld] move window to scratchpad",button->wid);
  else
    snprintf(buff,255,"[con_id=%ld] focus",button->wid);
  ipc_send ( context->ipc, 0, buff );
}

void taskbar_update_window (struct ipc_event *ev, struct context *context, struct wt_window *win)
{
  GtkWidget *box,*icon;
  if(context->features & F_TASKBAR)
  {
    win->button = gtk_button_new();
    box = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(win->button),box);
    if(context->features & F_TB_ICON)
    {
      icon = widget_icon_by_name(ev->appid,context->tb_isize);
      gtk_container_add(GTK_CONTAINER(box),icon);
    }
    if(context->features & F_TB_LABEL)
    {
      win->label = gtk_label_new(ev->title);
      gtk_label_set_ellipsize (GTK_LABEL(win->label),PANGO_ELLIPSIZE_END);
      widget_set_css(win->label);
      gtk_container_add(GTK_CONTAINER(box),win->label);
    }

    g_object_set_data(G_OBJECT(win->button),"parent",win);
    g_object_ref(G_OBJECT(win->button));
    g_signal_connect(win->button,"clicked",G_CALLBACK(taskbar_button_click),context);
  }
}

void taskbar_refresh( struct context *context )
{
  GList *item;
  gint tb_count=0;
  gtk_container_foreach(GTK_CONTAINER(context->box),(GtkCallback)taskbar_remove_button,context);
  for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
  {
    if (AS_WINDOW(item->data)->wid == context->tb_focus)
      gtk_widget_set_name(AS_WINDOW(item->data)->button, "taskbar_active");
    else
      gtk_widget_set_name(AS_WINDOW(item->data)->button, "taskbar_normal");
    widget_set_css(AS_WINDOW(item->data)->button);
    if(context->tb_rows>0)
      gtk_grid_attach(GTK_GRID(context->box),AS_WINDOW(item->data)->button,
        (tb_count)/(context->tb_rows),(tb_count)%(context->tb_rows),1,1);
    else
      gtk_grid_attach(GTK_GRID(context->box),AS_WINDOW(item->data)->button,
        (tb_count)%(context->tb_cols),(tb_count)/(context->tb_cols),1,1);
    tb_count++;
  }
  gtk_widget_show_all(context->box);
}
