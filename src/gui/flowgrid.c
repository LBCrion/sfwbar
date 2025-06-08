/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include "css.h"
#include "gui/flowgrid.h"
#include "config/config.h"
#include "window.h"
#include "gui/bar.h"

G_DEFINE_TYPE_WITH_CODE (FlowGrid, flow_grid, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE(FlowGrid))

enum {
  FLOW_GRID_LABELS = 1,
  FLOW_GRID_ICONS,
  FLOW_GRID_TOOLTIPS,
  FLOW_GRID_TITLE_WIDTH,
  FLOW_GRID_SORT,
  FLOW_GRID_COLS,
  FLOW_GRID_ROWS,
  FLOW_GRID_PRIMARY_AXIS,
  FLOW_GRID_VALUE_OVERRIDE,
  FLOW_GRID_TOOLTIP_OVERRIDE,
};

GEnumValue flow_grid_axis[] = {
  { FLOW_GRID_AXIS_DEFAULT, "default", "default" },
  { FLOW_GRID_AXIS_ROWS, "rows", "rows" },
  { FLOW_GRID_AXIS_COLS, "cols", "cols" },
};

static void flow_grid_get_preferred_width (GtkWidget *self, gint *minimal,
    gint *natural)
{
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  GTK_WIDGET_CLASS(flow_grid_parent_class)->get_preferred_width(
      self, minimal, natural);

  if(priv->rows>0 && FLOW_GRID_GET_CLASS(self)->limit)
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

  if(priv->cols>0 && FLOW_GRID_GET_CLASS(self)->limit)
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

static void flow_grid_get_property ( GObject *self, guint id, GValue *value,
    GParamSpec *spec )
{
  FlowGridPrivate *priv;

  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  switch(id)
  {
    case FLOW_GRID_LABELS:
      g_value_set_boolean(value, priv->labels);
      break;
    case FLOW_GRID_ICONS:
      g_value_set_boolean(value, priv->icons);
      break;
    case FLOW_GRID_TOOLTIPS:
      g_value_set_boolean(value, priv->tooltips);
      break;
    case FLOW_GRID_TITLE_WIDTH:
      g_value_set_int(value, priv->title_width);
      break;
    case FLOW_GRID_SORT:
      g_value_set_boolean(value, priv->sort);
      break;
    case FLOW_GRID_COLS:
      g_value_set_int(value, priv->cols);
      break;
    case FLOW_GRID_ROWS:
      g_value_set_int(value, priv->rows);
      break;
    case FLOW_GRID_PRIMARY_AXIS:
      g_value_set_enum(value, priv->primary_axis);
      break;
    case FLOW_GRID_VALUE_OVERRIDE:
    case FLOW_GRID_TOOLTIP_OVERRIDE:
      g_value_set_boxed(value, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void flow_grid_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  FlowGridPrivate *priv;

  priv = flow_grid_get_instance_private(FLOW_GRID(self));
  switch(id)
  {
    case FLOW_GRID_LABELS:
      priv->labels = g_value_get_boolean(value);
      break;
    case FLOW_GRID_ICONS:
      priv->icons = g_value_get_boolean(value);
      break;
    case FLOW_GRID_TOOLTIPS:
      priv->tooltips = g_value_get_boolean(value);
      break;
    case FLOW_GRID_TITLE_WIDTH:
      priv->title_width = g_value_get_int(value);
      break;
    case FLOW_GRID_SORT:
      priv->sort = g_value_get_boolean(value);
      flow_grid_invalidate(GTK_WIDGET(self));
      break;
    case FLOW_GRID_COLS:
      priv->cols = g_value_get_int(value);
      priv->rows = (priv->cols<1);
      flow_grid_invalidate(GTK_WIDGET(self));
      break;
    case FLOW_GRID_ROWS:
      priv->rows = g_value_get_int(value);
      priv->cols = (priv->rows<1);
      flow_grid_invalidate(GTK_WIDGET(self));
      break;
    case FLOW_GRID_PRIMARY_AXIS:
      priv->primary_axis = g_value_get_enum(value);
      flow_grid_invalidate(GTK_WIDGET(self));
      break;
    case FLOW_GRID_VALUE_OVERRIDE:
    case FLOW_GRID_TOOLTIP_OVERRIDE:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
  }
}

static void flow_grid_mirror ( GtkWidget *dest, GtkWidget *src )
{
  BASE_WIDGET_CLASS(flow_grid_parent_class)->mirror(dest, src);

  g_object_bind_property(G_OBJECT(src), "labels",
      G_OBJECT(dest), "labels", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "icons",
      G_OBJECT(dest), "icons", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "tooltips",
      G_OBJECT(dest), "tooltips", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "title_width",
      G_OBJECT(dest), "title_width", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "sort",
      G_OBJECT(dest), "sort", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "cols",
      G_OBJECT(dest), "cols", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "rows",
      G_OBJECT(dest), "rows", G_BINDING_SYNC_CREATE);
  g_object_bind_property(G_OBJECT(src), "primary_axis",
      G_OBJECT(dest), "primary_axis", G_BINDING_SYNC_CREATE);
}


static void flow_grid_class_init ( FlowGridClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->get_preferred_width = flow_grid_get_preferred_width;
  widget_class->get_preferred_height = flow_grid_get_preferred_height;
  widget_class->destroy = flow_grid_destroy;
  widget_class->style_updated = flow_grid_style_updated;

  kclass->limit = TRUE;

  G_OBJECT_CLASS(kclass)->get_property = flow_grid_get_property;
  G_OBJECT_CLASS(kclass)->set_property = flow_grid_set_property;
  BASE_WIDGET_CLASS(kclass)->mirror = flow_grid_mirror;

  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_LABELS,
      g_param_spec_boolean("labels", "labels", "sfwbar_config", TRUE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_ICONS,
      g_param_spec_boolean("icons", "icons", "sfwbar_config", FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_TOOLTIPS,
      g_param_spec_boolean("tooltips", "tooltips", "sfwbar_config", FALSE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      FLOW_GRID_TITLE_WIDTH, g_param_spec_int("title_width", "title_width",
        "sfwbar_config", -1, INT_MAX, -1, G_PARAM_READWRITE |
        G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_SORT,
      g_param_spec_boolean("sort", "sort", "sfwbar_config", TRUE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_COLS,
    g_param_spec_int("cols", "cols", "sfwbar_config", -1, INT_MAX, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass), FLOW_GRID_ROWS,
    g_param_spec_int("rows", "rows", "sfwbar_config", -1, INT_MAX, 1,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(G_OBJECT_CLASS(kclass),
      FLOW_GRID_PRIMARY_AXIS,
      g_param_spec_enum("primary_axis", "primary_axis", "sfwbar_config",
        g_enum_register_static("flow_grid_axis", flow_grid_axis),
        FLOW_GRID_AXIS_DEFAULT, G_PARAM_READWRITE));
  g_object_class_override_property(G_OBJECT_CLASS(kclass),
      FLOW_GRID_VALUE_OVERRIDE, "value");
  g_object_class_override_property(G_OBJECT_CLASS(kclass),
      FLOW_GRID_TOOLTIP_OVERRIDE, "tooltip");
}

static void flow_grid_init ( FlowGrid *self )
{
  FlowGridPrivate *priv;
  gchar *sig;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->grid = GTK_GRID(gtk_grid_new());
  gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->grid));

  sig = g_strdup_printf("flow-item-%p", (void *)self);
  priv->dnd_target = gtk_target_entry_new(sig, 0, SFWB_DND_TARGET_FLOW_ITEM);
  g_free(sig);
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
  FlowGridPrivate *priv;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  priv->children = g_list_append(priv->children, child);
  flow_item_set_parent(child, self);
  flow_grid_invalidate(self);
}

void flow_grid_delete_child ( GtkWidget *self, void *source )
{
  FlowGridPrivate *priv;
  GtkWidget *child;

  g_return_if_fail(IS_FLOW_GRID(self));
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if( !(child = flow_grid_find_child(self, source)) )
    return;

  priv->children = g_list_remove(priv->children, child);
  g_object_unref(child);
  flow_grid_invalidate(self);
}

static void flow_grid_child_position ( GtkGrid *grid, GtkWidget *child,
    gint x, gint y )
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
  FlowGridPrivate *priv;
  GList *iter;
  gint count, i, dim, axis_cols;

  g_return_val_if_fail(IS_FLOW_GRID(self), FALSE);
  priv = flow_grid_get_instance_private(FLOW_GRID(self));

  if(!priv->invalid)
    return TRUE;
  priv->invalid = FALSE;

  gtk_container_foreach(GTK_CONTAINER(priv->grid),
      (GtkCallback)flow_grid_remove_widget_maybe, self);

  if(priv->sort)
    priv->children = g_list_sort_with_data(priv->children,
        (GCompareDataFunc)flow_item_compare, self);

  count = 0;
  for(iter=priv->children; iter; iter=g_list_next(iter))
  {
    flow_item_update(iter->data);
    if(flow_item_get_active(iter->data))
      count++;
  }

  axis_cols = (priv->primary_axis == FLOW_GRID_AXIS_COLS ||
     (priv->primary_axis == FLOW_GRID_AXIS_DEFAULT && priv->rows>0));

  if(axis_cols)
    dim = priv->rows>0? priv->rows : (count/priv->cols) + !!(count%priv->cols);
  else
    dim = priv->cols>0? priv->cols : (count/priv->rows) + !!(count%priv->rows);

  i = 0;
  for(iter=priv->children; iter; iter=g_list_next(iter))
    if(flow_item_get_active(iter->data))
    {
      flow_grid_child_position(priv->grid, iter->data,
          axis_cols? i/dim : i%dim, axis_cols? i%dim : i/dim);
      i++;
    }
    else if(gtk_widget_get_parent(iter->data) == GTK_WIDGET(priv->grid))
      gtk_container_remove(GTK_CONTAINER(priv->grid), iter->data);

  for(;i<dim; i++)
    gtk_grid_attach(priv->grid, gtk_label_new(""),
        axis_cols? 0 : i, axis_cols? i : 0, 1, 1);
  css_widget_cascade(self, NULL);

  return TRUE;
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

  iter = g_list_find_custom(priv->children, source,
      (GCompareFunc)flow_item_check_source);
  return iter? iter->data : NULL;
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

  priv->children = g_list_remove(priv->children, child);
  priv->children = g_list_insert_before(priv->children,
      after ? g_list_next(dlist) : dlist, child);

  flow_item_invalidate(child);
  flow_item_invalidate(ref);
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
  gtk_selection_data_set(sel, gdk_atom_intern_static_string("gpointer"),
      8, (const guchar *)&data, sizeof(gpointer));
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
    g_signal_handlers_block_matched(src, G_SIGNAL_MATCH_FUNC, 0, 0 ,NULL,
        (GFunc)flow_grid_dnd_enter_cb, NULL);
  }
}
