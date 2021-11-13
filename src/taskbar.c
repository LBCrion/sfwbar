/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

GtkWidget *taskbar_init ( GtkWidget *widget )
{
  GtkWidget *w;
  gint i;

  context->features |= F_TASKBAR;
  if(!(context->features & F_TB_ICON))
    context->features |= F_TB_LABEL;
  if((context->tb_rows<1)&&(context->tb_cols<1))
    context->tb_rows = 1;
  if((context->tb_rows>0)&&(context->tb_cols>0))
    context->tb_cols = -1;

  context->box = widget;
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

void taskbar_remove_button ( GtkWidget *widget, gpointer data )
{
  gtk_container_remove ( GTK_CONTAINER(context->box), widget );
}

void taskbar_button_click( GtkWidget *widget, gpointer data )
{
  struct wt_window *button = g_object_get_data(G_OBJECT(widget),"parent");
  gchar *cmd;

  if(button == NULL)
    return;

  if(context->ipc >=0 )
  {
    if ( button->wid == context->tb_focus)
      cmd = g_strdup_printf("[con_id=%ld] move window to scratchpad",AS_WINDOW(button)->wid);
    else
      cmd = g_strdup_printf("[con_id=%ld] focus",AS_WINDOW(button)->wid);
    sway_ipc_send ( context->ipc, 0, cmd );
    g_free( cmd );
    return;
  }

  if ( button->wid == context->tb_focus )
  {
    zwlr_foreign_toplevel_handle_v1_set_minimized(button->wlr);
    if ( button->wid == context->tb_focus )
    {
      context->tb_focus = -1;
      context->wt_dirty = 1;
    }
  }
  else
  {
    zwlr_foreign_toplevel_handle_v1_unset_minimized(button->wlr);
    zwlr_foreign_toplevel_handle_v1_activate(button->wlr,seat);
  }
}

gint win_compare ( struct wt_window *a, struct wt_window *b)
{
  gint s;
  s = g_strcmp0(a->title,b->title);
  if(s==0)
    return (a->wid - b->wid);
  return s;
}

void taskbar_window_init ( struct wt_window *win )
{
  GtkWidget *box,*icon;

  if(!(context->features & F_TASKBAR))
    return;

  win->button = gtk_button_new();
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(win->button),box);
  if(context->features & F_TB_ICON)
  {
    icon = scale_image_new();
    gtk_container_add(GTK_CONTAINER(box),icon);
    scale_image_set_image(icon,win->appid);
  }
  if(context->features & F_TB_LABEL)
  {
    win->label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(win->label),PANGO_ELLIPSIZE_END);
    widget_set_css(win->label);
    gtk_container_add(GTK_CONTAINER(box),win->label);
  }

  g_object_set_data(G_OBJECT(win->button),"parent",win);
  g_object_ref(G_OBJECT(win->button));
  g_signal_connect(win->button,"clicked",G_CALLBACK(taskbar_button_click),context);
}

void taskbar_refresh( void )
{
  GList *item;
  gint tb_count=0;
  gtk_container_foreach(GTK_CONTAINER(context->box),
      (GtkCallback)taskbar_remove_button,context);
  context->wt_list = g_list_sort(context->wt_list,
      (GCompareFunc)wintree_compare);
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
