/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "gui/bar.h"
#include "gui/css.h"
#include "gui/flowitem.h"
#include "gui/flowgrid.h"
#include "window.h"

G_DEFINE_TYPE_WITH_CODE(FlowItem, flow_item, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE(FlowItem))

static void flow_item_destroy ( GtkWidget *self )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));
  priv = flow_item_get_instance_private(FLOW_ITEM(self));

  if(priv->parent)  
    g_signal_handlers_disconnect_by_data(G_OBJECT(priv->parent), self);

  GTK_WIDGET_CLASS(flow_item_parent_class)->destroy(self);
}

static gboolean flow_item_drag_drop ( GtkWidget *self, GdkDragContext *ctx,
    gint x, gint y, guint time )
{
  GtkWidget *src;

  g_return_val_if_fail(IS_FLOW_ITEM(self), FALSE);

  if( (src = base_widget_get_parent(gtk_drag_get_source_widget(ctx))) &&
      src != self && IS_FLOW_ITEM(src) && 
      flow_item_get_parent(src) == flow_item_get_parent(self) )
  {
    flow_grid_children_order(flow_item_get_parent(self), self, src,
        !!g_list_find(g_list_find(
            flow_grid_children_get(flow_item_get_parent(self)), src), self));
    gtk_drag_finish(ctx, TRUE, FALSE, time);
    return TRUE;
  }

  return
    GTK_WIDGET_CLASS(flow_item_parent_class)->drag_drop(self, ctx, x, y, time);
}

static gboolean flow_item_drag_motion ( GtkWidget *self,  GdkDragContext *ctx,
    gint x, gint y, guint time )
{
  gboolean zone;
  GtkWidget *src;

  g_return_val_if_fail(IS_FLOW_ITEM(self), FALSE);

  zone = (src = base_widget_get_parent(gtk_drag_get_source_widget(ctx))) &&
      src != self && IS_FLOW_ITEM(src) &&
      flow_item_get_parent(src) == flow_item_get_parent(self);

  if(zone)
    css_add_class(self, "drop_target");
  else
    css_remove_class(self, "drop_target");

  return
    GTK_WIDGET_CLASS(flow_item_parent_class)->drag_motion(self, ctx, x, y, time)
    || zone;
}

static void flow_item_dnd_leave ( GtkWidget *self, GdkDragContext *ctx,
    guint time )
{
  css_remove_class(self, "drop_target");
}

static void flow_item_class_init ( FlowItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = flow_item_destroy;
  GTK_WIDGET_CLASS(kclass)->drag_drop = flow_item_drag_drop;
  GTK_WIDGET_CLASS(kclass)->drag_motion = flow_item_drag_motion;
  BASE_WIDGET_CLASS(kclass)->dnd_leave = flow_item_dnd_leave;
}

void flow_item_set_parent ( GtkWidget *self, GtkWidget *parent )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));
  priv = flow_item_get_instance_private(FLOW_ITEM(self));

  if(priv->parent)
    g_signal_handlers_disconnect_by_data(G_OBJECT(priv->parent), self);
  priv->parent = parent;

  g_clear_pointer(&priv->store_binding, g_binding_unbind);
  priv->store_binding = g_object_bind_property(G_OBJECT(parent), "store",
      G_OBJECT(self), "store", G_BINDING_SYNC_CREATE);
}

GtkWidget *flow_item_get_parent ( GtkWidget *self )
{
  FlowItemPrivate *priv;

  g_return_val_if_fail(IS_FLOW_ITEM(self), NULL);

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  return priv->parent;
}

void flow_item_set_active ( GtkWidget *self, gboolean active )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  priv->active = active;
}

gboolean flow_item_get_active ( GtkWidget *self )
{
  FlowItemPrivate *priv;

  g_return_val_if_fail(IS_FLOW_ITEM(self), FALSE);

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  return priv->active;
}

static void flow_item_init ( FlowItem *self )
{
  flow_item_set_active(GTK_WIDGET(self), TRUE);
}

void flow_item_update ( GtkWidget *self )
{
  g_return_if_fail(IS_FLOW_ITEM(self));

  if(FLOW_ITEM_GET_CLASS(self)->update)
    FLOW_ITEM_GET_CLASS(self)->update(self);
}

void flow_item_invalidate ( GtkWidget *self )
{
  if(!self)
    return;

  g_return_if_fail(IS_FLOW_ITEM(self));

  if(FLOW_ITEM_GET_CLASS(self)->invalidate)
    FLOW_ITEM_GET_CLASS(self)->invalidate(self);
}

void *flow_item_get_source ( GtkWidget *self )
{
  g_return_val_if_fail(IS_FLOW_ITEM(self), NULL);

  if(FLOW_ITEM_GET_CLASS(self)->get_source)
    return FLOW_ITEM_GET_CLASS(self)->get_source(self);
  else
    return NULL;
}

gint flow_item_check_source ( GtkWidget *self, gconstpointer source )
{
  g_return_val_if_fail(IS_FLOW_ITEM(self), 1);
  if(FLOW_ITEM_GET_CLASS(self)->comp_source)
    return FLOW_ITEM_GET_CLASS(self)->comp_source(
        flow_item_get_source(self), source);
  else
    return GPOINTER_TO_INT(flow_item_get_source(self) - source);
}

gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent )
{
  g_return_val_if_fail(IS_FLOW_ITEM(p1),0);
  g_return_val_if_fail(IS_FLOW_ITEM(p2),0);

  if(!FLOW_ITEM_GET_CLASS(p1)->compare)
    return 0;

  return FLOW_ITEM_GET_CLASS(p1)->compare(p1,p2,parent);
}

static void flow_item_dnd_begin_cb ( GtkWidget *widget, GdkDragContext *ctx,
    gpointer *d )
{
  cairo_surface_t *cursor;
  cairo_t *cr;

  cursor = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
      gtk_widget_get_allocated_width(widget),
      gtk_widget_get_allocated_height(widget));
  cr = cairo_create(cursor);
  gtk_widget_draw(widget, cr);
  gtk_drag_set_icon_surface(ctx, cursor);
  cairo_destroy(cr);
  cairo_surface_destroy(cursor);

  gtk_grab_add(widget);
  window_ref(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW), widget);
}

static void flow_item_dnd_end_cb ( GtkWidget *widget, GdkDragContext *ctx,
    gpointer d )
{
  gtk_grab_remove(widget);
  window_unref(widget, gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW));
}

void flow_item_dnd_enable ( GtkWidget *grid, GtkWidget *self, GtkWidget *src )
{
  GtkTargetEntry *target;

  g_return_if_fail(IS_FLOW_ITEM(self));

  if( !(target = flow_grid_get_dnd_target(grid)) )
    return;

  gtk_drag_dest_set(self, 0, target, 1, GDK_ACTION_MOVE);
  gtk_drag_dest_set_track_motion(self, TRUE);

  if(src)
  {
    gtk_drag_source_set(src, GDK_BUTTON1_MASK, target, 1, GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(src), "drag-begin",
        G_CALLBACK(flow_item_dnd_begin_cb), NULL);
    g_signal_connect(G_OBJECT(src), "drag-end",
        G_CALLBACK(flow_item_dnd_end_cb), NULL);
  }
}
