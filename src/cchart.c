/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "cchart.h"
#include "chart.h"

G_DEFINE_TYPE_WITH_CODE (CChart, cchart, BASE_WIDGET_TYPE,
    G_ADD_PRIVATE (CChart))

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

static GtkWidget *cchart_mirror ( GtkWidget *src )
{
  g_return_val_if_fail(IS_CCHART(src), NULL);
  return cchart_new();
}

static void cchart_class_init ( CChartClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->update_value = cchart_update_value;
  BASE_WIDGET_CLASS(kclass)->mirror = cchart_mirror;
}

static void cchart_init ( CChart *self )
{
  CChartPrivate *priv;

  priv = cchart_get_instance_private(CCHART(self));

  base_widget_set_always_update(GTK_WIDGET(self), TRUE);
  priv->chart = chart_new();
  gtk_container_add(GTK_CONTAINER(self),priv->chart);
}

GtkWidget *cchart_new ( void )
{
  return GTK_WIDGET(g_object_new(cchart_get_type(), NULL));
}
