/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020- sfwbar maintainers
 */

#include "sfwbar.h"
#include "scaleimage.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

G_DEFINE_TYPE_WITH_CODE (ScaleImage, scale_image, GTK_TYPE_IMAGE,
    G_ADD_PRIVATE (ScaleImage))

static void scale_image_get_preferred_width ( GtkWidget *self, gint *m,
    gint *n )
{
  GtkStyleContext *style;
  GtkStateFlags flags;
  GtkBorder border, padding, margin;
  gint w;

  g_return_if_fail(IS_SCALE_IMAGE(self));

  style = gtk_widget_get_style_context(self);
  flags = gtk_style_context_get_state(style);
  gtk_style_context_get_border(style,flags,&border);
  gtk_style_context_get_padding(style,flags,&padding);
  gtk_style_context_get_margin(style,flags,&margin);
  gtk_style_context_get(style,flags,"min-width",&w,NULL);

  *m = (w?w:16) + border.left + border.right + padding.left +
      padding.right + margin.left + margin.right;
  *n = *m;
}

static void scale_image_get_preferred_height ( GtkWidget *self, gint *m,
    gint *n )
{
  GtkStyleContext *style;
  GtkStateFlags flags;
  GtkBorder border, padding, margin;
  gint h;

  g_return_if_fail(IS_SCALE_IMAGE(self));

  style = gtk_widget_get_style_context(self);
  flags = gtk_style_context_get_state(style);
  gtk_style_context_get_border(style,flags,&border);
  gtk_style_context_get_padding(style,flags,&padding);
  gtk_style_context_get_margin(style,flags,&margin);
  gtk_style_context_get(style,flags,"min-height",&h,NULL);

  *m = (h?h:16) + border.top + border.bottom + padding.top +
      padding.bottom + margin.top + margin.bottom;
  *n = *m;
}

static void scale_image_surface_update ( GtkWidget *self, gint width,
    gint height )
{
  ScaleImagePrivate *priv;
  GdkPixbuf *buf, *tmp;
  GdkPixbufLoader *loader;
  gchar *fallback;
  gboolean aspect;

  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  priv->fallback = FALSE;

  if(priv->ftype == SI_ICON)
    buf =  gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
        priv->fname,MIN(width,height),0,NULL);

  else if(priv->ftype == SI_FILE && priv->fname)
    buf = gdk_pixbuf_new_from_file_at_scale(priv->fname,width,height,TRUE,NULL);

  else if(priv->ftype == SI_BUFF && priv->pixbuf)
    buf = g_object_ref(priv->pixbuf);

  else if (priv->ftype == SI_DATA && priv->file)
  {
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_set_size(loader,width,height);
    GdkRGBA col;
    gtk_style_context_get_color(gtk_widget_get_style_context(self),
        GTK_STATE_FLAG_NORMAL,&col);
    gchar *svg;
    gchar *rgba;
    if(strstr(priv->file,"@theme_fg_color"))
    {
      rgba = g_strdup_printf("Rgba(%d,%d,%d,%f)",(gint)(col.red*256),
          (gint)(col.green*256),(gint)(col.blue*256),col.alpha);
      svg = str_replace(priv->file,"@theme_fg_color",rgba);
      g_free(rgba);
    }
    else
      svg = NULL;
    gdk_pixbuf_loader_write(loader,(guchar *)(svg?svg:priv->file),
        strlen(svg?svg:priv->file), NULL);
    gdk_pixbuf_loader_close(loader,NULL);
    buf = gdk_pixbuf_loader_get_pixbuf(loader);
    if(buf)
      buf = gdk_pixbuf_copy(buf);
    g_object_unref(G_OBJECT(loader));
    g_free(svg);
  }
  else
    buf = NULL;

  if(!buf)
  {
    fallback = get_xdg_config_file("icons/misc/missing.svg",NULL);
    if(fallback)
    {
      buf = gdk_pixbuf_new_from_file_at_scale(fallback, width, height,
          TRUE, NULL);
      g_free(fallback);
      priv->fallback = TRUE;
    }
  }

  if(buf)
  {
    aspect = (gboolean)gdk_pixbuf_get_width(buf) /
      (gboolean)gdk_pixbuf_get_height(buf);

    if((gboolean)width/(gboolean)height > aspect)
      width = (gboolean)height * aspect;
    else if((gboolean)width/(gboolean)height < aspect)
      height = (gboolean)width / aspect;
  }

  if(buf && gdk_pixbuf_get_width(buf) != width &&
      gdk_pixbuf_get_height(buf) != height )
  {
    tmp = buf;
    buf = gdk_pixbuf_scale_simple(tmp, width, height, GDK_INTERP_BILINEAR);
    g_object_unref(G_OBJECT(tmp));
  }

  cairo_surface_destroy(priv->cs); 
  if(!buf)
  {
    priv->cs = NULL;
    return;
  }

  priv->width = width;
  priv->height = height;
  priv->cs = gdk_cairo_surface_create_from_pixbuf(buf,0,
      gtk_widget_get_window(self));
  g_object_unref(G_OBJECT(buf));
}

static gboolean scale_image_draw ( GtkWidget *self, cairo_t *cr )
{
  ScaleImagePrivate *priv;
  GdkRGBA col, *color = NULL;
  GtkStyleContext *style;
  GtkStateFlags flags;
  GtkBorder border, padding, margin;
  gint width, height, scale;
  gdouble x_origin, y_origin;

  g_return_val_if_fail(IS_SCALE_IMAGE(self),-1);
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  style = gtk_widget_get_style_context(self);
  flags = gtk_style_context_get_state(style);
  gtk_style_context_get_border(style,flags,&border);
  gtk_style_context_get_padding(style,flags,&padding);
  gtk_style_context_get_margin(style,flags,&margin);

  width = gtk_widget_get_allocated_width(self);
  height = gtk_widget_get_allocated_height(self);

  gtk_render_background(style,cr,margin.left, margin.top, width - margin.left -
      margin.right, height - margin.top - margin.bottom);
  gtk_render_frame(style,cr,margin.left, margin.top, width - margin.left -
      margin.right, height - margin.top - margin.bottom);
  scale = gtk_widget_get_scale_factor(self);

  width = (width - border.left - border.right - padding.left
    - padding.right - margin.left - margin.right) * scale;
  height = (height - border.top - border.bottom - padding.top
    - padding.bottom - margin.top - margin.bottom) * scale;

  if( width < 1 || height < 1 )
    return FALSE;

  if(!priv->cs || priv->width != width || priv->height != height )
    scale_image_surface_update(self,width,height);

  if(!priv->cs)
    return FALSE;

  if(priv->file)
    g_debug("image: %s @ %d x %d",priv->ftype==SI_DATA?"svg":priv->file,
        width, height );

  x_origin = margin.left + padding.left + border.left +
    (width - cairo_image_surface_get_width(priv->cs))/(2*scale);
  y_origin = margin.top + padding.top + border.top +
    (height - cairo_image_surface_get_height(priv->cs))/(2*scale);

  gtk_widget_style_get(self, "color", &color, NULL);
  if(!color && (priv->symbolic || priv->fallback))
  {
    gtk_style_context_get_color(gtk_widget_get_style_context(self),
        GTK_STATE_FLAG_NORMAL, &col);
    col.alpha = 1.0;
    color = gdk_rgba_copy(&col);
  }

  if(color)
  {
    cairo_set_source_rgba(cr,color->red,color->green,color->blue,color->alpha);
    cairo_mask_surface(cr, priv->cs, x_origin, y_origin);
    gdk_rgba_free(color);
  }
  else if(priv->cs)
  {
    cairo_set_source_surface(cr,priv->cs, x_origin, y_origin);
    cairo_paint(cr);
  }

  return TRUE;
}

static void scale_image_clear ( GtkWidget *self )
{
  ScaleImagePrivate *priv;

  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  g_clear_pointer(&priv->fname,g_free);
  g_clear_pointer(&priv->file,g_free);
  g_clear_pointer(&priv->extra,g_free);
  g_clear_pointer(&priv->pixbuf,g_object_unref);
  g_clear_pointer(&priv->cs,cairo_surface_destroy);
  priv->ftype = SI_NONE;
}

static void scale_image_destroy ( GtkWidget *self )
{
  g_return_if_fail(IS_SCALE_IMAGE(self));

  scale_image_clear(self);
  GTK_WIDGET_CLASS(scale_image_parent_class)->destroy(self);
}

static void scale_image_style_updated ( GtkWidget *self )
{
  ScaleImagePrivate *priv;
  gboolean prefer_symbolic;
  gchar *image, *extra;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  gtk_widget_style_get(self, "symbolic", &prefer_symbolic, NULL);
  if(priv->symbolic_pref != prefer_symbolic && priv->ftype == SI_ICON)
  {
    image = g_strdup(priv->file);
    extra = g_strdup(priv->extra);
    scale_image_clear(self);
    scale_image_set_image(self, image, extra);
    g_free(image);
    g_free(extra);
  }
  GTK_WIDGET_CLASS(scale_image_parent_class)->style_updated(self);
}

static void scale_image_class_init ( ScaleImageClass *kclass )
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(kclass);
  widget_class->destroy = scale_image_destroy;
  widget_class->draw = scale_image_draw;
  widget_class->get_preferred_width = scale_image_get_preferred_width;
  widget_class->get_preferred_height = scale_image_get_preferred_height;
  widget_class->style_updated = scale_image_style_updated;

  gtk_widget_class_install_style_property( widget_class,
      g_param_spec_boxed("color","image color",
        "draw image in this color using it's alpha channel as a mask",
        GDK_TYPE_RGBA,G_PARAM_READABLE));
  gtk_widget_class_install_style_property( widget_class,
      g_param_spec_boolean("symbolic","symbolic icon",
        "treat image as a symbolic icon and apply theme specific color",
        FALSE,G_PARAM_READABLE));
}

static void scale_image_init ( ScaleImage *self )
{
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  priv->file = NULL;
  priv->fname = NULL;
  priv->pixbuf = NULL;
  priv->cs = NULL;
  priv->width = 0;
  priv->height = 0;
  priv->fallback = FALSE;
  priv->ftype = SI_NONE;
}

GtkWidget *scale_image_new()
{
  return GTK_WIDGET(g_object_new(scale_image_get_type(), NULL));
}

void scale_image_set_pixbuf ( GtkWidget *self, GdkPixbuf *pb )
{
  ScaleImagePrivate *priv;

  g_return_if_fail(IS_SCALE_IMAGE(self));
  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  if(priv->pixbuf == pb)
    return;

  scale_image_clear(self);
  priv->pixbuf = gdk_pixbuf_copy(pb);
  priv->ftype = SI_BUFF;
  gtk_widget_queue_draw(self);
}

static void scale_image_check_appinfo ( GtkWidget *self, GtkIconTheme *theme,
    const gchar *icon )
{
  ScaleImagePrivate *priv;
  GDesktopAppInfo *app;
  GtkIconInfo *info;
  gchar *file;

  priv = scale_image_get_instance_private(SCALE_IMAGE(self));
  if(priv->ftype != SI_NONE)
    return;

  app = g_desktop_app_info_new(icon);
  if(!app)
    return;

  if( !g_desktop_app_info_get_nodisplay(app) )
  {
    file = g_desktop_app_info_get_string(app,"Icon");
    if( (info = gtk_icon_theme_lookup_icon(theme,file,10,0)) )
    {
      g_object_unref(G_OBJECT(info));
      priv->fname = g_strdup(file);
      priv->ftype = SI_ICON;
    }
    g_free(file);
  }

  g_object_unref(G_OBJECT(app));
}

static gboolean scale_image_check_icon_1 ( GtkWidget *self, const gchar *icon )
{
  ScaleImagePrivate *priv;
  GtkIconTheme *theme;
  GtkIconInfo *info;
  gint i,j;
  gchar *temp;
  gchar ***desktop;

  theme = gtk_icon_theme_get_default();
  if(!theme)
    return FALSE;

  priv = scale_image_get_instance_private(SCALE_IMAGE(self));

  if( (info = gtk_icon_theme_lookup_icon(theme,icon,10,0)) )
  {
    g_object_unref(G_OBJECT(info));
    priv->fname = g_strdup(icon);
    priv->ftype = SI_ICON;
    return TRUE;
  }

  desktop = g_desktop_app_info_search(icon);
  for(j=0;desktop[j];j++)
  {
    for(i=0;desktop[j][i];i++)
      scale_image_check_appinfo(self, theme, desktop[j][i]);
    g_strfreev(desktop[j]);
  }
  g_free(desktop);
  
  if(priv->ftype!=SI_NONE)
    return TRUE;
  temp = g_strconcat(icon,".desktop",NULL);
  scale_image_check_appinfo(self, theme, temp);
  g_free(temp);

  return (priv->ftype!=SI_NONE);
}

static gboolean scale_image_check_icon ( GtkWidget *self, const gchar *icon )
{
  ScaleImagePrivate *priv;
  gchar *temp;

  priv = scale_image_get_instance_private( SCALE_IMAGE(self));

  if(scale_image_check_icon_1(self, icon))
    return TRUE;

  temp = g_ascii_strdown(icon, -1);
  scale_image_check_icon_1(self, temp);
  g_free(temp);
  if(priv->ftype == SI_ICON)
    return TRUE;

  return FALSE;
}

static gboolean scale_image_check_icon_symbolic ( GtkWidget *self,
    gchar *icon )
{
  ScaleImagePrivate *priv;
  gchar *temp;

  priv = scale_image_get_instance_private( SCALE_IMAGE(self));
  temp = g_strconcat(icon, "-symbolic", NULL);
  scale_image_check_icon(self, temp);
  g_free(temp);

  if(priv->ftype != SI_ICON)
    return FALSE;

  priv->symbolic = TRUE;
  return TRUE;
}

gboolean scale_image_set_image ( GtkWidget *self, const gchar *image,
    gchar *extra )
{
  static gchar *exts[4] = {"", ".svg", ".png", ".xpm"};
  ScaleImagePrivate *priv;
  gboolean symbolic_suffix;
  GdkPixbuf *buf;
  gint i;
  gchar *temp,*test;

  g_return_val_if_fail(IS_SCALE_IMAGE(self), FALSE);
  priv = scale_image_get_instance_private( SCALE_IMAGE(self));

  if(!image)
    return FALSE;

  if( !g_strcmp0(priv->file, image) && !g_strcmp0(priv->extra, extra) )
    return (priv->ftype != SI_NONE);

  scale_image_clear(self);
  priv->file = g_strdup(image);
  priv->extra = g_strdup(extra);
  priv->symbolic = FALSE;
  gtk_widget_queue_draw(self);

  if(!g_ascii_strncasecmp(priv->file,"<?xml",5))
  {
    priv->ftype = SI_DATA;
    return TRUE;
  }

  symbolic_suffix = (strlen(image) >= 9) &&
    (!g_ascii_strcasecmp(image+strlen(image)-9, "-symbolic"));
  gtk_widget_style_get(self, "symbolic", &priv->symbolic_pref, NULL);

  if(priv->symbolic_pref && !symbolic_suffix)
    if(scale_image_check_icon_symbolic(self, priv->file))
      return TRUE;
  if(scale_image_check_icon(self, priv->file))
    return TRUE;
  if(!priv->symbolic_pref && !symbolic_suffix)
    if(scale_image_check_icon_symbolic(self, priv->file))
      return TRUE;

  for(i=0; i<8; i++)
  {
    test = g_strconcat(priv->file, (i%2!=priv->symbolic_pref)?"-symbolic":"",
        exts[i/2], NULL);
    temp = get_xdg_config_file(test, extra);
    g_free(test);
    if(temp)
    {
      buf = gdk_pixbuf_new_from_file_at_scale(temp, 10, 10, TRUE, NULL);
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

  return priv->ftype != SI_NONE;
}
