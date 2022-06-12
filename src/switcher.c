/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 Lev Babiev
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
static gint title_width = -1;
static gboolean icons, labels;
static GList *focus;

void switcher_init ( void )
{
  if(!grid)
    return;

  g_object_ref(grid);
  if(switcher)
  {
    gtk_container_remove(GTK_CONTAINER(switcher),grid);
    gtk_window_close(GTK_WINDOW(switcher));
  }

  switcher = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW(switcher));
  gtk_layer_set_layer(GTK_WINDOW(switcher),GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_widget_set_name(switcher, "switcher");
  gtk_container_add(GTK_CONTAINER(switcher),grid);
  g_object_unref(grid);
}

void switcher_config ( gint ncols, gchar *css, gint nmax,
    gboolean nicons, gboolean nlabels, gint twidth )
{
  GList *iter;

  if(!switcher)
  {
    grid = flow_grid_new(FALSE,(GCompareFunc)switcher_item_compare);
    gtk_widget_set_name(grid, "switcher");
    switcher_init();
  }
  flow_grid_set_cols(grid,ncols);
  if(css!=NULL)
  {
    GtkStyleContext *cont = gtk_widget_get_style_context (grid);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,css,strlen(css),NULL);
    gtk_style_context_add_provider (cont,
      GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
    g_free(css);
  }
  interval = nmax;
  hstate = 's';
  icons = nicons;
  labels = nlabels;
  title_width = twidth;
  g_object_set_data(G_OBJECT(grid),"title_width",
      GINT_TO_POINTER(title_width));
  g_object_set_data(G_OBJECT(grid),"icons", GINT_TO_POINTER(icons));
  g_object_set_data(G_OBJECT(grid),"labels", GINT_TO_POINTER(labels));
  if(!icons)
    labels = TRUE;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    switcher_window_init(iter->data);
}

void switcher_invalidate ( void )
{
  if(grid)
    flow_grid_invalidate(grid);
}

gboolean switcher_event ( struct json_object *obj )
{
  gchar *state,*id;
  GList *item;
  gboolean event = FALSE;

  if(!switcher)
    return TRUE;

  if(obj)
  {
    state = json_string_by_name(obj,"hidden_state");
    if(state)
    {
      if(*state!='h')
      {
        event=TRUE;
        id = json_string_by_name(obj,"id");
        if(id)
          sway_ipc_command("bar %s hidden_state hide",id);
        g_free(id);
      }
      g_free(state);
    }
  }
  else
    event = TRUE;

  if(event)
  {
    counter = interval;
    focus = NULL;
    for (item = wintree_get_list(); item; item = g_list_next(item) )
      if ( wintree_is_focused(((window_t *)item->data)->uid) )
        focus = g_list_next(item);
    if(focus==NULL)
      focus=wintree_get_list();
    if(focus!=NULL)
      wintree_set_focus(((window_t *)focus->data)->uid);
    switcher_invalidate();
  }

  return TRUE;
}

void switcher_window_init ( window_t *win)
{
  if(!grid)
    return;
  flow_grid_add_child(grid,switcher_item_new(win,grid));
}

void switcher_update ( void )
{
  window_t *win;

  if(!switcher)
    return;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    flow_grid_update(grid);
    if(!gtk_widget_is_visible(switcher))
      switcher_init();
    gtk_widget_show_all(switcher);
    widget_set_css(switcher,NULL);
  }
  else
  {
    gtk_widget_hide(switcher);
    win = focus->data;
    wintree_focus(win->uid);
  }
}
