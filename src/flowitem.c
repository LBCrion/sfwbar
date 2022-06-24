/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"

G_DEFINE_TYPE_WITH_CODE(FlowItem, flow_item, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE(FlowItem));

static void flow_item_class_init ( FlowItemClass *kclass )
{
}

void flow_item_set_active ( GtkWidget *self, gboolean active )
{
  FlowItemPrivate *priv;

  g_return_if_fail(FLOW_IS_ITEM(self));

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  priv->active = active;
}

gboolean flow_item_get_active ( GtkWidget *self )
{
  FlowItemPrivate *priv;

  g_return_val_if_fail(FLOW_IS_ITEM(self),FALSE);

  priv = flow_item_get_instance_private(FLOW_ITEM(self));
  return priv->active;
}

static void flow_item_init ( FlowItem *self )
{
  flow_item_set_active(GTK_WIDGET(self),TRUE);
}

void flow_item_update ( GtkWidget *self )
{
  g_return_if_fail(FLOW_IS_ITEM(self));

  if(FLOW_ITEM_GET_CLASS(self)->update)
    FLOW_ITEM_GET_CLASS(self)->update(self);
}

void *flow_item_get_parent ( GtkWidget *self )
{
  g_return_val_if_fail(FLOW_IS_ITEM(self),NULL);

  if(FLOW_ITEM_GET_CLASS(self)->get_parent)
    return FLOW_ITEM_GET_CLASS(self)->get_parent(self);
  else
    return NULL;
}

gint flow_item_compare ( GtkWidget *p1, GtkWidget *p2, GtkWidget *parent )
{
  g_return_val_if_fail(FLOW_IS_ITEM(p1),0);
  g_return_val_if_fail(FLOW_IS_ITEM(p2),0);

  if(!FLOW_ITEM_GET_CLASS(p1)->compare)
    return 0;

  return FLOW_ITEM_GET_CLASS(p1)->compare(p1,p2,parent);
}
