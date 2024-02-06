/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "basewidget.h"
#include "taskbarpopup.h"
#include "config.h"
#include "window.h"
#include "bar.h"

G_DEFINE_TYPE_WITH_CODE (FlowGrid, flow_grid, GTK_TYPE_GRID,
    G_ADD_PRIVATE(FlowGrid))

static void flow_grid_get_preferred_width (GtkWidget *widget, gint *minimal,
    gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  priv = flow_grid_get_instance_private(FLOW_GRID(widget));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_width(
      widget, minimal, natural);

  if(priv->rows>0 && priv->limit )
    *minimal = MIN(*natural, 1);
}

static void flow_grid_get_preferred_height (GtkWidget *widget, gint *minimal,
    gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(IS_FLOW_GRID(widget));

  priv = flow_grid_get_instance_private(FLOW_GRID(widget));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_height(
      widget, minimal, natural);

  if(priv->cols>0 && priv->limit)
    *minimal = MIN(*natural, 1);
}

static void flow_grid_destroy( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  g_clear_pointer(&priv->dnd_target, gtk_target_entry_free);
  g_list_free_full(g_steal_pointer(&priv->children),
      (GDestroyNotify)gtk_widget_destroy);
  GTK_WIDGET_CLASS(flow_grid_parent_class)->destroy(self);
}

static void style_updated ( GtkWidget *self )
{
  gboolean h;

  gtk_widget_style_get(self,"row-homogeneous",&h,NULL);
  gtk_grid_set_row_homogeneous(GTK_GRID(self), h);
  gtk_widget_style_get(self,"column-homogeneous",&h,NULL);
  gtk_grid_set_column_homogeneous(GTK_GRID(self), h);

  GTK_WIDGET_CLASS(flow_grid_parent_class)->style_updated(self);
}

static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
  widget_class->destroy = flow_grid_destroy;
  widget_class->style_updated = style_updated;
  gtk_widget_class_install_style_property( widget_class,
      g_param_spec_boolean("row-homogeneous","row homogeneous",
        "make all rows within the grid equal height", TRUE, G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
      g_param_spec_boolean("column-homogeneous","column homogeneous",
        "make all columns within the grid equal width", TRUE,
        G_PARAM_READABLE));
}

static void flow_grid_init ( FlowGrid *self )
{
  FlowGridPrivate *priv;
  gchar *sig;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->rows = 1;
  priv->cols = 0;
  priv->limit = TRUE;
  sig = g_strdup_printf("flow-item-%p", (void *)self);
  priv->dnd_target = gtk_target_entry_new(sig, 0, SFWB_DND_TARGET_FLOW_ITEM);
  g_free(sig);

  gtk_grid_set_row_homogeneous(GTK_GRID(self), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(self), TRUE);
}

GtkWidget *flow_grid_new( gboolean limit)
{
  GtkWidget *w;
  FlowGridPrivate *priv;

  w = GTK_WIDGET(g_object_new(flow_grid_get_type(), NULL));
  priv = flow_grid_get_instance_private(FLOW_GRID(w));
  flow_grid_set_sort(w, TRUE);

  priv->limit = limit;
  
  return w;
}

void flow_grid_set_dnd_target ( GtkWidget *self, GtkTargetEntry *target )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  g_clear_pointer(&priv->dnd_target, gtk_target_entry_free);
  if(target)
    priv->dnd_target = gtk_target_entry_copy(target);
}

GtkTargetEntry *flow_grid_get_dnd_target ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self), NULL);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->dnd_target;
}

void flow_grid_set_cols ( GtkWidget *self, gint cols )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->cols = cols;
  priv->rows = 0;
  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
}

void flow_grid_set_rows ( GtkWidget *self, gint rows )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->rows = rows;
  priv->cols = 0;

  if((priv->rows<1)&&(priv->cols<1))
    priv->rows = 1;
}

gint flow_grid_get_cols ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_val_if_fail(IS_FLOW_GRID(self), -1);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->cols;
}

gint flow_grid_get_rows ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_val_if_fail(IS_FLOW_GRID(self), -1);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->rows;
}

void flow_grid_set_primary ( GtkWidget *self, gint primary )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->primary_axis = primary;
}

void flow_grid_set_sort ( GtkWidget *cgrid, gboolean sort )
{
  FlowGridPrivate *priv;

  g_return_if_fail(cgrid != NULL);
  g_return_if_fail(IS_FLOW_GRID(cgrid));
  priv = flow_grid_get_instance_private(FLOW_GRID(cgrid));

  priv->sort = sort;
}

void flow_grid_remove_widget ( GtkWidget *widget, GtkWidget *parent )
{
  gtk_container_remove ( GTK_CONTAINER(parent), widget );
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
  flow_item_set_parent(child, self);
  flow_grid_invalidate(self);
}

void flow_grid_delete_child ( GtkWidget *self, void *source )
{
  FlowGridPrivate *priv;
  GList *iter;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  if(!priv->children || !priv->children->data)
    return;

  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(!flow_item_check_source(iter->data, source))
    {
      g_object_unref(iter->data);
      priv->children = g_list_delete_link(priv->children, iter);
      break;
    }
  flow_grid_invalidate(self);
}

void flow_grid_update ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;
  gint count, i, cols, rows;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->invalid)
    return;
  priv->invalid = FALSE;

  if(!priv->primary_axis)
  {
    if(priv->rows>0)
      priv->primary_axis = G_TOKEN_COLS;
    else
      priv->primary_axis = G_TOKEN_ROWS;
  }

  gtk_container_foreach(GTK_CONTAINER(self),
      (GtkCallback)flow_grid_remove_widget,self);

  if(priv->sort)
    priv->children = g_list_sort_with_data(priv->children,
        (GCompareDataFunc)flow_item_compare,self);

  count = 0;
  for(iter=priv->children;iter;iter=g_list_next(iter))
  {
    flow_item_update(iter->data);
    if(flow_item_get_active(iter->data))
      count++;
  }

  rows = 0;
  cols = 0;
  if(priv->rows>0)
  {
    if(priv->primary_axis == G_TOKEN_COLS)
      rows = priv->rows;
    else
      cols = (count/priv->rows) + ((count%priv->rows)?1:0);
  }
  else
  {
    if(priv->primary_axis == G_TOKEN_ROWS)
      cols = priv->cols;
    else
      rows = (count/priv->cols) + ((count%priv->cols)?1:0);
  }

  i = 0;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(flow_item_get_active(iter->data))
    {
      if(rows>0)
        gtk_grid_attach(GTK_GRID(self), iter->data, i/rows, i%rows, 1, 1);
      else if(cols>0)
        gtk_grid_attach(GTK_GRID(self), iter->data, i%cols, i/cols, 1, 1);
      i++;
    }
  if(rows>0)
    for(;i<rows; i++)
      gtk_grid_attach(GTK_GRID(self), gtk_label_new(""), 0, i, 1, 1);
  else
    for(;i<cols;i++)
      gtk_grid_attach(GTK_GRID(self), gtk_label_new(""), i, 0, 1, 1);
  css_widget_cascade(self, NULL);
}

guint flow_grid_n_children ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;
  guint n = 0;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self),0);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(flow_item_get_active(iter->data))
      n++;

  return n;
}

gpointer flow_grid_find_child ( GtkWidget *self, gconstpointer source )
{
  FlowGridPrivate *priv;
  GList *iter;

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_val_if_fail(IS_FLOW_GRID(self),NULL);

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->children || !priv->children->data)
    return NULL;

  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(!flow_item_check_source(iter->data, source))
      return iter->data;

  return NULL;
}

void flow_grid_set_parent ( GtkWidget *self, GtkWidget *parent )
{
  FlowGridPrivate *priv;

  if(!IS_BASE_WIDGET(parent))
    return;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->parent = parent;
}

GtkWidget *flow_grid_get_parent ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_val_if_fail(IS_FLOW_GRID(self),NULL);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->parent;
}

void flow_grid_children_order ( GtkWidget *self, GtkWidget *ref,
    GtkWidget *child, gboolean after )
{
  FlowGridPrivate *priv;
  GList *dlist;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  dlist = g_list_find(priv->children, ref);
  if(!dlist || !g_list_find(priv->children, child))
    return;

  priv->children = g_list_remove(priv->children, child );
  priv->children = g_list_insert_before(priv->children,
      after ? g_list_next(dlist) : dlist, child );

  flow_item_invalidate(child);
  flow_item_invalidate(ref);
}

static void flow_grid_dnd_data_rec_cb ( GtkWidget *dest, GdkDragContext *ctx,
    gint x, gint y, GtkSelectionData *sel, guint info, guint time,
    gpointer parent )
{
  GtkWidget *src;
  if (info != SFWB_DND_TARGET_FLOW_ITEM)
    return;

  if(IS_BASE_WIDGET(parent))
    parent = base_widget_get_child(parent);
  g_return_if_fail(IS_FLOW_GRID(parent));
  src = *(GtkWidget **)gtk_selection_data_get_data(sel);

  flow_item_dnd_dest(dest, src, x, y);
}

static void flow_grid_dnd_enter_cb ( GtkWidget *widget, GdkEventCrossing *ev,
    gpointer data )
{
  bar_sensor_cancel_hide(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW));
}

static void flow_grid_dnd_begin_cb ( GtkWidget *widget, GdkDragContext *ctx,
    gpointer data )
{
  g_signal_handlers_unblock_matched(widget, G_SIGNAL_MATCH_FUNC, 0,0 ,NULL,
      (GFunc)flow_grid_dnd_enter_cb, NULL);
  gtk_grab_add(widget);
  window_ref(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW), widget);
}

static void flow_grid_dnd_end_cb ( GtkWidget *widget, GdkDragContext *ctx,
    gpointer data )
{
  g_signal_handlers_block_matched(widget, G_SIGNAL_MATCH_FUNC, 0,0 ,NULL,
      (GFunc)flow_grid_dnd_enter_cb,NULL);
  gtk_grab_remove(widget);
  window_unref(widget, gtk_widget_get_ancestor(data, GTK_TYPE_WINDOW));
}

static void flow_grid_dnd_data_get_cb ( GtkWidget *widget, GdkDragContext *ctx,
    GtkSelectionData *sel, guint info, guint time, gpointer *data )
{
  gtk_selection_data_set(sel,gdk_atom_intern_static_string("gpointer"),
      8,(const guchar *)&data,sizeof(gpointer));
}

void flow_grid_child_dnd_enable ( GtkWidget *self, GtkWidget *child,
    GtkWidget *src )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_ITEM(child));

  if(IS_BASE_WIDGET(self))
    self = base_widget_get_child(self);
  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->dnd_target)
    return;
  gtk_drag_dest_set(child, GTK_DEST_DEFAULT_ALL, priv->dnd_target, 1,
      GDK_ACTION_MOVE);
  gtk_drag_dest_add_uri_targets(child);
  gtk_drag_dest_add_text_targets(child);
  g_signal_connect(G_OBJECT(child), "drag-data-received",
      G_CALLBACK(flow_grid_dnd_data_rec_cb), self);

  if(src)
  {
    gtk_drag_source_set(src, GDK_BUTTON1_MASK, priv->dnd_target, 1,
        GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(src),"drag-data-get",
        G_CALLBACK(flow_grid_dnd_data_get_cb),child);
    g_signal_connect(G_OBJECT(src), "drag-begin",
        G_CALLBACK(flow_grid_dnd_begin_cb), self);
    g_signal_connect(G_OBJECT(src), "drag-end",
        G_CALLBACK(flow_grid_dnd_end_cb), self);
    g_signal_connect(G_OBJECT(src), "enter-notify-event",
        G_CALLBACK(flow_grid_dnd_enter_cb), NULL);
    g_signal_handlers_block_matched(src, G_SIGNAL_MATCH_FUNC, 0,0 ,NULL,
        (GFunc)flow_grid_dnd_enter_cb,NULL);
  }
}

void flow_grid_copy_properties ( GtkWidget *dest, GtkWidget *src )
{
  FlowGridPrivate *spriv, *dpriv;

  g_return_if_fail( IS_BASE_WIDGET(src) && IS_BASE_WIDGET(dest) );
  g_return_if_fail( IS_FLOW_GRID(base_widget_get_child(src)) &&
      IS_FLOW_GRID(base_widget_get_child(dest)) );
  spriv = flow_grid_get_instance_private(FLOW_GRID(
        base_widget_get_child(src)));
  dpriv = flow_grid_get_instance_private(FLOW_GRID(
        base_widget_get_child(dest)));

  dpriv->rows = spriv->rows;
  dpriv->cols = spriv->cols;
  dpriv->sort = spriv->sort;
  dpriv->primary_axis = spriv->primary_axis;

  g_object_set_data(G_OBJECT(dest), "icons",
      g_object_get_data(G_OBJECT(src), "icons"));
  g_object_set_data(G_OBJECT(dest), "labels",
      g_object_get_data(G_OBJECT(src), "labels"));
}
