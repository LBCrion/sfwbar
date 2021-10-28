/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

void switcher_init ( const ucl_object_t *obj )
{
  const ucl_object_t *ptr;
  gchar *css;
  if((ptr=ucl_object_lookup(obj,"switcher"))==NULL)
    return;
  context->features |= F_SWITCHER;
  context->sw_max = ucl_int_by_name(ptr,"delay",1);
  context->sw_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW(context->sw_win));
  gtk_layer_set_layer(GTK_WINDOW(context->sw_win),GTK_LAYER_SHELL_LAYER_OVERLAY);
  context->sw_box = gtk_grid_new();
  gtk_widget_set_name(context->sw_box, "switcher");
  gtk_widget_set_name(context->sw_win, "switcher");
  gtk_container_add(GTK_CONTAINER(context->sw_win),context->sw_box);
  if(ucl_bool_by_name(ptr,"icon",TRUE))
    context->features |= F_SW_ICON;
  if(ucl_bool_by_name(ptr,"title",TRUE))
    context->features |= F_SW_LABEL;
  if(!(context->features & F_SW_ICON))
    context->features |= F_SW_LABEL;
  context->sw_cols = ucl_int_by_name(ptr,"columns",1);
  css = ucl_string_by_name(ptr,"css");
  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (context->sw_box);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }
}

gboolean switcher_event ( const ucl_object_t *obj )
{
  gchar *mode;
  GList *item, *focus;
  gboolean event = FALSE;

  if(obj!=NULL)
  {
    mode = ucl_string_by_name(obj,"mode");
    if(mode!=NULL)
    {
      if(*mode=='h')
        gtk_widget_hide(GTK_WIDGET(context->window));
      else
        gtk_widget_show(GTK_WIDGET(context->window));
    }

    if(!(context->features & F_SWITCHER))
    {
      mode = ucl_string_by_name(obj,"hidden_state");
      if(mode!=NULL)
      {
        if(*mode!=context->sw_hstate)
        {
          context->sw_hstate = *mode;
          event=TRUE;
        }
      }
    }

    if(event)
    {
      context->sw_count = context->sw_max;
      focus = NULL;
      for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
        if (AS_WINDOW(item->data)->wid == context->tb_focus)
          focus = g_list_next(item);
      if(focus==NULL)
        focus=context->wt_list;
      if(focus!=NULL)
      {
        context->tb_focus = AS_WINDOW(focus->data)->wid;
      }
    }
    g_free(mode);
  }
  return TRUE;
}

void switcher_delete ( GtkWidget *w )
{
  gtk_container_remove ( GTK_CONTAINER(context->sw_box), w );
}

void switcher_window_init ( struct wt_window *win)
{
  GtkWidget *img;
  if(!(context->features & F_SWITCHER))
    return;
  win->switcher = gtk_grid_new();
  g_object_ref(G_OBJECT(win->switcher));
  gtk_widget_set_hexpand(win->switcher,TRUE);
  if(context->features & F_SW_LABEL)
    gtk_grid_attach(GTK_GRID(win->switcher),gtk_label_new(win->title),2,1,1,1);
  if(context->features & F_SW_ICON)
  {
    img = scale_image_new();
    scale_image_set_image(img,win->appid);
    gtk_grid_attach(GTK_GRID(win->switcher),img,1,1,1,1);
  }
}

void switcher_update ( void )
{
  GList *item;
  gint i = 0;
  gchar *cmd;
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
  {
    gtk_widget_hide(GTK_WIDGET(context->sw_win));
    if(context->ipc>=0)
    {
       cmd = g_strdup_printf("[con_id=%ld] focus",context->tb_focus);
       sway_ipc_send ( context->ipc, 0, cmd );
       g_free( cmd );
    }
    else
      for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
        if (AS_WINDOW(item->data)->wid == context->tb_focus)
          zwlr_foreign_toplevel_handle_v1_activate(AS_WINDOW(item->data)->wlr, seat);
  }
}


