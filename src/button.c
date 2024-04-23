/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "button.h"
#include "scaleimage.h"

G_DEFINE_TYPE_WITH_CODE (Button, button, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Button))

static void button_update_value ( GtkWidget *self )
{
  ButtonPrivate *priv;

  g_return_if_fail(IS_BUTTON(self));
  priv = button_get_instance_private(BUTTON(self));

  scale_image_set_image(GTK_WIDGET(priv->image),base_widget_get_value(self),NULL);
}

static GtkWidget *button_get_child ( GtkWidget *self )
{
  ButtonPrivate *priv;

  g_return_val_if_fail(IS_BUTTON(self),NULL);
  priv = button_get_instance_private(BUTTON(self));

  return priv->button;
}

static GtkWidget *button_mirror ( GtkWidget *src )
{
  g_return_val_if_fail(IS_BUTTON(src),NULL);
  return button_new();
}

static void button_class_init ( ButtonClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->update_value = button_update_value;
  BASE_WIDGET_CLASS(kclass)->get_child = button_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = button_mirror;
}

static void button_init ( Button *self )
{
}

GtkWidget *button_new ( void )
{
  GtkWidget *self;
  ButtonPrivate *priv;

  self = GTK_WIDGET(g_object_new(button_get_type(), NULL));
  priv = button_get_instance_private(BUTTON(self));

  priv->button = gtk_button_new();
  priv->image = scale_image_new();
  gtk_container_add(GTK_CONTAINER(priv->button),priv->image);
  gtk_container_add(GTK_CONTAINER(self),priv->button);

  return self;
}
