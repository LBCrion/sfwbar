/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "switcheritem.h"
#include "flowgrid.h"
#include "scaleimage.h"
#include "pager.h"
#include "switcher.h"

G_DEFINE_TYPE_WITH_CODE (SwitcherItem, switcher_item, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE (SwitcherItem))

static gboolean switcher_item_check ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_val_if_fail(IS_SWITCHER_ITEM(self),FALSE);
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  return switcher_check(priv->switcher, priv->win);
}

static void switcher_item_decorate ( GtkWidget *self, gboolean labels,
    gboolean icons )
{
  SwitcherItemPrivate *priv;
  gint dir;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

  if(!labels && !icons)
    labels = TRUE;

  if(!!priv->icon == icons && !!priv->label == labels)
    return;

  g_clear_pointer(&priv->label, gtk_widget_destroy);
  g_clear_pointer(&priv->icon, gtk_widget_destroy);
  gtk_widget_style_get(gtk_bin_get_child(GTK_BIN(self)), "direction", &dir,
      NULL);

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
    gtk_grid_attach_next_to(GTK_GRID(priv->grid), priv->label, priv->icon, dir,
        1,1);
  }
}

static void switcher_item_set_title_width ( GtkWidget *self, gint title_width )
{
  SwitcherItemPrivate *priv;

  g_return_if_fail(IS_SWITCHER_ITEM(self));
  priv = switcher_item_get_instance_private(SWITCHER_ITEM(self));

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
    if(g_strcmp0(gtk_label_get_text(GTK_LABEL(priv->label)),priv->win->title))
      gtk_label_set_text(GTK_LABEL(priv->label), priv->win->title);

  if(priv->icon)
    scale_image_set_image(priv->icon,priv->win->appid,NULL);

  css_set_class(gtk_bin_get_child(GTK_BIN(self)), "focused",
      switcher_is_focused(((window_t *)flow_item_get_source(self))->uid));
  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  flow_item_set_active(self, switcher_item_check(self));

  priv->invalid = FALSE;
}

window_t *switcher_item_get_window ( GtkWidget *self )
{
  SwitcherItemPrivate *priv;

  g_return_val_if_fail(IS_SWITCHER_ITEM(self),NULL);
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
  FLOW_ITEM_CLASS(kclass)->decorate = switcher_item_decorate;
  FLOW_ITEM_CLASS(kclass)->set_title_width = switcher_item_set_title_width;
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
  flow_item_invalidate(self);

  return self;
}
