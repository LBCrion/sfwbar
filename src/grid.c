/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "grid.h"

G_DEFINE_TYPE_WITH_CODE (Grid, grid, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Grid));

static void grid_destroy ( GtkWidget *self )
{
}

static GtkWidget *grid_get_child ( GtkWidget *self )
{
  GridPrivate *priv;

  g_return_val_if_fail(IS_GRID(self),NULL);
  priv = grid_get_instance_private(GRID(self));

  return priv->grid;
}

static void grid_child_park ( GtkWidget *widget, GtkWidget *grid )
{
  g_object_ref(widget);
  gtk_container_remove(GTK_CONTAINER(grid),widget);
}

static void grid_style_updated ( GtkWidget *grid, GtkWidget *self )
{
  GridPrivate *priv;
  gint dir;
  GList *iter;

  priv = grid_get_instance_private(GRID(self));
  gtk_widget_style_get(priv->grid,"direction",&dir,NULL);
  if(priv->dir == dir)
    return;
  priv->dir = dir;

  gtk_container_foreach(GTK_CONTAINER(priv->grid),
      (GtkCallback)grid_child_park,priv->grid);

  priv->last = NULL;
  for(iter=priv->children;iter;iter=g_list_next(iter))
    grid_attach(self,iter->data);
}

static void grid_class_init ( GridClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = grid_destroy;
  BASE_WIDGET_CLASS(kclass)->get_child = grid_get_child;
}

static void grid_init ( Grid *self )
{
}

GtkWidget *grid_new ( void )
{
  GtkWidget *self;
  GridPrivate *priv;

  self = GTK_WIDGET(g_object_new(grid_get_type(), NULL));
  priv = grid_get_instance_private(GRID(self));

  priv->grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(self),priv->grid);
  g_signal_connect(G_OBJECT(priv->grid),"style_updated",
      (GCallback)grid_style_updated,self);

  return self;
}

void grid_attach ( GtkWidget *self, GtkWidget *child )
{
  GridPrivate *priv;

  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));

  if(!g_list_find(priv->children,child))
    priv->children = g_list_append(priv->children,child);

  base_widget_attach(priv->grid,child,priv->last);
  priv->last = child;
}
