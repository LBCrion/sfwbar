/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "image.h"
#include "scaleimage.h"

G_DEFINE_TYPE_WITH_CODE (Image, image, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Image))

static void image_update_value ( GtkWidget *self )
{
  ImagePrivate *priv;

  g_return_if_fail(IS_IMAGE(self));
  priv = image_get_instance_private(IMAGE(self));

  scale_image_set_image(GTK_WIDGET(priv->image),
      base_widget_get_value(self),NULL);
}

static GtkWidget *image_get_child ( GtkWidget *self )
{
  ImagePrivate *priv;

  g_return_val_if_fail(IS_IMAGE(self),NULL);
  priv = image_get_instance_private(IMAGE(self));

  return priv->image;
}

static GtkWidget *image_mirror ( GtkWidget *src )
{
  GtkWidget *self;

  g_return_val_if_fail(IS_IMAGE(src),NULL);
  self = image_new();

  base_widget_copy_properties(self,src);

  return self;
}

static void image_class_init ( ImageClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->update_value = image_update_value;
  BASE_WIDGET_CLASS(kclass)->get_child = image_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = image_mirror;
}

static void image_init ( Image *self )
{
}

GtkWidget *image_new ( void )
{
  GtkWidget *self;
  ImagePrivate *priv;

  self = GTK_WIDGET(g_object_new(image_get_type(), NULL));
  priv = image_get_instance_private(IMAGE(self));

  priv->image = scale_image_new();
  gtk_container_add(GTK_CONTAINER(self),priv->image);

  return self;
}
