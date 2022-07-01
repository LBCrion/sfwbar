
/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"
#include "trayitem.h"

G_DEFINE_TYPE_WITH_CODE (TrayItem, tray_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (TrayItem));

static void tray_item_destroy ( GtkWidget *self )
{
}

void tray_item_update ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_if_fail(IS_TRAY_ITEM(self));

  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  if(priv->sni->string[SNI_PROP_STATUS]!=NULL)
  {
    if(priv->sni->string[SNI_PROP_STATUS][0]=='A')
    {
      gtk_widget_set_name(priv->icon,"tray_active");
      sni_item_set_icon(priv->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
    if(priv->sni->string[SNI_PROP_STATUS][0]=='N')
    {
      gtk_widget_set_name(priv->icon,"tray_attention");
      sni_item_set_icon(priv->sni,SNI_PROP_ATTN,SNI_PROP_ATTNPIX);
    }
    if(priv->sni->string[SNI_PROP_STATUS][0]=='P')
    {
      gtk_widget_set_name(priv->icon,"tray_passive");
      sni_item_set_icon(priv->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
  }

  widget_set_css(self,NULL);
}

sni_item_t *tray_item_get_sni ( GtkWidget *self )
{
  TrayItemPrivate *priv;

  g_return_val_if_fail(IS_TRAY_ITEM(self),NULL);
  priv = tray_item_get_instance_private(TRAY_ITEM(self));


  return priv->sni;
}

static void tray_item_class_init ( TrayItemClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = tray_item_destroy;
  FLOW_ITEM_CLASS(kclass)->update = tray_item_update;
  FLOW_ITEM_CLASS(kclass)->compare = tray_item_compare;
  FLOW_ITEM_CLASS(kclass)->get_parent = 
    (void *(*)(GtkWidget *))tray_item_get_sni;
}

static void tray_item_init ( TrayItem *self )
{
}

GtkWidget *tray_item_new( sni_item_t *sni, GtkWidget *tray )
{
  GtkWidget *self;
  TrayItemPrivate *priv;

  g_return_val_if_fail(sni,NULL);

  self = GTK_WIDGET(g_object_new(tray_item_get_type(), NULL));
  priv = tray_item_get_instance_private(TRAY_ITEM(self));

  priv->icon = scale_image_new();
  priv->sni = sni;
  priv->sni->image = priv->icon;

  g_object_ref(self);
  flow_grid_add_child(tray,self);

  gtk_container_add(GTK_CONTAINER(self),priv->icon);
  gtk_widget_add_events(GTK_WIDGET(self),GDK_SCROLL_MASK);
  g_signal_connect(G_OBJECT(self),"button-press-event",
      G_CALLBACK(sni_item_click_cb),priv->sni);
  g_signal_connect(G_OBJECT(self),"scroll-event",
      G_CALLBACK(sni_item_scroll_cb),priv->sni);
  return self;
}

gint tray_item_compare ( GtkWidget *a, GtkWidget *b, GtkWidget *parent )
{
  TrayItemPrivate *p1,*p2;

  g_return_val_if_fail(IS_TRAY_ITEM(a),0);
  g_return_val_if_fail(IS_TRAY_ITEM(b),0);

  p1 = tray_item_get_instance_private(TRAY_ITEM(a));
  p2 = tray_item_get_instance_private(TRAY_ITEM(b));
  return g_strcmp0(p1->sni->string[SNI_PROP_TITLE],p2->sni->string[SNI_PROP_TITLE]);
}
