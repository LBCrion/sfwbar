/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

GtkWidget *taskbar_init ( GtkWidget *widget )
{
  context->features |= F_TASKBAR;
  if(!(context->features & F_TB_ICON))
    context->features |= F_TB_LABEL;

  context->box = widget;

  return context->box;
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
      context->status |= ST_TASKBAR;
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

  flow_grid_clean(context->box);
  context->wt_list = g_list_sort(context->wt_list,
      (GCompareFunc)wintree_compare);
  for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
  {
    if (AS_WINDOW(item->data)->wid == context->tb_focus)
      gtk_widget_set_name(AS_WINDOW(item->data)->button, "taskbar_active");
    else
      gtk_widget_set_name(AS_WINDOW(item->data)->button, "taskbar_normal");
    gtk_widget_unset_state_flags(AS_WINDOW(item->data)->button, GTK_STATE_FLAG_PRELIGHT);

    widget_set_css(AS_WINDOW(item->data)->button);
    flow_grid_attach(context->box,AS_WINDOW(item->data)->button);
  }
  flow_grid_pad(context->box);
  gtk_widget_show_all(context->box);
  context->status &= ~ST_TASKBAR;
}
