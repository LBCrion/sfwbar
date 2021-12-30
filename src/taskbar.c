/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */


#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

static GtkWidget *taskbar;
static gboolean icons, labels;
static gboolean invalid;

void taskbar_init ( GtkWidget *widget )
{
  taskbar = widget;
}

void taskbar_invalidate ( void )
{
  invalid = TRUE;
}

void taskbar_set_visual ( gboolean nicons, gboolean nlabels )
{
  icons = nicons;
  labels = nlabels;

  if(!icons)
    labels = TRUE;
}

void taskbar_button_click( GtkWidget *widget, gpointer data )
{
  struct wt_window *button = g_object_get_data(G_OBJECT(widget),"parent");

  if(button == NULL)
    return;

  if(sway_ipc_active())
  {
    if ( wintree_is_focused(button->uid) )
      sway_ipc_command("[con_id=%ld] move window to scratchpad",button->wid);
    else
      sway_ipc_command("[con_id=%ld] focus",button->wid);
    return;
  }

  if ( wintree_is_focused(button->uid) )
  {
    zwlr_foreign_toplevel_handle_v1_set_minimized(button->uid);
    wintree_set_focus(NULL);
    taskbar_invalidate();
  }
  else
  {
    zwlr_foreign_toplevel_handle_v1_unset_minimized(button->uid);
    foreign_toplevel_activate(button->uid);
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
  GtkWidget *box,*icon,*label;
  gint dir;

  if(!taskbar)
    return;

  win->button = gtk_button_new();
  gtk_widget_set_name(win->button, "taskbar_normal");
  gtk_widget_style_get(win->button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(win->button),box);
  if(icons)
  {
    icon = scale_image_new();
    scale_image_set_image(icon,win->appid);
    gtk_grid_attach_next_to(GTK_GRID(box),icon,NULL,dir,1,1);
  }
  else
    icon = NULL;
  if(labels)
  {
    label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(label),PANGO_ELLIPSIZE_END);
    widget_set_css(label);
    gtk_grid_attach_next_to(GTK_GRID(box),label,icon,dir,1,1);
  }

  g_object_set_data(G_OBJECT(win->button),"parent",win);
  g_object_ref(G_OBJECT(win->button));
  g_signal_connect(win->button,"clicked",G_CALLBACK(taskbar_button_click),NULL);
}

void taskbar_set_label ( struct wt_window *win, gchar *title )
{
  GList *blist, *glist, *iter;

  if(!taskbar || !labels)
    return;

  if(!win->button)
    return;

  blist = gtk_container_get_children(GTK_CONTAINER(win->button));

  if(!blist)
    return;

  glist = gtk_container_get_children(GTK_CONTAINER((blist->data)));

  for(iter = glist; iter; iter = g_list_next(iter))
    if(GTK_IS_LABEL(iter->data))
      gtk_label_set_text(GTK_LABEL(iter->data), title);
  g_list_free(glist);
  g_list_free(blist);
}

void taskbar_update( void )
{
  GList *item;
  struct wt_window *win;

  if(!taskbar || !invalid)
    return;

  flow_grid_clean(taskbar);
  for (item = wintree_get_list(); item; item = g_list_next(item) )
  {
    win = item->data;
    if ( wintree_is_focused(win->uid) )
      gtk_widget_set_name(win->button, "taskbar_active");
    else
      gtk_widget_set_name(win->button, "taskbar_normal");
    gtk_widget_unset_state_flags(win->button, GTK_STATE_FLAG_PRELIGHT);

    widget_set_css(win->button);
    flow_grid_attach(taskbar,win->button);
  }
  flow_grid_pad(taskbar);
  gtk_widget_show_all(taskbar);
  invalid = FALSE;
}
