#ifndef __CHART_H__
#define __CHART_H__

#include "basewidget.h"

#define CHART_TYPE            (chart_get_type())
#define CHART(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), CHART_TYPE, Chart))
#define CHART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CHART_TYPE, ChartClass))
#define IS_CHART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CHART_TYPE))
#define IS_CHART_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CHART_TYPE))

typedef struct _Chart Chart;
typedef struct _ChartClass ChartClass;

struct _Chart
{
  GtkBox parent_class;
};

struct _ChartClass
{
  GtkBoxClass parent_class;
};

typedef struct _ChartPrivate ChartPrivate;

struct _ChartPrivate
{
  GQueue *data;
  GtkWidget *chart;
};

GType chart_get_type ( void );

GtkWidget *chart_new( void );
int chart_update ( GtkWidget *widget, gdouble n );

#endif
