/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "chart.h"
#include "basewidget.h"

G_DEFINE_TYPE_WITH_CODE (Chart, chart, GTK_TYPE_BOX, G_ADD_PRIVATE (Chart))

static void chart_destroy ( GtkWidget *self )
{
  ChartPrivate *priv;
  
  g_return_if_fail(IS_CHART(self));
  priv = chart_get_instance_private(CHART(self));

  g_queue_free(priv->data);
  GTK_WIDGET_CLASS(chart_parent_class)->destroy(self);
}

static gboolean chart_draw ( GtkWidget *self, cairo_t *cr )
{
  ChartPrivate *priv;
  GtkStyleContext *context;
  gint width,height;
  GtkBorder border,margin,padding,extents;
  GtkStateFlags flags;
  GdkRGBA fg;
  gdouble x_offset, y_offset;
  gint i, len;

  g_return_val_if_fail(IS_CHART(self),FALSE);
  priv = chart_get_instance_private(CHART(self));

  width = gtk_widget_get_allocated_width(self);
  height = gtk_widget_get_allocated_height(self);

  context = gtk_widget_get_style_context(self);
  flags = gtk_style_context_get_state(context);
  gtk_style_context_get_border(context,flags,&border);
  gtk_style_context_get_padding(context,flags,&padding);
  gtk_style_context_get_margin(context,flags,&margin);
  extents.left = border.left + margin.left + padding.left;
  extents.right = border.right + margin.right + padding.right;
  extents.top = border.top + margin.top + padding.top;
  extents.bottom = border.bottom + margin.bottom + padding.bottom;

  gtk_render_background(context,cr,extents.left,extents.top, width -
      extents.left - extents.right, height - extents.top - extents.bottom);
  gtk_render_frame(context,cr,margin.left, margin.top, width - margin.left -
      margin.right, height - margin.top - margin.bottom);

  width = width - extents.left - extents.right;
  height = height - extents.top - extents.bottom - 1;
  if( width<1 || height<1 )
    return FALSE;

  while(g_queue_get_length(priv->data)>width)
    g_free(g_queue_pop_head(priv->data));

  len = g_queue_get_length(priv->data);

  x_offset = width + extents.left - len + 0.5;
  y_offset = height + extents.top + 0.5;

  gtk_style_context_get_color (context,flags, &fg);
  cairo_set_source_rgba(cr,fg.red,fg.green,fg.blue,fg.alpha);
  cairo_set_line_width(cr,1);
  cairo_move_to(cr,x_offset,y_offset);
  for(i=0;i<len;i++)
    cairo_line_to(cr, x_offset + i, y_offset -
        height * *(gdouble *)g_queue_peek_nth(priv->data,i));
  cairo_line_to(cr,x_offset + len - 1, y_offset);
  cairo_close_path(cr);
  cairo_stroke_preserve(cr);
  cairo_fill(cr);

  return TRUE;
}

static void chart_class_init ( ChartClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(kclass),"chart");
  widget_class->destroy = chart_destroy;
  widget_class->draw = chart_draw;
}

static void chart_init ( Chart *self )
{
  ChartPrivate *priv;

  g_return_if_fail(IS_CHART(self));
  priv = chart_get_instance_private(CHART(self));

  priv->data = g_queue_new();
}

GtkWidget *chart_new( void )
{
  return GTK_WIDGET(g_object_new(chart_get_type(), NULL));
}

int chart_update ( GtkWidget *self, gdouble n )
{
  ChartPrivate *priv;

  g_return_val_if_fail(IS_CHART(self),0);
  priv = chart_get_instance_private(CHART(self));

  g_queue_push_tail(priv->data,g_memdup(&n,sizeof(double)));
  gtk_widget_queue_draw(self);

  return 0;
}
