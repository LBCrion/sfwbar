/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "gui/capture.h"
#include "gui/css.h"
#include "gui/filter.h"
#include "gui/flowgrid.h"
#include "gui/pager.h"
#include "gui/scaleimage.h"
#include "gui/switcher.h"
#include "gui/switcheritem.h"

G_DEFINE_TYPE_WITH_CODE (SwitcherItem, switcher_item, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE (SwitcherItem))

static void switcher_item_style_updated ( GtkWidget *w, GtkWidget *self )
{
  SwitcherItemPrivate *priv;
  gint dir;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  gtk_widget_style_get(base_widget_get_child(self), "direction", &dir, NULL);

  if(dir==priv->dir)
    return;
  priv->dir = dir;

  if(!priv->label)
    return;
  g_object_ref(priv->label);
  gtk_container_remove(GTK_CONTAINER(priv->grid), priv->label);
  gtk_grid_attach(GTK_GRID(priv->grid), priv->label,
      dir==GTK_POS_LEFT? 0 : dir==GTK_POS_RIGHT? 2 : 1,
      dir==GTK_POS_TOP? 0 : dir==GTK_POS_BOTTOM? 2 : 1,
      1, 1);
  g_object_unref(priv->label);
}

void switcher_item_update ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;
  gboolean preview;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  if(!priv->invalid)
    return;

  g_object_get(G_OBJECT(flow_item_get_parent(self)), "preview", &preview, NULL);

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)), priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
    capture_window_image_set(priv->icon, priv->win, preview);

  css_set_class(base_widget_get_child(self), "minimized",
      priv->win->state & WS_MINIMIZED);
  css_set_class(base_widget_get_child(self), "maximized",
      priv->win->state & WS_MAXIMIZED);
  css_set_class(base_widget_get_child(self), "fullscreen",
      priv->win->state & WS_FULLSCREEN);
  css_set_class(base_widget_get_child(self), "urgent",
      priv->win->state & WS_URGENT);
  css_set_class(base_widget_get_child(self), "focused",
      switcher_is_focused(priv->win->uid));
  css_set_class(base_widget_get_child(self), "preview",
      preview && priv->win->image);
  gtk_widget_unset_state_flags(base_widget_get_child(self),
      GTK_STATE_FLAG_PRELIGHT);

  flow_item_set_active(self, filter_window_check(priv->switcher, priv->win));
  switcher_item_style_updated(NULL, self);

  priv->invalid = FALSE;
}

static void switcher_item_icons_handle ( GtkWidget *parent, GParamSpec *spec,
    GtkWidget *self )
{
  SwitcherItemPrivate *priv;
  gboolean icons, preview;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  g_object_get(G_OBJECT(parent), "icons", &icons, "preview", &preview, NULL);

  if(priv->icon && !icons)
    g_clear_pointer(&priv->icon, gtk_widget_destroy);
  else if(icons && !priv->icon)
  {
    priv->icon = scale_image_new();
    gtk_grid_attach(GTK_GRID(priv->grid), priv->icon, 1, 1, 1, 1);
    switcher_item_update(self);
  }
}

static void switcher_item_labels_handle ( GtkWidget *parent, GParamSpec *spec,
    GtkWidget *self )
{
  SwitcherItemPrivate *priv;
  gboolean labels;
  gint title_width;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  g_object_get(G_OBJECT(parent), "labels", &labels,
      "title-width", &title_width, NULL);

  if(priv->label && !labels)
    g_clear_pointer(&priv->label, gtk_widget_destroy);
  else if(!priv->label && labels)
  {
    priv->label = gtk_label_new(priv->win? priv->win->title : "");
    gtk_label_set_ellipsize (GTK_LABEL(priv->label), PANGO_ELLIPSIZE_END);
    gtk_grid_attach(GTK_GRID(priv->grid), priv->label, 1, 2, 1, 1);
    switcher_item_style_updated(NULL, self);
  }
  if(priv->label)
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label), title_width);
}

void switcher_item_destroy ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));
  g_signal_handlers_disconnect_by_data(priv->switcher, self);
  GTK_WIDGET_CLASS(switcher_item_parent_class)->destroy(self);
}

window_t *switcher_item_get_window ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_val_if_fail(IS_SWITCHER_ITEM(self), NULL);
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  return priv->win;
}

static void switcher_item_invalidate ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  flow_grid_invalidate(priv->switcher);
  priv->invalid = TRUE;
}

static gint switcher_item_compare ( GtkWidget *a, GtkWidget *b,
    GtkWidget *parent )
{
  SwitcherItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_SWITCHER_ITEM(a), 0);
  g_return_val_if_fail(IS_SWITCHER_ITEM(b), 0);

  p1 = switcher_item_get_instance_private(SWITCHER_ITEM(a));
  p2 = switcher_item_get_instance_private(SWITCHER_ITEM(b));

  if(g_list_find(g_list_find(wintree_get_list(), p1->win), p2->win))
    return -1;
  else
    return 1;
}

static void switcher_item_class_init ( SwitcherItemClass *kclass )
{
  FLOW_ITEM_CLASS(kclass)->update = switcher_item_update;
  FLOW_ITEM_CLASS(kclass)->compare = switcher_item_compare;
  FLOW_ITEM_CLASS(kclass)->invalidate = switcher_item_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_source =
    (void * (*)(GtkWidget *))switcher_item_get_window;
  GTK_WIDGET_CLASS(kclass)->destroy = switcher_item_destroy;
}

static void switcher_item_init ( SwitcherItem *cgrid )
{
}

GtkWidget *switcher_item_new( window_t *win, GtkWidget *switcher )
{
  GtkWidget *self;
  SwitcherItemPrivate *priv;

  if(!switcher)
    return NULL;

  self = GTK_WIDGET(g_object_new(switcher_item_get_type(), NULL));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  priv->win = win;
  priv->switcher = switcher;

  priv->grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(self), priv->grid);
  gtk_widget_set_name(priv->grid, "switcher_item");
  g_object_ref_sink(G_OBJECT(self));

  g_signal_connect(G_OBJECT(switcher), "notify::labels",
      G_CALLBACK(switcher_item_labels_handle), self);
  g_signal_connect(G_OBJECT(switcher), "notify::icons",
      G_CALLBACK(switcher_item_icons_handle), self);
  g_signal_connect(G_OBJECT(switcher), "notify::title-width",
      G_CALLBACK(switcher_item_labels_handle), self);
  g_signal_connect(G_OBJECT(priv->grid), "style-updated",
      G_CALLBACK(switcher_item_style_updated), self);
  switcher_item_labels_handle(switcher, NULL, self);
  switcher_item_icons_handle(switcher, NULL, self);

  flow_item_invalidate(self);

  return self;
}
