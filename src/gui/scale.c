/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "basewidget.h"
#include "scale.h"

G_DEFINE_TYPE_WITH_CODE (Scale, scale, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Scale))

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
}

static void scale_init ( Scale *self )
{
  ScalePrivate *priv;

  priv = scale_get_instance_private(SCALE(self));

  priv->scale = gtk_progress_bar_new();
  gtk_container_add(GTK_CONTAINER(self), priv->scale);
  g_signal_connect(G_OBJECT(priv->scale), "style_updated",
      (GCallback)scale_style_updated, self);
}
