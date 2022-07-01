/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "cchart.h"
#include "chart.h"

G_DEFINE_TYPE_WITH_CODE (CChart, cchart, BASE_WIDGET_TYPE, G_ADD_PRIVATE (CChart));

static void cchart_destroy ( GtkWidget *self )
{
}

static void cchart_update_value ( GtkWidget *self )
{
  CChartPrivate *priv;
  gchar *value;

  g_return_if_fail(IS_CCHART(self));
  priv = cchart_get_instance_private(CCHART(self));

  value = base_widget_get_value(self);

  if(!g_strrstr(value,"nan"))
      chart_update(priv->chart,g_ascii_strtod(value,NULL));

}

static GtkWidget *cchart_get_child ( GtkWidget *self )
{
  CChartPrivate *priv;

  g_return_val_if_fail(IS_CCHART(self),NULL);
  priv = cchart_get_instance_private(CCHART(self));

  return priv->chart;
}

static void cchart_class_init ( CChartClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = cchart_destroy;
  BASE_WIDGET_CLASS(kclass)->update_value = cchart_update_value;
  BASE_WIDGET_CLASS(kclass)->get_child = cchart_get_child;
}

static void cchart_init ( CChart *self )
{
}

GtkWidget *cchart_new ( void )
{
  GtkWidget *self;
  CChartPrivate *priv;

  self = GTK_WIDGET(g_object_new(cchart_get_type(), NULL));
  priv = cchart_get_instance_private(CCHART(self));

  priv->chart = chart_new();
  gtk_container_add(GTK_CONTAINER(self),priv->chart);

  return self;
}
