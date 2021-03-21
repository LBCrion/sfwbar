/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

void switcher_event ( struct context *context, const ucl_object_t *obj )
{
  char *mode;
  GList *item, *focus;
  char buff[256];

  if(obj==NULL)
    return;

  mode = ucl_string_by_name(obj,"mode");
  if(mode!=NULL)
  {
    if(*mode=='h')
      gtk_widget_hide(GTK_WIDGET(context->window));
    else
      gtk_widget_show(GTK_WIDGET(context->window));
    g_free(mode);
  }

  if(!(context->features & F_SWITCHER))
    return;

  mode = ucl_string_by_name(obj,"hidden_state");
  if(mode!=NULL)
  {
    if(*mode!=context->sw_hstate)
    {
      context->sw_hstate = *mode;
      context->sw_count = context->sw_max;
      focus = NULL;
      for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
        if (AS_WINDOW(item->data)->wid == context->tb_focus)
          focus = g_list_next(item);
      if(focus==NULL)
        focus=context->wt_list;
      if(focus!=NULL)
      {
        snprintf(buff,255,"[con_id=%ld] focus",AS_WINDOW(focus->data)->wid);
        ipc_send ( context->ipc, 0, buff );
      }
    }
    g_free(mode);
  }
}

void switcher_delete ( GtkWidget *w, struct context *context )
{
  gtk_container_remove ( GTK_CONTAINER(context->sw_box), w );
}

void switcher_update_window (struct ipc_event *ev, struct context *context, struct wt_window *win)
{
  GtkWidget *img;
  if(context->features & F_SWITCHER)
  {
    win->switcher = gtk_grid_new();
    g_object_ref(G_OBJECT(win->switcher));
    gtk_widget_set_hexpand(win->switcher,TRUE);
    if(context->features & F_SW_LABEL)
      gtk_grid_attach(GTK_GRID(win->switcher),gtk_label_new(win->title),2,1,1,1);
    if(context->features & F_SW_ICON)
    {
      img=gtk_image_new_from_icon_name(win->appid,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_image_set_pixel_size(GTK_IMAGE(img),context->sw_isize);
      gtk_grid_attach(GTK_GRID(win->switcher),img,1,1,1,1);
    }
  }
}


void switcher_update ( struct context *context )
{
  GList *item;
  int i = 1;
  if(context->sw_count <= 0)
    return;
  context->sw_count--;

  if(context->sw_count > 0)
  {
    gtk_container_foreach(GTK_CONTAINER(context->sw_box),(GtkCallback)switcher_delete,context);
    for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
    {
      if (AS_WINDOW(item->data)->wid == context->tb_focus)
        gtk_widget_set_name(AS_WINDOW(item->data)->switcher, "switcher_active");
      else
        gtk_widget_set_name(AS_WINDOW(item->data)->switcher, "switcher_normal");

      gtk_grid_attach(GTK_GRID(context->sw_box),AS_WINDOW(item->data)->switcher,
          i%context->sw_cols,i/context->sw_cols,1,1);
      i++;
    }
    gtk_widget_show_all(GTK_WIDGET(context->sw_win));
  }
  else
    gtk_widget_hide(GTK_WIDGET(context->sw_win));
}


