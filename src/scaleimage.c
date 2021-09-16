/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021 Lev Babiev
 */


#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

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
  gchar *file;
};

G_DEFINE_TYPE_WITH_CODE (ScaleImage, scale_image, GTK_TYPE_IMAGE, G_ADD_PRIVATE (ScaleImage));

int scale_image_update ( GtkWidget *widget );

void scale_image_resize ( GtkWidget *widget, GdkRectangle *alloc )
{
  ScaleImagePrivate *priv = scale_image_get_instance_private(SCALE_IMAGE(widget));

  printf("%p %s %d %d\n",widget,priv->file,alloc->width,alloc->height);

  if((alloc->width!=priv->w)||(alloc->height!=priv->h))
  {
    priv->w = alloc->width;
    priv->h = alloc->height;
    scale_image_update(widget);
  }

  gtk_widget_set_allocation(widget,alloc);
  GTK_WIDGET_CLASS(scale_image_parent_class)->size_allocate(widget,alloc);
}

static void scale_image_get_preferred_width ( GtkWidget *w, gint *m, gint *n )
{
  GTK_WIDGET_CLASS(scale_image_parent_class)->get_preferred_width(w,m,n);
  *m=1;
}

static void scale_image_get_preferred_height ( GtkWidget *w, gint *m, gint *n )
{
  GTK_WIDGET_CLASS(scale_image_parent_class)->get_preferred_height(w,m,n);
  *m=1;
}

static void scale_image_style ( GtkWidget *widget )
{
  printf("style %p\n",widget);
  GTK_WIDGET_CLASS(scale_image_parent_class)->style_updated(widget);
}

static void scale_image_class_init ( ScaleImageClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->size_allocate = scale_image_resize;
  widget_class->style_updated = scale_image_style;
  widget_class->get_preferred_width = scale_image_get_preferred_width;
  widget_class->get_preferred_height = scale_image_get_preferred_height;
}

static void scale_image_init ( ScaleImage *widget )
{
  ScaleImagePrivate *priv = scale_image_get_instance_private(SCALE_IMAGE(widget));
  priv->w = -1;
  priv->h = -1;
  priv->file = NULL;
}

GtkWidget *scale_image_new()
{
  return GTK_WIDGET(g_object_new(scale_image_get_type(), NULL));
}

void scale_image_set_image ( GtkWidget *widget, gchar *image )
{
  ScaleImagePrivate *priv = scale_image_get_instance_private(SCALE_IMAGE(widget));
  if(g_strcmp0(priv->file,image)==0)
    return;
  g_free(priv->file);
  priv->file = g_strdup(image);
}


int scale_image_update ( GtkWidget *widget )
{
  ScaleImagePrivate *priv = scale_image_get_instance_private(SCALE_IMAGE(widget));
  GtkIconTheme *theme;
  GdkPixbuf *buf=NULL;
  GDesktopAppInfo *app;
  cairo_surface_t *cs;
  int i=0;
  char ***desktop;
  char *iname;
  gint w,h;
  gint size;

  if(priv->file == NULL)
    return -1;

  w = priv->w;
  h = priv->h;
  if(w!=-1)
    w *= gtk_widget_get_scale_factor(widget);
  if(h!=-1)
    h *= gtk_widget_get_scale_factor(widget);

  if(w > h)
    size = h;
  else
    size = w;

  theme = gtk_icon_theme_get_default();
  if(theme)
    buf = gtk_icon_theme_load_icon(theme,priv->file,size,0,NULL);

  if(buf==NULL)
  {
    desktop = g_desktop_app_info_search(priv->file);
    if(*desktop)
    {
      if(*desktop[0])
        while(desktop[0][i])
        {
          app = g_desktop_app_info_new(desktop[0][i]);
          if(app)
            if(!g_desktop_app_info_get_nodisplay(app))
            {
              iname = (char *)g_desktop_app_info_get_string(app,"Icon");
              g_object_unref(G_OBJECT(app));
              if(iname)
              {
                buf = gtk_icon_theme_load_icon(theme,iname,size,0,NULL);
                g_free(iname);
              }
            }
          i++;
        }
      i=0;
      while(desktop[i])
        g_strfreev(desktop[i++]);
      g_free(desktop);
    }
  }

  if(buf==NULL)
    if(g_file_test(priv->file,G_FILE_TEST_EXISTS))
      buf = gdk_pixbuf_new_from_file_at_scale(priv->file,w,h,TRUE,NULL);

  if(buf==NULL)
    return -1;

  cs = gdk_cairo_surface_create_from_pixbuf(buf,0,gtk_widget_get_window(widget));
  if(cs != NULL)
  {
    gtk_image_set_from_surface(GTK_IMAGE(widget), cs);
    cairo_surface_destroy(cs); 
  }

  g_object_unref(G_OBJECT(buf));
  return 0;
}
