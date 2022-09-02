/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowitem.h"
#include "flowgrid.h"

G_DEFINE_TYPE_WITH_CODE(FlowItem, flow_item, GTK_TYPE_EVENT_BOX,
    G_ADD_PRIVATE(FlowItem));

static gint flow_item_comp_parent ( gconstpointer p1, gconstpointer p2 )
{
  return p2 - p1;
}

static void flow_item_class_init ( FlowItemClass *kclass )
{
  FLOW_ITEM_CLASS(kclass)->comp_parent = flow_item_comp_parent;
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

void flow_item_invalidate ( GtkWidget *self )
{
  g_return_if_fail(FLOW_IS_ITEM(self));

  if(FLOW_ITEM_GET_CLASS(self)->invalidate)
    FLOW_ITEM_GET_CLASS(self)->invalidate(self);
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

static void flow_item_dnd_data_rec_cb ( GtkWidget *widget, GdkDragContext *ctx,
    gint x, gint y, GtkSelectionData *sel, guint info, guint time,
    gpointer data )
{
  GtkWidget *src = *(GtkWidget **)gtk_selection_data_get_data(sel);

  if(src!=widget)
    flow_grid_reorder(data,src,widget);
}

static void flow_item_dnd_data_get_cb ( GtkWidget *widget, GdkDragContext *ctx,
    GtkSelectionData *sel, guint info, guint time, gpointer *data )
{
  gtk_selection_data_set(sel,gdk_atom_intern_static_string("gpointer"),
      8,(const guchar *)&data,sizeof(gpointer));
}

void flow_item_dnd_enable ( GtkWidget *self, GtkWidget *src, GtkWidget *parent )
{
  GtkTargetEntry *dnd_target;

  g_return_if_fail(FLOW_IS_ITEM(self));

  dnd_target = flow_grid_get_dnd_target(parent);
  if(!dnd_target)
    return;

  gtk_drag_source_set(src,GDK_BUTTON1_MASK,dnd_target,1,
      GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(src),"drag-data-get",
      G_CALLBACK(flow_item_dnd_data_get_cb),self);
  gtk_drag_dest_set(self,GTK_DEST_DEFAULT_ALL,dnd_target,1,
      GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(self),"drag-data-received",
      G_CALLBACK(flow_item_dnd_data_rec_cb),parent);
}
