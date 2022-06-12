/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2021 Lev Babiev
 */

#include "sfwbar.h"

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


static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
}

static void flow_grid_init ( FlowGrid *cgrid )
{
  FlowGridPrivate *priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->rows = 1;
  priv->cols = 0;
  priv->limit = TRUE;

  gtk_grid_set_row_homogeneous(GTK_GRID(cgrid),TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(cgrid),TRUE);
}

GtkWidget *flow_grid_new( gboolean limit, GCompareFunc comp_f )
{
  GtkWidget *w;
  FlowGridPrivate *priv;

  w = GTK_WIDGET(g_object_new(flow_grid_get_type(), NULL));
  priv = flow_grid_get_instance_private(FLOW_GRID(w));

  priv->limit = limit;
  priv->comp = comp_f;
  
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
  if((priv->rows>0)&&(priv->cols>0))
    priv->cols = -1;
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
  if((priv->rows>0)&&(priv->cols>0))
    priv->cols = -1;
}

void flow_grid_attach ( GtkWidget *cgrid, GtkWidget *w )
{
  FlowGridPrivate *priv;
  GList *children;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

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
  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));

  gtk_container_foreach(GTK_CONTAINER(cgrid),
      (GtkCallback)flow_grid_remove_widget,cgrid);
}

void flow_grid_invalidate ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->invalid = TRUE;
}

void flow_grid_add_child ( GtkWidget *self, GtkWidget *child )
{
  FlowGridPrivate *priv;
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->children = g_list_insert_sorted(priv->children,child,priv->comp);
}

void flow_grid_delete_child ( GtkWidget *self, void *parent )
{
  FlowGridPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  for(iter=priv->children;iter;iter=g_list_next(iter))
    if(flow_item_get_parent(iter->data)==parent)
    {
      gtk_widget_destroy(iter->data);
      priv->children = g_list_remove_link(priv->children,iter);
    }
}

void flow_grid_update ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->invalid)
    return;
  priv->invalid = FALSE;

  flow_grid_clean(self);
//  if(priv->comp)
//    priv->children = g_list_sort(priv->children,priv->comp);
  for(iter=priv->children;iter;iter=g_list_next(iter))
  {
    flow_item_update(iter->data);
    flow_grid_attach(self,iter->data);
  }
  flow_grid_pad(self);
}
