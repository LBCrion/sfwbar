#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "basewidget.h"

#define BUTTON_TYPE            (button_get_type())
#define BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BUTTON_TYPE, Button))
#define BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), BUTTON_TYPE, ButtonClass))
#define IS_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BUTTON_TYPE))
#define IS_BUTTONCLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BUTTON_TYPE))

typedef struct _Button Button;
typedef struct _ButtonClass ButtonClass;

struct _Button
{
  BaseWidget item;
};

struct _ButtonClass
{
  BaseWidgetClass parent_class;
};

typedef struct _ButtonPrivate ButtonPrivate;

struct _ButtonPrivate
{
  GtkWidget *button;
  GtkWidget *image;
};

GType button_get_type ( void );

GtkWidget *button_new();

#endif
