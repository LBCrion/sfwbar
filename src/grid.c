/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "grid.h"
#include "taskbarshell.h"

G_DEFINE_TYPE_WITH_CODE (Grid, grid, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Grid))

static void grid_destroy ( GtkWidget *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));
  g_list_free(g_steal_pointer(&priv->children));
  g_list_free(g_steal_pointer(&priv->last));
  GTK_WIDGET_CLASS(grid_parent_class)->destroy(self);
}

static void grid_mirror ( GtkWidget *self, GtkWidget *src )
{
  GridPrivate *priv;
  GList *iter;

  BASE_WIDGET_CLASS(grid_parent_class)->mirror(self, src);
  priv = grid_get_instance_private(GRID(src));

  for(iter=priv->children; iter; iter=g_list_next(iter))
    grid_attach(self, base_widget_mirror(iter->data));
}

static void grid_child_park ( GtkWidget *widget, GtkWidget *grid )
{
  g_object_ref(widget);
  gtk_container_remove(GTK_CONTAINER(grid), widget);
}

static void grid_style_updated ( GtkWidget *grid, GtkWidget *self )
{
  GridPrivate *priv;
  gint dir;
  GList *iter;

  priv = grid_get_instance_private(GRID(self));
  gtk_widget_style_get(priv->grid, "direction", &dir, NULL);
  if(priv->dir == dir)
    return;
  priv->dir = dir;

  gtk_container_foreach(GTK_CONTAINER(priv->grid),
      (GtkCallback)grid_child_park, priv->grid);

  g_list_free(g_steal_pointer(&priv->last));
  for(iter=priv->children; iter; iter=g_list_next(iter))
  {
    grid_attach(self, iter->data);
    g_object_unref(iter->data);
  }
}

static void grid_remove ( GtkWidget *grid, GtkWidget *child, GtkWidget *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));
  priv->last = g_list_remove(priv->last, child);
}

static void grid_class_init ( GridClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = grid_destroy;
  BASE_WIDGET_CLASS(kclass)->mirror = grid_mirror;
}

static void grid_init ( Grid *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));

  priv->grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(self),priv->grid);
  g_signal_connect(G_OBJECT(priv->grid),"style_updated",
      (GCallback)grid_style_updated,self);
  g_signal_connect(G_OBJECT(priv->grid),"remove",
      (GCallback)grid_remove,self);
}

GtkWidget *grid_new ( void )
{
  return GTK_WIDGET(g_object_new(grid_get_type(), NULL));
}

void grid_detach( GtkWidget *child, GtkWidget *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));
  g_signal_handlers_disconnect_by_func(G_OBJECT(child), (GCallback)grid_detach,
      self);
  priv->children = g_list_remove(priv->children, child);
  priv->last = g_list_remove(priv->last, child);
}

gboolean grid_attach ( GtkWidget *self, GtkWidget *child )
{
  GridPrivate *priv;

  g_return_val_if_fail(IS_GRID(self), FALSE);
  g_return_val_if_fail(IS_BASE_WIDGET(child), FALSE);

  priv = grid_get_instance_private(GRID(self));

  base_widget_attach(priv->grid, child, priv->last?priv->last->data:NULL);

  if(!g_list_find(priv->children, child))
  {
    priv->children = g_list_append(priv->children,child);
    priv->last = g_list_prepend(priv->last, child);
    g_signal_connect(G_OBJECT(child), "destroy", (GCallback)grid_detach, self);
  }

  return TRUE;
}

void grid_mirror_child ( GtkWidget *self, GtkWidget *child )
{
  GList *iter;

  g_return_if_fail(IS_GRID(self));
  g_return_if_fail(IS_BASE_WIDGET(child));

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    grid_attach(iter->data, base_widget_mirror(child));
}
