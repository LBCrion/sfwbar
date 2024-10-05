#ifndef __CCHART_H__
#define __CCHART_H__

#include "basewidget.h"

#define CCHART_TYPE            (cchart_get_type())
#define CCHART(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), CCHART_TYPE, CChart))
#define CCHART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CCHART_TYPE, CChartClass))
#define IS_CCHART(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CCHART_TYPE))
#define IS_CCHARTCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CCHART_TYPE))

typedef struct _CChart CChart;
typedef struct _CChartClass CChartClass;

struct _CChart
{
  BaseWidget item;
};

struct _CChartClass
{
  BaseWidgetClass parent_class;
};

typedef struct _CChartPrivate CChartPrivate;

struct _CChartPrivate
{
  GtkWidget *chart;
};

GType cchart_get_type ( void );

#endif
