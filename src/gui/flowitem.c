/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowitem.h"
#include "basewidget.h"
#include "flowgrid.h"

G_DEFINE_TYPE_WITH_CODE(FlowItem, flow_item, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE(FlowItem))

static void flow_item_destroy ( GtkWidget *self )
{
//  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));
//  priv = flow_item_get_instance_private(FLOW_ITEM(self));

//  flow_grid_delete_child(priv->parent,self);
  GTK_WIDGET_CLASS(flow_item_parent_class)->destroy(self);
}

static void flow_item_dnd_dest_impl ( GtkWidget *dest, GtkWidget *src,
    gint x, gint y )
{
  FlowItemPrivate *priv, *spriv;
  window_t *swin, *dwin;
  workspace_t *ws;
  GtkAllocation alloc;
  gboolean after;

  if(src==dest)
    return;

  g_return_if_fail(IS_FLOW_ITEM(dest));
  g_return_if_fail(IS_FLOW_ITEM(src));
  priv = flow_item_get_instance_private(FLOW_ITEM(dest));
  spriv = flow_item_get_instance_private(FLOW_ITEM(src));

  if(priv->parent == spriv->parent)
  {
    gtk_widget_get_allocation( dest, &alloc );
    after = ((flow_grid_get_cols(priv->parent)>0 && y>alloc.height/2) ||
        (flow_grid_get_rows(priv->parent)>0 && x>alloc.width/2));
    flow_grid_children_order(priv->parent, dest, src, after);
  }
  else
  {
    swin = flow_item_get_source(src);
    dwin = flow_item_get_source(dest);
    ws = workspace_from_id(dwin->workspace);
    if(swin && ws)
      wintree_move_to(swin->uid, ws->id);
  }
}

static void flow_item_class_init ( FlowItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = flow_item_destroy;
  FLOW_ITEM_CLASS(kclass)->dnd_dest = flow_item_dnd_dest_impl;
}

void flow_item_set_parent ( GtkWidget *self, GtkWidget *parent )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  priv->parent = parent;
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
  flow_item_set_active(GTK_WIDGET(self),TRUE);
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
  g_return_val_if_fail(IS_FLOW_ITEM(self),NULL);

  if(FLOW_ITEM_GET_CLASS(self)->get_source)
    return FLOW_ITEM_GET_CLASS(self)->get_source(self);
  else
    return NULL;
}

void flow_item_decorate ( GtkWidget *self, gboolean labels, gboolean icons )
{
  g_return_if_fail(IS_FLOW_ITEM(self));
  if(FLOW_ITEM_GET_CLASS(self)->decorate)
    FLOW_ITEM_GET_CLASS(self)->decorate(self, labels, icons);
}

void flow_item_set_title_width ( GtkWidget *self, gint title_width )
{
  g_return_if_fail(IS_FLOW_ITEM(self));
  if(FLOW_ITEM_GET_CLASS(self)->set_title_width)
    FLOW_ITEM_GET_CLASS(self)->set_title_width(self, title_width);
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

void flow_item_dnd_dest ( GtkWidget *self, GtkWidget *src, gint x, gint y )
{
  g_return_if_fail(IS_FLOW_ITEM(self));
  if(FLOW_ITEM_GET_CLASS(self)->dnd_dest)
    FLOW_ITEM_GET_CLASS(self)->dnd_dest(self, src, x, y);
}
