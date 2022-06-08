#ifndef __SCALEIMAGE_H__
#define __SCALEIMAGE_H__

#include <gtk/gtk.h>

#define SCALE_IMAGE_TYPE            (scale_image_get_type())
#define SCALE_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SCALE_IMAGE_TYPE, ScaleImage))
#define SCALE_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SCALE_IMAGE_TYPE, ScaleImageClass))
#define IS_SCALE_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SCALE_IMAGE_TYPE))
#define IS_SCALE_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SCALE_IMAGE_TYPE))

typedef struct _ScaleImage ScaleImage;
typedef struct _ScaleImageClass ScaleImageClass;

struct _ScaleImage
{
  GtkImage parent_class;
};

struct _ScaleImageClass
{
  GtkImageClass parent_class;
};

typedef struct _ScaleImagePrivate ScaleImagePrivate;

struct _ScaleImagePrivate
{
  gint w,h;
  gint ftype;
  gchar *file;
  gchar *fname;
  GdkPixbuf *pixbuf;
};

enum {
  SI_NONE,
  SI_ICON,
  SI_FILE,
  SI_BUFF
};

GType scale_image_get_type ( void );

#endif
