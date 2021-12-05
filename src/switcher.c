/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021 Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GtkWidget *window;
static GtkWidget *grid;
static gint interval;
static gchar hstate;
static gint counter;

void switcher_config ( GtkWidget *nwin, GtkWidget *ngrid, gint nmax)
{
  window = nwin;
  grid = ngrid;
  interval = nmax;
  hstate = 's';
  counter = 0;
}


gboolean hide_event ( struct json_object *obj )
{
  gchar *mode, state;

  if ( obj )
  {
    mode = json_string_by_name(obj,"mode");
    if(mode!=NULL)
      state = *mode;
    else
      state ='s';
    g_free(mode);
  }
  else
    if( gtk_widget_is_visible (GTK_WIDGET(context->window)) )
      state = 'h';
    else
      state = 's';

  if(state=='h')
    gtk_widget_hide(GTK_WIDGET(context->window));
  else
    gtk_widget_show(GTK_WIDGET(context->window));
  return TRUE;
}

gboolean switcher_event ( struct json_object *obj )
{
  gchar *mode;
  GList *item, *focus;
  gboolean event = FALSE;

  if(!(context->features & F_SWITCHER))
    return TRUE;

  if(obj!=NULL)
  {
    mode = json_string_by_name(obj,"hidden_state");
    if(mode!=NULL)
      if(*mode!=hstate)
      {
        if(hstate!=0)
          event=TRUE;
        hstate = *mode;
      }
    g_free(mode);
  }
  else
    event = TRUE;

  if(event)
  {
    counter = interval;
    focus = NULL;
    for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
      if (AS_WINDOW(item->data)->wid == context->tb_focus)
        focus = g_list_next(item);
    if(focus==NULL)
      focus=context->wt_list;
    if(focus!=NULL)
      context->tb_focus = AS_WINDOW(focus->data)->wid;
    context->status |= ST_SWITCHER;
  }

  return TRUE;
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
  gchar *cmd;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    if(!(context->status & ST_SWITCHER))
      return;
    flow_grid_clean(grid);
    for (item = context->wt_list; item!= NULL; item = g_list_next(item) )
    {
      if (AS_WINDOW(item->data)->wid == context->tb_focus)
        gtk_widget_set_name(AS_WINDOW(item->data)->switcher, "switcher_active");
      else
        gtk_widget_set_name(AS_WINDOW(item->data)->switcher, "switcher_normal");

      flow_grid_attach(grid,AS_WINDOW(item->data)->switcher);
    }
    gtk_widget_show_all(window);
    context->status &= ~ST_SWITCHER;
  }
  else
  {
    gtk_widget_hide(window);
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
