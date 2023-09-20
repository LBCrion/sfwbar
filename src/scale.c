/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "basewidget.h"
#include "scale.h"

G_DEFINE_TYPE_WITH_CODE (Scale, scale, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Scale));

static void scale_update_value ( GtkWidget *self )
{
  ScalePrivate *priv;
  gchar *value;

  g_return_if_fail(IS_SCALE(self));
  priv = scale_get_instance_private(SCALE(self));

  value = base_widget_get_value(self);

  if(!g_strrstr(value,"nan"))
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(priv->scale),
          g_ascii_strtod(value,NULL));
}

static GtkWidget *scale_get_child ( GtkWidget *self )
{
  ScalePrivate *priv;

  g_return_val_if_fail(IS_SCALE(self),NULL);
  priv = scale_get_instance_private(SCALE(self));

  return priv->scale;
}

static GtkWidget *scale_mirror ( GtkWidget *src )
{
  GtkWidget *self;

  g_return_val_if_fail(IS_SCALE(src),NULL);
  self = scale_new();
  base_widget_copy_properties(self,src);
  return self;
}

static void scale_style_updated ( GtkWidget *widget, GtkWidget *self )
{
  ScalePrivate *priv;
  gint dir;

  priv = scale_get_instance_private(SCALE(self));
  gtk_widget_style_get(priv->scale,"direction",&dir,NULL);
  if(priv->dir == dir)
    return;
  priv->dir = dir;
  gtk_orientable_set_orientation(GTK_ORIENTABLE(priv->scale),
      (dir==GTK_POS_LEFT || dir==GTK_POS_RIGHT)?
      GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);
  gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(priv->scale),
      ( priv->dir==GTK_POS_TOP || priv->dir==GTK_POS_LEFT ));
}

static void scale_class_init ( ScaleClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->update_value = scale_update_value;
  BASE_WIDGET_CLASS(kclass)->get_child = scale_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = scale_mirror;
}

static void scale_init ( Scale *self )
{
}

GtkWidget *scale_new ( void )
{
  GtkWidget *self;
  ScalePrivate *priv;

  self = GTK_WIDGET(g_object_new(scale_get_type(), NULL));
  priv = scale_get_instance_private(SCALE(self));

  priv->scale = gtk_progress_bar_new();
  gtk_container_add(GTK_CONTAINER(self),priv->scale);
  g_signal_connect(G_OBJECT(priv->scale),"style_updated",
      (GCallback)scale_style_updated,self);

  return self;
}
