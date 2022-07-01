#ifndef __LABEL_H__
#define __LABEL_H__

#include "basewidget.h"

#define LABEL_TYPE            (label_get_type())
#define LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), LABEL_TYPE, Label))
#define LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), LABEL_TYPE, LabelClass))
#define IS_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), LABEL_TYPE))
#define IS_LABELCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), LABEL_TYPE))

typedef struct _Label Label;
typedef struct _LabelClass LabelClass;

struct _Label
{
  BaseWidget item;
};

struct _LabelClass
{
  BaseWidgetClass parent_class;
};

typedef struct _LabelPrivate LabelPrivate;

struct _LabelPrivate
{
  GtkWidget *label;
};

GType label_get_type ( void );

GtkWidget *label_new();

#endif
