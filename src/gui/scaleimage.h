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
  gint ftype;
  gint width, height;
  gint radius, shadow_dx, shadow_dy;
  GdkRGBA *color;
  GdkRGBA *shadow_color;
  gboolean shadow_clip;
  gboolean fallback;
  gboolean symbolic;
  gboolean symbolic_pref;
  gchar *file;
  gchar *extra;
  gchar *fname;
  GdkPixbuf *pixbuf;
  cairo_surface_t *cs, *shadow;
};

enum {
  SI_NONE,
  SI_ICON,
  SI_FILE,
  SI_BUFF,
  SI_DATA
};

GType scale_image_get_type ( void );
gboolean scale_image_set_image ( GtkWidget *, const gchar *, gchar *);
GtkWidget *scale_image_new();
int scale_image_update ( GtkWidget *widget );
gboolean scale_image_cache_insert ( gchar *name, GdkPixbuf *pb );
gboolean scale_image_cache_remove ( gchar *name );

#endif
