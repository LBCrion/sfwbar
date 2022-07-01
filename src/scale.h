#ifndef __SCALE_H__
#define __SCALE_H__

#include "basewidget.h"

#define SCALE_TYPE            (scale_get_type())
#define SCALE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SCALE_TYPE, Scale))
#define SCALE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SCALE_TYPE, ScaleClass))
#define IS_SCALE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SCALE_TYPE))
#define IS_SCALECLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SCALE_TYPE))

typedef struct _Scale Scale;
typedef struct _ScaleClass ScaleClass;

struct _Scale
{
  BaseWidget item;
};

struct _ScaleClass
{
  BaseWidgetClass parent_class;
};

typedef struct _ScalePrivate ScalePrivate;

struct _ScalePrivate
{
  GtkWidget *scale;
  gint dir;
};

GType scale_get_type ( void );

GtkWidget *scale_new();

#endif
