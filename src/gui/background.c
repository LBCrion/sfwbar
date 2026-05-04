/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include <gdk/gdkwayland.h>
#include "wayland.h"
#include "gui/background.h"
#include "gui/basewidget.h"

#define EXT_BACKGROUND_EFFECT_VERSION 1

static struct ext_background_effect_manager_v1 *background_effect_manager;
GType background_effect_enum;

GEnumValue background_effect_enum_defs[] = {
  { BACKGROUND_EFFECT_NONE, "none", "none" },
  { BACKGROUND_EFFECT_BLUR, "blur", "blur" },
  { 0, NULL, NULL },
};

static void background_effect_region_calc ( GtkWidget *widget,
    struct wl_region *region )
{
  GtkStyleContext *style;
  GtkStateFlags flags;
  GtkBorder margin;
  GtkAllocation alloc;
  gint x, y, w, h, r, dx, dy, t;

  style = gtk_widget_get_style_context(widget);
  flags = gtk_style_context_get_state(style);
  gtk_widget_get_allocation(widget, &alloc);
  gtk_style_context_get(style, flags,
      GTK_STYLE_PROPERTY_BORDER_RADIUS, &r, NULL);

  if(GTK_IS_MENU(widget))
    margin.left = margin.right = margin.top = margin.bottom = 0;
  else
    gtk_style_context_get_margin(style, flags, &margin);

  gtk_widget_translate_coordinates(widget, gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW),
      margin.left + r, margin.right + r, &x, &y);
  w = alloc.width - margin.left - margin.right - 2*r;
  h = alloc.height - margin.top - margin.bottom - 2*r;
  wl_region_add(region, x - r, y, w + r*2, h);

  t = r  / 16;
  dx = r;
  dy = 0;
  do {
    wl_region_add(region, x - dx, y - dy, w + 2*dx, 1);
    wl_region_add(region, x - dx, y + h - 1 + dy, w + 2*dx, 1);
    dy++;
    t += dy;

    if(t-dx >= 0)
    {
      wl_region_add(region, x - dy + 1, y - dx, w + 2*dy - 2, 1);
      wl_region_add(region, x - dy + 1, y + h - 1 + dx, w + 2*dy - 2, 1);
      t -= dx;
      dx--;
    }
  } while(dx>=dy);
}

void background_effect_apply ( GtkWidget *widget )
{
  background_effect_priv_t *priv;
  struct wl_compositor *compositor;
  struct wl_surface *surface;
  struct wl_region *region;
  GdkWindow *gwin;

  if(!background_effect_manager)
    return;

  if( !(priv = g_object_get_data(G_OBJECT(widget), "background_effect_priv")) )
    return;

  if(!priv->invalid)
    return;

  if(IS_BASE_WIDGET(widget))
    widget = base_widget_get_child(widget);
  if(!(gwin = gtk_widget_get_window(widget)) ||
      !gdk_window_is_visible(gwin) ||
      !(surface = gdk_wayland_window_get_wl_surface(gwin)) )
    return;

  if(!priv->effect)
    priv->effect = ext_background_effect_manager_v1_get_background_effect(
        background_effect_manager, surface);

  compositor = gdk_wayland_display_get_wl_compositor(
      gdk_display_get_default());
  region = wl_compositor_create_region(compositor);

  if(priv->type == BACKGROUND_EFFECT_BLUR)
    background_effect_region_calc(widget, region);

  ext_background_effect_surface_v1_set_blur_region(priv->effect, region);
  wl_region_destroy(region);
}

static void background_effect_size_allocate ( GtkWidget *self,
    GdkRectangle *allocation )
{
  background_effect_apply(self);
}

static void background_effect_unmap ( GtkWidget *self, gpointer d )
{
  background_effect_priv_t *priv;

  if( !(priv = g_object_get_data(G_OBJECT(self), "background_effect_priv")) )
    return;

  g_clear_pointer(&priv->effect, ext_background_effect_surface_v1_destroy);
}

static void background_effect_map ( GtkWidget *self, gpointer d )
{
  background_effect_apply(self);
}

void background_effect_init ( void )
{
  if(background_effect_manager)
    return;

  background_effect_manager = wayland_iface_register(
      ext_background_effect_manager_v1_interface.name,
      EXT_BACKGROUND_EFFECT_VERSION, EXT_BACKGROUND_EFFECT_VERSION,
      &ext_background_effect_manager_v1_interface);

  background_effect_enum = g_enum_register_static("background_effect",
      background_effect_enum_defs);
}

background_effect_t background_effect_get ( GtkWidget *widget )
{
  background_effect_priv_t *priv;

  if( !(priv = g_object_get_data(G_OBJECT(widget), "background_effect_priv")) )
    return BACKGROUND_EFFECT_NONE;

  return priv->type;
}

void background_effect_set ( GtkWidget *widget, background_effect_t type )
{
  background_effect_priv_t *priv;

  if(!background_effect_manager)
    return;

  g_return_if_fail(GTK_IS_WIDGET(widget));
  if( !(priv = g_object_get_data(G_OBJECT(widget), "background_effect_priv")) )
  {
    priv = g_malloc0(sizeof(background_effect_priv_t));
    priv->invalid = TRUE;
    g_object_set_data(G_OBJECT(widget), "background_effect_priv", priv);
  }

  if(!priv->invalid)
    return;

  priv->type = type;
  background_effect_apply(widget);
  g_signal_connect(G_OBJECT(widget), "map-event",
      G_CALLBACK(background_effect_map), NULL);
  g_signal_connect(G_OBJECT(widget), "size-allocate",
      G_CALLBACK(background_effect_size_allocate), NULL);
  g_signal_connect(G_OBJECT(widget), "unmap",
      G_CALLBACK(background_effect_unmap), NULL);
}
