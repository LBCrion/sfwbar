/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "gui/css.h"
#include "gui/filter.h"
#include "gui/flowgrid.h"
#include "gui/pager.h"
#include "gui/scaleimage.h"
#include "gui/switcher.h"
#include "gui/switcheritem.h"

G_DEFINE_TYPE_WITH_CODE (SwitcherItem, switcher_item, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE (SwitcherItem))

static void switcher_item_decorate ( GtkWidget *parent, GParamSpec *spec,
    GtkWidget *self )
{
  SwitcherItemPrivate *priv;
  gboolean labels, icons;
  gint dir, title_width;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  g_object_get(G_OBJECT(parent), "labels", &labels, "icons", &icons,
      "title-width", &title_width, NULL);

  if(!labels && !icons)
    labels = TRUE;

  if(!!priv->icon != icons || !!priv->label != labels)
  {
    g_clear_pointer(&priv->label, gtk_widget_destroy);
    g_clear_pointer(&priv->icon, gtk_widget_destroy);
    gtk_widget_style_get(base_widget_get_child(self), "direction", &dir, NULL);

    if(icons)
    {
      priv->icon = scale_image_new();
      gtk_grid_attach_next_to(GTK_GRID(priv->grid), priv->icon, NULL, dir, 1,1);
      if(priv->win)
        scale_image_set_image(priv->icon, priv->win->appid, NULL);
    }
    if(labels)
    {
      priv->label = gtk_label_new(priv->win?priv->win->title:"");
      gtk_label_set_ellipsize (GTK_LABEL(priv->label), PANGO_ELLIPSIZE_END);
      gtk_grid_attach_next_to(GTK_GRID(priv->grid), priv->label, priv->icon,
          dir, 1,1);
    }
  }
  if(priv->label)
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label), title_width);
}

void switcher_item_update ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  if(!priv->invalid)
    return;

  if(priv->label)
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)), priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
    scale_image_set_image(priv->icon, priv->win->appid, NULL);

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
  gtk_widget_unset_state_flags(base_widget_get_child(self),
      GTK_STATE_FLAG_PRELIGHT);

  flow_item_set_active(self, filter_window_check(priv->switcher, priv->win));

  priv->invalid = FALSE;
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

  g_return_val_if_fail(IS_SWITCHER_ITEM(a),0);
  g_return_val_if_fail(IS_SWITCHER_ITEM(b),0);

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
      G_CALLBACK(switcher_item_decorate), self);
  g_signal_connect(G_OBJECT(switcher), "notify::icons",
      G_CALLBACK(switcher_item_decorate), self);
  g_signal_connect(G_OBJECT(switcher), "notify::title-width",
      G_CALLBACK(switcher_item_decorate), self);
  switcher_item_decorate(switcher, NULL, self);

  flow_item_invalidate(self);

  return self;
}
