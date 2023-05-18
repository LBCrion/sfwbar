/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
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

static GtkWidget *cchart_mirror ( GtkWidget *src )
{
  GtkWidget *self;

  g_return_val_if_fail(IS_CCHART(src),NULL);
  self = cchart_new();
  base_widget_copy_properties(self,src);

  return self;
}

static void cchart_class_init ( CChartClass *kclass )
{
  GTK_WIDGET_CLASS(kclass)->destroy = cchart_destroy;
  BASE_WIDGET_CLASS(kclass)->update_value = cchart_update_value;
  BASE_WIDGET_CLASS(kclass)->get_child = cchart_get_child;
  BASE_WIDGET_CLASS(kclass)->mirror = cchart_mirror;
}

static void cchart_init ( CChart *self )
{
  base_widget_set_always_update(GTK_WIDGET(self),TRUE);
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
