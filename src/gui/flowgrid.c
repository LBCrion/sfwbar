/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include "css.h"
#include "flowgrid.h"
#include "basewidget.h"
#include "taskbarpopup.h"
#include "config.h"
#include "window.h"
#include "bar.h"

G_DEFINE_TYPE_WITH_CODE (FlowGrid, flow_grid, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE(FlowGrid))

static void flow_grid_get_preferred_width (GtkWidget *self, gint *minimal,
    gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_width(
      self, minimal, natural);

  if(priv->rows>0 && priv->limit)
    *minimal = MIN(*natural, 1);
}

static void flow_grid_get_preferred_height (GtkWidget *self, gint *minimal,
    gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_height(
      self, minimal, natural);

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

static void flow_grid_style_updated ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  gboolean h;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  gtk_widget_style_get(GTK_WIDGET(priv->grid), "row-homogeneous", &h, NULL);
  gtk_grid_set_row_homogeneous(priv->grid, h);
  gtk_widget_style_get(GTK_WIDGET(priv->grid), "column-homogeneous", &h, NULL);
  gtk_grid_set_column_homogeneous(priv->grid, h);

  GTK_WIDGET_CLASS(flow_grid_parent_class)->style_updated(self);
}

static void flow_grid_action_configure ( GtkWidget *self, gint slot )
{
  FlowGridPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  for(iter=priv->children; iter; iter=g_list_next(iter))
    base_widget_action_configure(iter->data, slot);
}

static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
  widget_class->destroy = flow_grid_destroy;
  widget_class->style_updated = flow_grid_style_updated;
  BASE_WIDGET_CLASS(kclass)->action_configure = flow_grid_action_configure;
}

static void flow_grid_init ( FlowGrid *self )
{
  FlowGridPrivate *priv;
  gchar *sig;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->grid = GTK_GRID(gtk_grid_new());
  gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->grid));
  flow_grid_set_sort(GTK_WIDGET(self), TRUE);

  priv->rows = 1;
  priv->cols = 0;
  priv->limit = TRUE;
  priv->labels = TRUE;
  priv->icons = FALSE;
  priv->title_width = -1;
  sig = g_strdup_printf("flow-item-%p", (void *)self);
  priv->dnd_target = gtk_target_entry_new(sig, 0, SFWB_DND_TARGET_FLOW_ITEM);
  g_free(sig);
}

void flow_grid_set_limit ( GtkWidget *self, gboolean limit )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->limit = limit;
}

void flow_grid_set_dnd_target ( GtkWidget *self, GtkTargetEntry *target )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  g_clear_pointer(&priv->dnd_target, gtk_target_entry_free);
  if(target)
    priv->dnd_target = gtk_target_entry_copy(target);
}

GtkTargetEntry *flow_grid_get_dnd_target ( GtkWidget *self )
{
  FlowGridPrivate *priv;

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

  flow_grid_invalidate(self);
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

  flow_grid_invalidate(self);
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
  flow_grid_invalidate(self);
}

void flow_grid_set_sort ( GtkWidget *self, gboolean sort )
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->sort = sort;
  flow_grid_invalidate(self);
}

static void flow_grid_remove_widget_maybe (GtkWidget *widget, GtkWidget *self)
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!g_list_find(priv->children, widget))
    gtk_container_remove (GTK_CONTAINER(priv->grid), widget);
}

void flow_grid_invalidate ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    flow_grid_invalidate(iter->data);

  priv->invalid = TRUE;
}

void flow_grid_add_child ( GtkWidget *self, GtkWidget *child )
{
  FlowGridPrivate *priv, *ppriv;
  gint i;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  ppriv = flow_grid_get_instance_private(
      FLOW_GRID(base_widget_get_mirror_parent(self)));

  for(i=0; i<=BASE_WIDGET_MAX_ACTION; i++)
    base_widget_action_configure(child, i);
  priv->children = g_list_append(priv->children, child);
  flow_item_set_parent(child, self);
  flow_item_decorate(child, ppriv->labels, ppriv->icons);
  flow_item_set_title_width(child, ppriv->title_width);
  priv->invalid = TRUE;
}

void flow_grid_delete_child ( GtkWidget *self, void *source )
{
  FlowGridPrivate *priv;
  GList *iter;

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
  priv->invalid = TRUE;
}

void flow_grid_child_position ( GtkGrid *grid, GtkWidget *child, gint x, gint y )
{
  if(!gtk_widget_get_parent(child))
    gtk_grid_attach(GTK_GRID(grid), child, x, y, 1, 1);
  else
    gtk_container_child_set(GTK_CONTAINER(grid), child,
        "left-attach", x,
        "top-attach", y,
        "width", 1,
        "height", 1,
        NULL);
}

gboolean flow_grid_update ( GtkWidget *self )
{
  FlowGridPrivate *priv, *ppriv;
  GList *iter;
  gint count, i, cols, rows;

  g_return_val_if_fail(IS_FLOW_GRID(self), FALSE);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  ppriv = flow_grid_get_instance_private(
      FLOW_GRID(base_widget_get_mirror_parent(self)));

  if(!priv->invalid)
    return TRUE;
  priv->invalid = FALSE;

  if(!ppriv->primary_axis)
  {
    if(ppriv->rows>0)
      ppriv->primary_axis = G_TOKEN_COLS;
    else
      ppriv->primary_axis = G_TOKEN_ROWS;
  }

  gtk_container_foreach(GTK_CONTAINER(priv->grid),
      (GtkCallback)flow_grid_remove_widget_maybe, self);

  if(ppriv->sort)
    priv->children = g_list_sort_with_data(priv->children,
        (GCompareDataFunc)flow_item_compare, self);

  count = 0;
  for(iter=priv->children; iter; iter=g_list_next(iter))
  {
    flow_item_update(iter->data);
    if(flow_item_get_active(iter->data))
      count++;
  }

  rows = 0;
  cols = 0;
  if(ppriv->rows>0)
  {
    if(ppriv->primary_axis == G_TOKEN_COLS)
      rows = ppriv->rows;
    else
      cols = (count/ppriv->rows) + ((count%ppriv->rows)?1:0);
  }
  else
  {
    if(ppriv->primary_axis == G_TOKEN_ROWS)
      cols = ppriv->cols;
    else
      rows = (count/ppriv->cols) + ((count%ppriv->cols)?1:0);
  }

  i = 0;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(flow_item_get_active(iter->data))
    {
      if(rows>0)
        flow_grid_child_position(priv->grid, iter->data, i/rows, i%rows);
      else if(cols>0)
        flow_grid_child_position(priv->grid, iter->data, i%cols, i/cols);
      else
        g_warning("invalid row/column configuration in a FlowGrid");
      i++;
    }
    else if(gtk_widget_get_parent(iter->data))
      gtk_container_remove(GTK_CONTAINER(priv->grid), iter->data);

  if(rows>0)
    for(;i<rows; i++)
      gtk_grid_attach(priv->grid, gtk_label_new(""), 0, i, 1, 1);
  else
    for(;i<cols;i++)
      gtk_grid_attach(priv->grid, gtk_label_new(""), i, 0, 1, 1);
  css_widget_cascade(self, NULL);
  return TRUE;
}

GList *flow_grid_get_children ( GtkWidget *self )
{
  FlowGridPrivate *priv;

  g_return_val_if_fail(IS_FLOW_GRID(self),0);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  return priv->children;
}

guint flow_grid_n_children ( GtkWidget *self )
{
  FlowGridPrivate *priv;
  GList *iter;
  guint n = 0;

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

  g_return_val_if_fail(IS_FLOW_GRID(self), NULL);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->children || !priv->children->data)
    return NULL;

  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(!flow_item_check_source(iter->data, source))
      return iter->data;

  return NULL;
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

void flow_grid_set_labels ( GtkWidget *self, gboolean labels )
{
  FlowGridPrivate *priv, *ppriv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  ppriv = flow_grid_get_instance_private(
      FLOW_GRID(base_widget_get_mirror_parent(self)));

  if(ppriv->labels == labels)
    return;

  ppriv->labels = labels;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    flow_item_decorate(iter->data, ppriv->labels, ppriv->icons);

  if(labels)
    for(iter=priv->children; iter; iter=g_list_next(iter))
      flow_item_set_title_width(iter->data, ppriv->title_width);

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    flow_grid_set_labels(iter->data, labels);
}

void flow_grid_set_icons ( GtkWidget *self, gboolean icons )
{
  FlowGridPrivate *priv, *ppriv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  ppriv = flow_grid_get_instance_private(
      FLOW_GRID(base_widget_get_mirror_parent(self)));

  if(ppriv->icons == icons)
    return;

  if(!icons && !ppriv->labels)
    ppriv->labels = TRUE;

  ppriv->icons = icons;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    flow_item_decorate(iter->data, ppriv->labels, ppriv->icons);

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    flow_grid_set_icons(iter->data, icons);
}

void flow_grid_set_title_width ( GtkWidget *self, gint width )
{
  FlowGridPrivate *priv;
  GList *iter;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(priv->title_width == width)
    return;

  priv->title_width = width;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    flow_item_set_title_width(iter->data, priv->title_width);

  for(iter=base_widget_get_mirror_children(self); iter; iter=g_list_next(iter))
    flow_grid_set_title_width(iter->data, width);
}

static void flow_grid_dnd_data_rec_cb ( GtkWidget *dest, GdkDragContext *ctx,
    gint x, gint y, GtkSelectionData *sel, guint info, guint time,
    gpointer parent )
{
  GtkWidget *src;

  if(info == SFWB_DND_TARGET_FLOW_ITEM)
  {
    g_return_if_fail(IS_FLOW_GRID(parent));

    src = *(GtkWidget **)gtk_selection_data_get_data(sel);
    flow_item_dnd_dest(dest, src, x, y);
  }
  gtk_drag_finish(ctx, TRUE, FALSE, time);
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

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->dnd_target)
    return;
  gtk_drag_dest_set(child, GTK_DEST_DEFAULT_ALL, priv->dnd_target, 1,
      GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(child), "drag-data-received",
      G_CALLBACK(flow_grid_dnd_data_rec_cb), self);
  gtk_drag_dest_set_track_motion(child, TRUE);

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
