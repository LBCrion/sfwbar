/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021 Lev Babiev
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"

static GtkWidget *switcher;
static GtkWidget *grid;
static gint interval;
static gchar hstate;
static gint counter;
static gboolean icons, labels;
static gboolean invalid;
static GList *focus;

void switcher_config ( gint ncols, gchar *css, gint nmax,
    gboolean nicons, gboolean nlabels)
{
  if(!switcher)
  {
    switcher = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window (GTK_WINDOW(switcher));
    gtk_layer_set_layer(GTK_WINDOW(switcher),GTK_LAYER_SHELL_LAYER_OVERLAY);
    grid = flow_grid_new(FALSE);
    gtk_widget_set_name(grid, "switcher");
    gtk_widget_set_name(switcher, "switcher");
    gtk_container_add(GTK_CONTAINER(switcher),grid);
  }
  flow_grid_set_cols(grid,ncols);
  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (grid);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(css);
  }
  interval = nmax;
  hstate = 's';
  icons = nicons;
  labels = nlabels;
  if(!icons)
    labels = TRUE;
}

void switcher_invalidate ( void )
{
  invalid = TRUE;
}

gboolean switcher_event ( struct json_object *obj )
{
  gchar *mode;
  GList *item;
  gboolean event = FALSE;

  if(!switcher)
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
    for (item = wintree_get_list(); item; item = g_list_next(item) )
      if ( wintree_is_focused(AS_WINDOW(item->data)->uid) )
        focus = g_list_next(item);
    if(focus==NULL)
      focus=wintree_get_list();
    if(focus!=NULL)
      wintree_set_focus(AS_WINDOW(focus->data)->uid);
    switcher_invalidate();
  }

  return TRUE;
}

void switcher_window_init ( struct wt_window *win)
{
  GtkWidget *icon,*label;
  gint dir;

  if(!switcher)
  {
    win->switcher = NULL;
    return;
  }

  win->switcher = gtk_grid_new();
  gtk_widget_set_name(win->switcher, "switcher_normal");
  gtk_widget_style_get(win->switcher,"direction",&dir,NULL);
  g_object_ref(G_OBJECT(win->switcher));
  if(icons)
  {
    icon = scale_image_new();
    scale_image_set_image(icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(win->switcher),icon,NULL,dir,1,1);
  }
  else
    icon = NULL;
  if(labels)
  {
    label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(label),PANGO_ELLIPSIZE_END);
    widget_set_css(label);
    gtk_grid_attach_next_to(GTK_GRID(win->switcher),label,icon,dir,1,1);
  }
}

void switcher_set_label ( struct wt_window *win, gchar *title )
{
  GList *glist, *iter;

  if(!switcher || !labels)
    return;

  if(!win->switcher)
    return;

  glist = gtk_container_get_children(GTK_CONTAINER(win->switcher));

  for(iter = glist; iter; iter = g_list_next(iter))
    if(GTK_IS_LABEL(iter->data))
      gtk_label_set_text(GTK_LABEL(iter->data), title);
  g_list_free(glist);
}

void switcher_update ( void )
{
  GList *item;
  struct wt_window *win;

  if(!switcher)
    return;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    if(!invalid)
      return;

    flow_grid_clean(grid);
    for (item = wintree_get_list(); item; item = g_list_next(item) )
    {
      win = item->data;
      if (wintree_is_focused(win->uid) )
        gtk_widget_set_name(win->switcher,"switcher_active");
      else
        gtk_widget_set_name(win->switcher,"switcher_normal");

      flow_grid_attach(grid,win->switcher);
    }
    gtk_widget_show_all(switcher);
    invalid = FALSE;
  }
  else
  {
    gtk_widget_hide(switcher);
    win = focus->data;
    if(sway_ipc_active())
    {
      sway_ipc_command("[con_id=%ld] focus",win->wid);
    }
    else
      foreign_toplevel_activate(win->uid);
  }
}
