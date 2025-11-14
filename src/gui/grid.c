/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "basewidget.h"
#include "grid.h"
#include "taskbarshell.h"

G_DEFINE_TYPE_WITH_CODE (Grid, grid, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Grid))

enum {
  GRID_HSCROLL = 1,
  GRID_VSCROLL,
};

static void grid_destroy ( GtkWidget *self )
{
  GridPrivate *priv;

  g_return_if_fail(g_main_context_is_owner(g_main_context_default()));
  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));

  g_list_free(g_steal_pointer(&priv->last));
  while(priv->children)
    grid_detach(priv->children->data, self);
  if(priv->grid)
    g_signal_handlers_disconnect_by_data(G_OBJECT(priv->grid), self);
  priv->grid = NULL;
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

static void grid_style_updated ( GtkWidget *grid, GtkWidget *self )
{
  GridPrivate *priv;
  gint dir;
  GList *iter;

  g_return_if_fail(g_main_context_is_owner(g_main_context_default()));
  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));
  g_return_if_fail(GTK_IS_GRID(priv->grid));
  gtk_widget_style_get(priv->grid, "direction", &dir, NULL);
  if(priv->dir == dir)
    return;
  priv->dir = dir;

  g_list_free(g_steal_pointer(&priv->last));
  for(iter=priv->children; iter; iter=g_list_next(iter))
  {
    g_object_ref(iter->data);
    if(gtk_widget_get_parent(iter->data)==priv->grid)
      gtk_container_remove(GTK_CONTAINER(priv->grid), iter->data);
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

static void grid_update_scroller ( GtkWidget *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));

  if((priv->hscroll || priv->vscroll) && !priv->scroller)
  {
    priv->scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(priv->scroller), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->scroller),
        priv->hscroll? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER,
        priv->vscroll? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
    g_object_ref(priv->grid);
    gtk_container_remove(GTK_CONTAINER(self), priv->grid);
    gtk_container_add(GTK_CONTAINER(priv->scroller), priv->grid);
    gtk_container_add(GTK_CONTAINER(self), priv->scroller);

    g_object_unref(priv->grid);
  }

  else if(!priv->hscroll && !priv->vscroll && priv->scroller)
  {
    g_object_ref(priv->grid);
    gtk_container_remove(GTK_CONTAINER(priv->scroller), priv->grid);
    g_clear_pointer(&priv->scroller, gtk_widget_destroy);
    gtk_container_add(GTK_CONTAINER(self), priv->grid);
    g_object_unref(priv->grid);
  }
}

static void grid_get_property ( GObject *self, guint id, GValue *value,
    GParamSpec *spec )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));

  switch(id)
  {
    case GRID_HSCROLL:
      return g_value_set_boolean(value, priv->hscroll);
      break;
    case GRID_VSCROLL:
      return g_value_set_boolean(value, priv->hscroll);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void grid_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));
  switch(id)
  {
    case GRID_HSCROLL:
      priv->hscroll = g_value_get_boolean(value);
      grid_update_scroller(GTK_WIDGET(self));
      break;
    case GRID_VSCROLL:
      priv->vscroll = g_value_get_boolean(value);
      grid_update_scroller(GTK_WIDGET(self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void grid_get_preferred_width (GtkWidget *self, gint *minimal,
    gint *natural)
{
  GridPrivate *priv;
  GtkStyleContext *style;
  GtkStateFlags flags;
  gint limit;

  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));

  GTK_WIDGET_CLASS(grid_parent_class)->get_preferred_width(
      self, minimal, natural);

  if(priv->hscroll)
  {
    style = gtk_widget_get_style_context(self);
    flags = gtk_style_context_get_state(style);
    gtk_style_context_get(style, flags, "min-width", &limit, NULL);

    *natural = MIN(*natural, limit);
    *minimal = MIN(*minimal, limit);
  }
}

static void grid_get_preferred_height (GtkWidget *self, gint *minimal,
    gint *natural)
{
  GridPrivate *priv;
  GtkStyleContext *style;
  GtkStateFlags flags;
  gint limit;

  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));

  GTK_WIDGET_CLASS(grid_parent_class)->get_preferred_height(
      self, minimal, natural);

  if(priv->vscroll)
  {
    style = gtk_widget_get_style_context(self);
    flags = gtk_style_context_get_state(style);
    gtk_style_context_get(style, flags, "min-height", &limit, NULL);

    *natural = MIN(*minimal, limit);
    *minimal = MIN(*minimal, limit);
  }
}

static void grid_class_init ( GridClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = grid_destroy;
  GTK_WIDGET_CLASS(kclass)->get_preferred_height = grid_get_preferred_height;
  GTK_WIDGET_CLASS(kclass)->get_preferred_width = grid_get_preferred_width;
  BASE_WIDGET_CLASS(kclass)->mirror = grid_mirror;
  G_OBJECT_CLASS(kclass)->get_property = grid_get_property;
  G_OBJECT_CLASS(kclass)->set_property = grid_set_property;

  g_object_class_install_property(G_OBJECT_CLASS(kclass), GRID_HSCROLL,
      g_param_spec_boolean("hscroll", "horizonal scrollbar", "sfwbar_config",
        FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), GRID_VSCROLL,
      g_param_spec_boolean("vscroll", "vertical scrollbar", "sfwbar_config",
        FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void grid_init ( Grid *self )
{
  GridPrivate *priv;

  priv = grid_get_instance_private(GRID(self));

  priv->grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(self), priv->grid);
  g_signal_connect(G_OBJECT(priv->grid), "style_updated",
      (GCallback)grid_style_updated, self);
  g_signal_connect(G_OBJECT(priv->grid), "remove",
      (GCallback)grid_remove, self);
}

GtkWidget *grid_new ( void )
{
  return GTK_WIDGET(g_object_new(grid_get_type(), NULL));
}

void grid_detach( GtkWidget *child, GtkWidget *self )
{
  GridPrivate *priv;

  g_return_if_fail(g_main_context_is_owner(g_main_context_default()));
  g_return_if_fail(IS_GRID(self));
  priv = grid_get_instance_private(GRID(self));
  g_signal_handlers_disconnect_by_func(G_OBJECT(child), (GCallback)grid_detach,
      self);
  priv->children = g_list_remove(priv->children, child);
  priv->last = g_list_remove(priv->last, child);
}

gboolean grid_attach ( GtkWidget *self, GtkWidget *child )
{
  GridPrivate *priv;
  GdkRectangle *rect;
  gint dir;

  g_return_val_if_fail(g_main_context_is_owner(g_main_context_default()), TRUE);
  g_return_val_if_fail(IS_GRID(self), FALSE);
  g_return_val_if_fail(IS_BASE_WIDGET(child), FALSE);

  priv = grid_get_instance_private(GRID(self));

  g_object_get(G_OBJECT(child), "loc", &rect, NULL);
  gtk_widget_style_get(priv->grid, "direction", &dir, NULL);
  if(rect->x < 0 || rect->y < 0)
    gtk_grid_attach_next_to(GTK_GRID(priv->grid), child,
        priv->last? priv->last->data : NULL, dir, 1, 1);
  else
    gtk_grid_attach(GTK_GRID(priv->grid), child,
        rect->x, rect->y, rect->width, rect->height);
  g_free(rect);

  if(!g_list_find(priv->last, child))
    priv->last = g_list_prepend(priv->last, child);
  if(!g_list_find(priv->children, child))
  {
    priv->children = g_list_append(priv->children, child);
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
