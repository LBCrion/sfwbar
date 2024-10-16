/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "css.h"
#include "switcheritem.h"
#include "switcher.h"
#include "wintree.h"
#include "pager.h"
#include "config.h"
#include "bar.h"

G_DEFINE_TYPE_WITH_CODE (Switcher, switcher, FLOW_GRID_TYPE,
    G_ADD_PRIVATE (Switcher))

static GtkWidget *switcher_win;
static GtkWidget *switcher_grid;
static guint timer_handle;
static gint interval;
static gchar hstate;
static gint counter;
static window_t *focus;

GtkApplication *application;

static gboolean switcher_update ( GtkWidget *self )
{
  GList *iter;

  if(!switcher_grid)
    return TRUE;

  if(counter <= 0)
    return TRUE;
  counter--;

  if(counter > 0)
  {
    for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
      flow_item_invalidate(flow_grid_find_child(switcher_grid, iter->data));
    flow_grid_update(switcher_grid);
    css_widget_cascade(switcher_win, NULL);
  }
  else
  {
    gtk_widget_hide(switcher_win);
    wintree_focus(focus->uid);
  }
  return TRUE;
}

static void switcher_class_init ( SwitcherClass *kclass )
{
  g_unix_signal_add(SIGUSR1, (GSourceFunc)switcher_event, NULL);
}

void switcher_init_item (window_t *win, GtkWidget *self )
{
  flow_grid_add_child(self, switcher_item_new(win, self));
}

static void switcher_invalidate_item ( window_t *win, GtkWidget *self )
{
  flow_item_invalidate(flow_grid_find_child(self, win));
}

static void switcher_destroy_item( window_t *win, GtkWidget *self )
{
  flow_grid_delete_child(self, win);
}

static window_listener_t switcher_window_listener = {
  .window_new = (void (*)(window_t *, void *))switcher_init_item,
  .window_invalidate = (void (*)(window_t *, void *))switcher_invalidate_item,
  .window_destroy = (void (*)(window_t *, void *))switcher_destroy_item,
};

static void switcher_init ( Switcher *self )
{
  flow_grid_set_limit(GTK_WIDGET(self), FALSE);
  gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "switcher");
  wintree_listener_register(&switcher_window_listener, GTK_WIDGET(self));

  switcher_win = gtk_application_window_new(application);
  gtk_layer_init_for_window (GTK_WINDOW(switcher_win));
  gtk_layer_set_layer(GTK_WINDOW(switcher_win), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_widget_set_name(switcher_win, "switcher");
  gtk_container_add(GTK_CONTAINER(switcher_win), GTK_WIDGET(self));
  timer_handle = g_timeout_add(100, (GSourceFunc)switcher_update, self);

  hstate = 's';
}

gboolean switcher_state ( void )
{
  return !!switcher_grid;
}

GtkWidget *switcher_new ( void )
{
  return switcher_grid?switcher_grid:
    (switcher_grid = GTK_WIDGET(g_object_new(switcher_get_type(), NULL)));
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

  if(!switcher_grid)
    return TRUE;

  if(counter<1 || !focus)
    focus = wintree_from_id(wintree_get_focus());
  counter = interval + 1;

  for (iter = wintree_get_list(); iter; iter = g_list_next(iter) )
    if(switcher_check(switcher_grid, iter->data))
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

void switcher_set_interval ( gint new_interval )
{
  interval = new_interval;
}
