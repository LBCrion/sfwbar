/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"

G_DEFINE_TYPE_WITH_CODE (PagerItem, pager_item, FLOW_ITEM_TYPE, G_ADD_PRIVATE (PagerItem));

void pager_item_update ( GtkWidget *self )
{
  PagerItemPrivate *priv;

  g_return_if_fail(IS_PAGER_ITEM(self));

  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  if ( priv->focused )
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "pager_focused");
  else if (priv->visible)
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "pager_visible");
  else
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(self)), "pager_normal");

  gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(self)),
      GTK_STATE_FLAG_PRELIGHT);

  widget_set_css(self,NULL);
}

gchar *pager_item_get_label ( GtkWidget *self )
{
  PagerItemPrivate *priv;

  g_return_val_if_fail(IS_PAGER_ITEM(self),NULL);

  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  if(!priv || !priv->label)
    return NULL;

  return (gchar *)gtk_label_get_text(GTK_LABEL(priv->label));
}

static void pager_item_class_init ( PagerItemClass *kclass )
{
  FLOW_ITEM_CLASS(kclass)->update = pager_item_update;
  FLOW_ITEM_CLASS(kclass)->get_parent = (void * (*)(GtkWidget *))pager_item_get_label;
}

static void pager_item_init ( PagerItem *self )
{
}

static void pager_item_button_cb( GtkWidget *self, gpointer data )
{
  gchar *label;
  if(!sway_ipc_active())
    return;
  label = pager_item_get_label(self);
  if(label==NULL)
    return;
  sway_ipc_command("workspace '%s'",label);
}

GtkWidget *pager_item_new( gchar *text )
{
  GtkWidget *self;
  PagerItemPrivate *priv;

  self = GTK_WIDGET(g_object_new(pager_item_get_type(), NULL));
  priv = pager_item_get_instance_private(PAGER_ITEM(self));

  priv->button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(self),priv->button);
  gtk_widget_set_name(priv->button, "pager_normal");
  priv->label = gtk_label_new(text);
  gtk_label_set_ellipsize (GTK_LABEL(priv->label),PANGO_ELLIPSIZE_END);
  g_signal_connect(priv->button,"clicked",G_CALLBACK(pager_item_button_cb),NULL);

  return self;
}

