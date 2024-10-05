#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "basewidget.h"

#define IMAGE_TYPE            (image_get_type())
#define IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGE_TYPE, Image))
#define IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), IMAGE_TYPE, ImageClass))
#define IS_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGE_TYPE))
#define IS_IMAGECLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), IMAGE_TYPE))

typedef struct _Image Image;
typedef struct _ImageClass ImageClass;

struct _Image
{
  BaseWidget item;
};

struct _ImageClass
{
  BaseWidgetClass parent_class;
};

typedef struct _ImagePrivate ImagePrivate;

struct _ImagePrivate
{
  GtkWidget *image;
};

GType image_get_type ( void );

#endif
