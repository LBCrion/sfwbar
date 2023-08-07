/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowitem.h"
#include "basewidget.h"
#include "flowgrid.h"

G_DEFINE_TYPE_WITH_CODE(FlowItem, flow_item, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE(FlowItem));

static gint flow_item_comp_parent ( gconstpointer p1, gconstpointer p2 )
{
  return p2 - p1;
}

static void flow_item_destroy ( GtkWidget *self )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));
  priv = flow_item_get_instance_private(FLOW_ITEM(self));

  flow_grid_delete_child(priv->parent,self);
  GTK_WIDGET_CLASS(flow_item_parent_class)->destroy(self);
}

static void flow_item_class_init ( FlowItemClass *kclass )
{
  FLOW_ITEM_CLASS(kclass)->comp_parent = flow_item_comp_parent;
  GTK_WIDGET_CLASS(kclass)->destroy = flow_item_destroy;
}

void flow_item_set_parent ( GtkWidget *self, GtkWidget *parent )
{
  FlowItemPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(self));

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  priv->parent = parent;
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

  g_return_val_if_fail(IS_FLOW_ITEM(self),FALSE);

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

void *flow_item_get_parent ( GtkWidget *self )
{
  g_return_val_if_fail(IS_FLOW_ITEM(self),NULL);

  if(FLOW_ITEM_GET_CLASS(self)->get_parent)
    return FLOW_ITEM_GET_CLASS(self)->get_parent(self);
  else
    return NULL;
}

gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent )
{
  g_return_val_if_fail(IS_FLOW_ITEM(p1),0);
  g_return_val_if_fail(IS_FLOW_ITEM(p2),0);

  if(!FLOW_ITEM_GET_CLASS(p1)->compare)
    return 0;

  return FLOW_ITEM_GET_CLASS(p1)->compare(p1,p2,parent);
}

