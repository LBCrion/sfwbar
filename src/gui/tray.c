/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "trayitem.h"
#include "tray.h"

G_DEFINE_TYPE_WITH_CODE (Tray, tray, FLOW_GRID_TYPE, G_ADD_PRIVATE (Tray))

static void tray_destroy ( GtkWidget *self )
{
  TrayPrivate *priv;

  g_return_if_fail(IS_TRAY(self));
  priv = tray_get_instance_private(TRAY(self));
  if(priv->timer_h)
  {
    g_source_remove(priv->timer_h);
    priv->timer_h = 0;
  }
  GTK_WIDGET_CLASS(tray_parent_class)->destroy(self);
}

static void tray_class_init ( TrayClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = tray_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
  sni_init();
}

static void tray_invalidate_item ( sni_item_t *sni, GtkWidget *self )
{
  flow_item_invalidate(flow_grid_find_child(self, sni));
}

static void tray_destroy_item ( sni_item_t *sni, GtkWidget *self )
{
  flow_grid_delete_child(self, sni);
}

static sni_listener_t tray_sni_listener = {
  .sni_new = (void (*)(sni_item_t *, void *))tray_item_new,
  .sni_invalidate = (void (*)(sni_item_t *, void *))tray_invalidate_item,
  .sni_destroy = (void (*)(sni_item_t *, void *))tray_destroy_item,
};

static void tray_init ( Tray *self )
{
  TrayPrivate *priv;

  sni_listener_register(&tray_sni_listener, self);
  priv = tray_get_instance_private(TRAY(self));
  priv->timer_h = g_timeout_add(100, (GSourceFunc)flow_grid_update, self);
  gtk_grid_set_column_homogeneous(
      GTK_GRID(base_widget_get_child(GTK_WIDGET(self))), FALSE);

  g_list_foreach(sni_item_get_list(), (GFunc)tray_item_new, self);
}

GtkWidget *tray_new ( void )
{
  return GTK_WIDGET(g_object_new(tray_get_type(), NULL));
}
