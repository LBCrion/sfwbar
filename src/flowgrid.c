/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "basewidget.h"
#include "taskbargroup.h"

G_DEFINE_TYPE_WITH_CODE (FlowGrid, flow_grid, GTK_TYPE_GRID, G_ADD_PRIVATE (FlowGrid));

static void flow_grid_get_preferred_width (GtkWidget *widget, gint *minimal, gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  priv = flow_grid_get_instance_private(FLOW_GRID(widget));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_width(widget,minimal,natural);

  if(priv->rows>0 && priv->limit )
    *minimal = MIN(*natural,1);
}

static void flow_grid_get_preferred_height (GtkWidget *widget, gint *minimal, gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  priv = flow_grid_get_instance_private(FLOW_GRID(widget));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_height(widget,minimal,natural);

  if(priv->cols>0 && priv->limit)
    *minimal = MIN(*natural,1);
}

static void flow_grid_destroy( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  g_clear_pointer(&priv->dnd_target,gtk_target_entry_free);
  GTK_WIDGET_CLASS(flow_grid_parent_class)->destroy(self);
}

static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
  widget_class->destroy = flow_grid_destroy;
}

static void flow_grid_init ( FlowGrid *self )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->rows = 1;
  priv->cols = 0;
  priv->limit = TRUE;
  priv->dnd_target = gtk_target_entry_new(
      g_strdup_printf("flow-item-%p",self),0,1);

  gtk_grid_set_row_homogeneous(GTK_GRID(self),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(self),TRUE);
}

GtkWidget *flow_grid_new( gboolean limit)
{
  GtkWidget *w;
  FlowGridPrivate *priv;

  w = GTK_WIDGET(g_object_new(flow_grid_get_type(), NULL));
  priv = flow_grid_get_instance_private(FLOW_GRID(w));

  priv->limit = limit;
  
  return w;
}

void flow_grid_set_cols ( GtkWidget *cgrid, gint cols )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->cols = cols;
  priv->rows = 0;
  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
}

void flow_grid_set_rows ( GtkWidget *cgrid, gint rows )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->rows = rows;
  priv->cols = 0;

  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
}

void flow_grid_attach ( GtkWidget *cgrid, GtkWidget *w )
{
  FlowGridPrivate *priv;
  GList *children;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  if(!flow_item_get_active(w))
    return;

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  children = gtk_container_get_children(GTK_CONTAINER(cgrid));
  if(!children)
    priv->i=0;
  g_list_free(children);

  if(priv->rows>0)
    gtk_grid_attach(GTK_GRID(cgrid),w,
        (priv->i)/(priv->rows),(priv->i)%(priv->rows),1,1);
  else
    gtk_grid_attach(GTK_GRID(cgrid),w,
        (priv->i)%(priv->cols),(priv->i)/(priv->cols),1,1);
  priv->i++;
}

void flow_grid_pad ( GtkWidget *cgrid )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));
 
  if(priv->rows>0)
    for(;priv->i<priv->rows;priv->i++)
      gtk_grid_attach(GTK_GRID(cgrid),gtk_label_new(""),0,priv->i,1,1);
  else
    for(;priv->i<priv->cols;priv->i++)
      gtk_grid_attach(GTK_GRID(cgrid),gtk_label_new(""),priv->i,0,1,1);
}

void flow_grid_remove_widget ( GtkWidget *widget, GtkWidget *parent )
{
  gtk_container_remove ( GTK_CONTAINER(parent), widget );
}

void flow_grid_clean ( GtkWidget *cgrid )
{
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  gtk_container_foreach(GTK_CONTAINER(cgrid),
      (GtkCallback)flow_grid_remove_widget,cgrid);
}

GtkTargetEntry *flow_grid_get_dnd_target ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self),NULL);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->dnd_target;
}

void flow_grid_invalidate ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->invalid = TRUE;
}

void flow_grid_add_child ( GtkWidget *self, GtkWidget *child )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->children = g_list_append(priv->children,child);
  flow_grid_invalidate(self);
}

void flow_grid_delete_child ( GtkWidget *self, void *parent )
{
  FlowGridPrivate *priv;
  GList *iter;
  GCompareFunc comp_parent;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  if(!priv->children || !priv->children->data ||
      !FLOW_ITEM_GET_CLASS(priv->children->data)->comp_parent)
    return;
  comp_parent = FLOW_ITEM_GET_CLASS(priv->children->data)->comp_parent;

  for(iter=priv->children;iter;iter=g_list_next(iter))
    if(!comp_parent(flow_item_get_parent(iter->data),parent))
    {
      gtk_widget_destroy(iter->data);
      priv->children = g_list_delete_link(priv->children,iter);
      break;
    }
  flow_grid_invalidate(self);
}

void flow_grid_update ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->invalid)
    return;
  priv->invalid = FALSE;

  flow_grid_clean(self);
  priv->children = g_list_sort_with_data(priv->children,
      (GCompareDataFunc)flow_item_compare,self);
  for(iter=priv->children;iter;iter=g_list_next(iter))
  {
    flow_item_update(iter->data);
    flow_grid_attach(self,iter->data);
  }
  flow_grid_pad(self);
  css_widget_cascade(self,NULL);
}

guint flow_grid_n_children ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self),0);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return g_list_length(priv->children);
}

gpointer flow_grid_find_child ( GtkWidget *self, gconstpointer parent )
{
  FlowGridPrivate *priv;
  GList *iter;
  GCompareFunc comp_parent;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self),NULL);

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->children || !priv->children->data ||
      !FLOW_ITEM_GET_CLASS(priv->children->data)->comp_parent)
    return NULL;
  comp_parent = FLOW_ITEM_GET_CLASS(priv->children->data)->comp_parent;

  for(iter=priv->children;iter;iter=g_list_next(iter))
    if(!comp_parent(flow_item_get_parent(iter->data),parent))
      return iter->data;

  return NULL;
}

void flow_grid_reorder ( GtkWidget *self, GtkWidget *src, GtkWidget *dest )
{
  FlowGridPrivate *priv;
  GList *dlist;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  dlist = g_list_find(priv->children,dest);
  if(!dlist)
    return;

  priv->children = g_list_remove(priv->children, src );
  priv->children = g_list_insert_before(priv->children, dlist, src );
  flow_item_invalidate(dest);
}
