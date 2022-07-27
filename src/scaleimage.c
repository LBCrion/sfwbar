/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 sfwbar maintainers
 */

#include "sfwbar.h"
#include "scaleimage.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

G_DEFINE_TYPE_WITH_CODE (ScaleImage, scale_image, GTK_TYPE_IMAGE, G_ADD_PRIVATE (ScaleImage));

int scale_image_update ( GtkWidget *self );

void scale_image_resize ( GtkWidget *self, GdkRectangle *alloc )
{
  gint w,h;
  ScaleImagePrivate *priv;
  GtkBorder border,padding;
  GtkStyleContext *style;
  GtkStateFlags flags;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  style = gtk_widget_get_style_context(self);
  flags = gtk_style_context_get_state(style);

  gtk_style_context_get_border(style, flags, &border );
  gtk_style_context_get_padding(style, flags, &padding );

  w = alloc->width-border.left-border.right-padding.left-padding.right;
  h = alloc->height-border.top-border.bottom-padding.top-padding.bottom;

  if((w!=priv->w)||(h!=priv->h))
  {
    priv->w = w;
    priv->h = h;
    scale_image_update(self);
  }

  gtk_widget_set_allocation(self,alloc);
  GTK_WIDGET_CLASS(scale_image_parent_class)->size_allocate(self,alloc);
}

static void scale_image_get_preferred_width ( GtkWidget *w, gint *m, gint *n )
{
  GtkBorder border,padding;
  GtkStyleContext *style = gtk_widget_get_style_context(w);
  GtkStateFlags flags = gtk_style_context_get_state(style);
  gboolean expand;

  gtk_widget_style_get(w,"hexpand",&expand,NULL);
  gtk_style_context_get_border(style, flags, &border );
  gtk_style_context_get_padding(style, flags, &padding );
  GTK_WIDGET_CLASS(scale_image_parent_class)->get_preferred_width(w,m,n);
  if(expand)
    *n += border.left + border.right + padding.left + padding.right;
  *m=1;
}

static void scale_image_get_preferred_height ( GtkWidget *w, gint *m, gint *n )
{
  GtkBorder border,padding;
  GtkStyleContext *style = gtk_widget_get_style_context(w);
  GtkStateFlags flags = gtk_style_context_get_state(style);
  gboolean expand;

  gtk_widget_style_get(w,"hexpand",&expand,NULL);

  gtk_style_context_get_border(style, flags, &border );
  gtk_style_context_get_padding(style, flags, &padding );
  GTK_WIDGET_CLASS(scale_image_parent_class)->get_preferred_height(w,m,n);
  if(expand)
    *n += border.top + border.bottom + padding.top + padding.bottom;
  *m=1;
}

static void scale_image_destroy ( GtkWidget *self )
{
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  g_free(priv->file);
  priv->file=NULL;
  g_free(priv->fname);
  priv->fname=NULL;
  GTK_WIDGET_CLASS(scale_image_parent_class)->destroy(self);
}

static void scale_image_map ( GtkWidget *w )
{
  scale_image_update(w);
  GTK_WIDGET_CLASS(scale_image_parent_class)->map(w);
}

static void scale_image_class_init ( ScaleImageClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->size_allocate = scale_image_resize;
  widget_class->destroy = scale_image_destroy;
  widget_class->map = scale_image_map;
  widget_class->get_preferred_width = scale_image_get_preferred_width;
  widget_class->get_preferred_height = scale_image_get_preferred_height;

  gtk_widget_class_install_style_property( widget_class,
    g_param_spec_boxed("color","image color",
      "draw image in this color using it's alpha channel as a mask",
      GDK_TYPE_RGBA,G_PARAM_READABLE));
}

static void scale_image_init ( ScaleImage *self )
{
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  priv->w = -1;
  priv->h = -1;
  priv->file = NULL;
  priv->fname = NULL;
  priv->pixbuf = NULL;
  priv->ftype = SI_NONE;
}

GtkWidget *scale_image_new()
{
  return GTK_WIDGET(g_object_new(scale_image_get_type(), NULL));
}

void scale_image_set_pixbuf ( GtkWidget *widget, GdkPixbuf *pb )
{
  ScaleImagePrivate *priv = scale_image_get_instance_private(
      SCALE_IMAGE(widget));
  priv->pixbuf = pb;
  priv->ftype = SI_BUFF;
}

static void scale_image_load_icon ( GtkWidget *self, gchar *icon )
{
  GdkPixbuf *buf;
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  if(!icon)
    return;
  buf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),icon,10,0,NULL);
  if(buf)
  {
    g_free(priv->fname);
    priv->fname = icon;
    priv->ftype = SI_ICON;
    g_object_unref(G_OBJECT(buf));
  }
  else
    g_free(icon);
}

static void scale_image_check_appinfo ( GtkWidget *self, gchar *icon )
{
  ScaleImagePrivate *priv;
  GDesktopAppInfo *app;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  app = g_desktop_app_info_new(icon);
  if(!app)
    return;
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  if((!g_desktop_app_info_get_nodisplay(app))&&(priv->ftype==SI_NONE))
    scale_image_load_icon(self,g_desktop_app_info_get_string(app,"Icon"));
  g_object_unref(G_OBJECT(app));
}

static void scale_image_check_icon ( GtkWidget *self, gchar *file )
{
  gint i,j;
  gchar *temp;
  gchar ***desktop;
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  scale_image_load_icon(self,g_strdup(file));
  if(priv->ftype!=SI_NONE)
    return;

  desktop = g_desktop_app_info_search(file);
  for(j=0;desktop[j];j++)
  {
    for(i=0;desktop[j][i];i++)
      scale_image_check_appinfo(self,desktop[j][i]);
    g_strfreev(desktop[j]);
  }
  g_free(desktop);
  
  if(priv->ftype!=SI_NONE)
    return;
  temp = g_strconcat(file,".desktop",NULL);
  scale_image_check_appinfo(self,temp);
  g_free(temp);
}

void scale_image_set_image ( GtkWidget *self, gchar *image, gchar *extra )
{
  static gchar *exts[4] = {"", ".svg", ".png", ".xpm"};
  ScaleImagePrivate *priv;
  GdkPixbuf *buf;
  gint i;
  gchar *temp,*test;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private( SCALE_IMAGE(self));

  if( !g_strcmp0(priv->file,image) && !g_strcmp0(priv->extra,extra) )
    return;
  g_free(priv->file);
  priv->file = g_strdup(image);
  g_free(priv->extra);
  priv->extra = g_strdup(extra);
  priv->ftype = SI_NONE;

  if(!g_ascii_strncasecmp(priv->file,"<?xml",5))
  {
    priv->ftype = SI_DATA;
    return;
  }

  scale_image_check_icon(self,image);
  if(priv->ftype == SI_ICON)
    return;

  temp = g_ascii_strdown(image,-1);
  scale_image_check_icon(self,temp);
  g_free(temp);
  if(priv->ftype == SI_ICON)
    return;

  for(i=0;i<4;i++)
  {
    test = g_strconcat(priv->file,exts[i],NULL);
    temp = get_xdg_config_file(test,extra);
    g_free(test);
    if(temp)
    {
      buf = gdk_pixbuf_new_from_file_at_scale(temp,10,10,TRUE,NULL);
      if(buf)
      {
        g_object_unref(G_OBJECT(buf));
        g_free(priv->fname);
        priv->fname = temp;
        priv->ftype = SI_FILE;
        break;
      }
      else
        g_free(temp);
    }
  }
}

int scale_image_update ( GtkWidget *self )
{
  ScaleImagePrivate *priv;
  GtkIconTheme *theme;
  GdkPixbuf *buf = NULL, *tmp = NULL;
  GdkPixbufLoader *loader = NULL;
  GdkRGBA *color = NULL;
  cairo_surface_t *cs;
  cairo_t *cr;
  gchar *fallback;
  gint w,h;
  gint size;

  g_return_val_if_fail(IS_SCALE_IMAGE(self),-1);
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  w = priv->w * gtk_widget_get_scale_factor(self);
  h = priv->h * gtk_widget_get_scale_factor(self);
  size = MIN(w,h);

  if(priv->file)
    g_debug("image: %s @ %d",priv->ftype==SI_DATA?"svg":priv->file,size);

  if(size<1)
    return -1;

  if(priv->ftype == SI_ICON && priv->fname)
  {
    theme = gtk_icon_theme_get_default();
    if(theme)
    {
      tmp = gtk_icon_theme_load_icon(theme,priv->fname,size,0,NULL);

      if(tmp)
      {
        buf = gdk_pixbuf_scale_simple(tmp,size,size, GDK_INTERP_BILINEAR);
        g_object_unref(G_OBJECT(tmp));
      }
    }
  }

  else if(priv->ftype == SI_FILE && priv->fname)
    buf = gdk_pixbuf_new_from_file_at_scale(priv->fname,size,size,TRUE,NULL);

  else if(priv->ftype == SI_BUFF && priv->pixbuf)
    buf = gdk_pixbuf_scale_simple(priv->pixbuf,size,size, GDK_INTERP_BILINEAR);

  else if (priv->ftype == SI_DATA && priv->file)
  {
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_set_size(loader,size,size);
    gdk_pixbuf_loader_write(loader,(guchar *)priv->file,
        strlen(priv->file), NULL);
    gdk_pixbuf_loader_close(loader,NULL);
    buf = gdk_pixbuf_copy(gdk_pixbuf_loader_get_pixbuf(loader));
    g_object_unref(G_OBJECT(loader));
  }

  if(!buf)
  {
    fallback = get_xdg_config_file("icons/misc/missing.svg",NULL);
    if(fallback)
    {
      buf = gdk_pixbuf_new_from_file_at_scale(fallback,size,size,TRUE,NULL);
      g_free(fallback);
    }
  }

  if(!buf)
    return -1;

  cs = gdk_cairo_surface_create_from_pixbuf(buf,0,
      gtk_widget_get_window(self));

  gtk_widget_style_get(self,"color",&color,NULL);
  if(color)
  {
    cr = cairo_create(cs);
    cairo_set_source_rgba(cr,color->red,color->green,color->blue,color->alpha);
    cairo_mask_surface(cr,cs,0,0);
    cairo_destroy(cr);
  }

  if(cs != NULL)
  {
    gtk_image_set_from_surface(GTK_IMAGE(self), cs);
    cairo_surface_destroy(cs); 
  }

  g_object_unref(G_OBJECT(buf));
  return 0;
}
