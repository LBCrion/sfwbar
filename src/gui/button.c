/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "gui/button.h"
#include "gui/scaleimage.h"

G_DEFINE_TYPE_WITH_CODE (Button, button, BASE_WIDGET_TYPE, G_ADD_PRIVATE (Button))

enum {
  BUTTON_WTYPE = 1,
  BUTTON_N_PROPERTIES
};

GEnumValue button_types[] = {
  { FALSE, "image", "image" },
  { TRUE, "label", "label" },
  { 0, NULL, NULL },
};

static void button_update_value ( GtkWidget *self )
{
  ButtonPrivate *priv;

  g_return_if_fail(IS_BUTTON(self));
  priv = button_get_instance_private(BUTTON(self));

  if(GTK_IS_LABEL(priv->widget))
    gtk_label_set_markup(GTK_LABEL(priv->widget), base_widget_get_value(self));
  else
    scale_image_set_image(GTK_WIDGET(priv->widget),
        base_widget_get_value(self), NULL);
}

static void button_get_property ( GObject *self, guint id, GValue *value,
    GParamSpec *spec )
{
  ButtonPrivate *priv;

  priv = button_get_instance_private(BUTTON(self));

  if(id == BUTTON_WTYPE)
    g_value_set_enum(value, GTK_IS_LABEL(priv->widget));
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
}

static void button_set_property ( GObject *self, guint id,
    const GValue *value, GParamSpec *spec )
{
  ButtonPrivate *priv;

  priv = button_get_instance_private(BUTTON(self));

  if(id == BUTTON_WTYPE && g_value_get_enum(value) != GTK_IS_LABEL(priv->widget))
  {
    gtk_widget_destroy(priv->widget);
    priv->widget = g_value_get_enum(value)? gtk_label_new("") :
      scale_image_new();
    gtk_container_add(GTK_CONTAINER(priv->button), priv->widget);
    gtk_widget_show(priv->widget);
  }
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(self, id, spec);
}

static void button_mirror ( GtkWidget *dest, GtkWidget *src )
{
  BASE_WIDGET_CLASS(button_parent_class)->mirror(dest, src);

  g_object_bind_property(G_OBJECT(src), "type",
      G_OBJECT(dest), "type", G_BINDING_SYNC_CREATE);
}

static void button_class_init ( ButtonClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->update_value = button_update_value;
  BASE_WIDGET_CLASS(kclass)->mirror = button_mirror;
  G_OBJECT_CLASS(kclass)->get_property = button_get_property;
  G_OBJECT_CLASS(kclass)->set_property = button_set_property;

  g_object_class_install_property(G_OBJECT_CLASS(kclass), BUTTON_WTYPE,
      g_param_spec_enum("type", "button type", "sfwbar_config",
        g_enum_register_static("button_types", button_types),
        FALSE, G_PARAM_READWRITE));
}

static void button_init ( Button *self )
{
  ButtonPrivate *priv;

  priv = button_get_instance_private(BUTTON(self));

  priv->button = gtk_button_new();
  priv->widget = scale_image_new();
  gtk_container_add(GTK_CONTAINER(priv->button), priv->widget);
  gtk_container_add(GTK_CONTAINER(self), priv->button);
}
