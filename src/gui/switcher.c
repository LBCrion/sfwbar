/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "trigger.h"
#include "wintree.h"
#include "config/config.h"
#include "gui/bar.h"
#include "gui/css.h"
#include "gui/filter.h"
#include "gui/pager.h"
#include "gui/switcher.h"
#include "gui/switcheritem.h"
#include "gui/taskbarshell.h"

G_DEFINE_TYPE_WITH_CODE (Switcher, switcher, FLOW_GRID_TYPE,
    G_ADD_PRIVATE (Switcher))

enum {
  SWITCHER_FILTER = 1,
};

static GtkWidget *switcher_win;
static GtkWidget *switcher_grid;
static guint timer_handle;
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
    if(focus)
      wintree_focus(focus->uid);
  }
  return TRUE;
}

static void switcher_init_item (window_t *win, GtkWidget *self )
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

static void switcher_destroy ( GtkWidget *self )
{
  if(switcher_grid == self)
  {
    switcher_grid = NULL;
    wintree_listener_remove(self);
  }
  GTK_WIDGET_CLASS(switcher_parent_class)->destroy(self);
}

static void switcher_get_property ( GObject *self, guint id,
    GValue *value, GParamSpec *spec )
{
  SwitcherPrivate *priv;

  priv = switcher_get_instance_private(SWITCHER(self));
  if(id == SWITCHER_FILTER)
    g_value_set_enum(value, priv->filter);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
}

static void switcher_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  SwitcherPrivate *priv;

  priv = switcher_get_instance_private(SWITCHER(self));
  if(id == SWITCHER_FILTER)
    priv->filter |= g_value_get_enum(value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
}

static gboolean switcher_event ( gpointer dir )
{
  GList *iter, *list = NULL, *flink = NULL;
  gint64 interval;

  if(!switcher_grid)
    return TRUE;

  if(counter<1 || !focus)
    focus = wintree_from_id(wintree_get_focus());
  g_object_get(G_OBJECT(switcher_grid), "interval", &interval, NULL);
  counter = interval/100 + 1;

  for (iter = wintree_get_list(); iter; iter = g_list_next(iter) )
    if(filter_window_check(switcher_grid, iter->data))
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

static void switcher_class_init ( SwitcherClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = switcher_destroy;
  G_OBJECT_CLASS(kclass)->get_property = switcher_get_property;
  G_OBJECT_CLASS(kclass)->set_property = switcher_set_property;

  g_unix_signal_add(SIGUSR1, (GSourceFunc)switcher_event, NULL);
  trigger_add("switcher_forward", (trigger_func_t)switcher_event, NULL);
  trigger_add("switcher_back", (trigger_func_t)switcher_event, (void *)1);
  FLOW_GRID_CLASS(kclass)->limit = FALSE;
  g_object_class_install_property(G_OBJECT_CLASS(kclass), SWITCHER_FILTER,
      g_param_spec_enum("filter", "filter", "sfwbar_config", filter_type_get(),
        0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void switcher_init ( Switcher *self )
{
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

GtkWidget *switcher_new ( void )
{
  return switcher_grid?switcher_grid:
    (switcher_grid = GTK_WIDGET(g_object_new(switcher_get_type(), NULL)));
}

gboolean switcher_is_focused ( gpointer uid )
{
  return focus?uid==focus->uid:FALSE;
}
