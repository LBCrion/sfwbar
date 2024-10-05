/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"
#include "switcheritem.h"
#include "switcher.h"
#include "wintree.h"
#include "pager.h"
#include "config.h"
#include "bar.h"

G_DEFINE_TYPE_WITH_CODE (Switcher, switcher, FLOW_GRID_TYPE,
    G_ADD_PRIVATE (Switcher))

static GtkWidget *switcher_win;
static GtkWidget *grid;
static gint interval;
static gchar hstate;
static gint counter;
static gint title_width = -1;
static window_t *focus;

GtkApplication *application;

static void switcher_class_init ( SwitcherClass *kclass )
{
}

static void switcher_init ( Switcher *self )
{
}

gboolean switcher_state ( void )
{
  return !!grid;
}

GtkWidget *switcher_new ( void )
{
  if(grid)
    return grid;

  grid = GTK_WIDGET(g_object_new(switcher_get_type(), NULL));
  flow_grid_set_limit(grid, FALSE);
  gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(grid)), "switcher");

  switcher_win = gtk_application_window_new(application);
  gtk_layer_init_for_window (GTK_WINDOW(switcher_win));
  gtk_layer_set_layer(GTK_WINDOW(switcher_win), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_widget_set_name(switcher_win, "switcher");
  gtk_container_add(GTK_CONTAINER(switcher_win), grid);

  hstate = 's';

  return grid;
}

void switcher_populate ( void )
{
  GList *iter;

  if(!grid)
    return;

  interval = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(grid),"interval"));
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(grid),"title_width"));

  if(!title_width)
    title_width = -1;

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    switcher_window_init(iter->data);
}

gboolean switcher_is_focused ( gpointer uid )
{
  return focus?uid==focus->uid:FALSE;
}

gboolean switcher_check ( GtkWidget *switcher, window_t *win )
{
  switch(switcher_get_filter(switcher))
  {
    case G_TOKEN_OUTPUT:
      return (!win->outputs || g_list_find_custom(win->outputs,
          bar_get_output(base_widget_get_child(switcher)),
          (GCompareFunc)g_strcmp0));
    case G_TOKEN_WORKSPACE:
      return (!win->workspace || win->workspace==workspace_get_focused());
  }

  return !wintree_is_filtered(win);
}

gboolean switcher_event ( gpointer dir )
{
  GList *iter, *list = NULL, *flink = NULL;

  if(!grid)
    return TRUE;

  if(counter<1 || !focus)
    focus = wintree_from_id(wintree_get_focus());
  counter = interval + 1;

  for (iter = wintree_get_list(); iter; iter = g_list_next(iter) )
    if(switcher_check(grid, iter->data))
      list = g_list_prepend(list, iter->data);
  list = g_list_reverse(list);

  flink = g_list_find(list, focus);
  if(!dir)
    flink = g_list_next(flink)?g_list_next(flink):list;
  else
    flink = g_list_previous(flink)?g_list_previous(flink):g_list_last(list);

  if(flink)
    focus = flink->data;

  g_list_free(list);

  return TRUE;
}

void switcher_window_delete (window_t *win )
{
  if(grid)
    flow_grid_delete_child(grid, win);
}

void switcher_window_init (window_t *win )
{
  if(grid)
    flow_grid_add_child(grid, switcher_item_new(win, grid));
}


void switcher_update ( void )
{
  GList *iter;

  if(!grid)
    return;

  if(counter <= 0)
    return;
  counter--;

  if(counter > 0)
  {
    for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
      flow_item_invalidate(flow_grid_find_child(grid, iter->data));
    flow_grid_update(grid);
    css_widget_cascade(switcher_win, NULL);
  }
  else
  {
    gtk_widget_hide(switcher_win);
    wintree_focus(focus->uid);
  }
}

void switcher_set_filter ( GtkWidget *self, gint filter )
{
  SwitcherPrivate *priv;

  g_return_if_fail(IS_SWITCHER(self));
  priv = switcher_get_instance_private(SWITCHER(self));

  priv->filter = filter;
}

gint switcher_get_filter ( GtkWidget *self )
{
  SwitcherPrivate *priv;

  g_return_val_if_fail(IS_SWITCHER(self),0);
  priv = switcher_get_instance_private(SWITCHER(self));

  return priv->filter;
}

void switcher_invalidate ( window_t *win )
{
  if(grid)
    flow_item_invalidate(flow_grid_find_child(grid, win));
}
